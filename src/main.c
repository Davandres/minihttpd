#include "server.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define MAX_EVENTS  64
#define RECV_BUF   8192

/* ── Helper: pone un fd en modo no-bloqueante ───────────────────────
 * Necesario para epoll edge-triggered (EPOLLET): si un recv() no
 * tiene datos disponibles debe retornar EAGAIN en lugar de bloquear
 * el proceso entero. */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <puerto> <directorio_www>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Puerto inválido: %s\n", argv[1]);
        return 1;
    }

    const char *www_root = argv[2];

    /* ── Inicializar socket servidor (server.c) ─────────────────── */
    int server_fd = server_init(port);
    if (server_fd < 0) {
        fprintf(stderr, "No se pudo iniciar el servidor\n");
        return 1;
    }

    printf("MiniHTTPd corriendo en http://localhost:%d\n", port);
    printf("Sirviendo archivos desde: %s\n", www_root);

    /* ── Crear instancia epoll ───────────────────────────────────── */
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    /* Registrar el socket servidor en epoll.
     * EPOLLIN = "avísame cuando haya una nueva conexión entrante" */
    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    struct epoll_event events[MAX_EVENTS];

    /* ── Bucle de eventos ───────────────────────────────────────────
     * epoll_wait bloquea hasta que algún fd registrado tenga eventos.
     * Cuando retorna, 'n' indica cuántos fds están listos. */
    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n < 0) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                /* ── Nueva conexión entrante ────────────────────────
                 * sockaddr_storage soporta tanto IPv4 como IPv6
                 * (gracias a que usamos AF_UNSPEC en server_init) */
                struct sockaddr_storage client_addr;
                socklen_t addrlen = sizeof(client_addr);

                int client_fd = accept(server_fd,
                                       (struct sockaddr *)&client_addr,
                                       &addrlen);
                if (client_fd < 0) continue;

                set_nonblocking(client_fd);

                /* Registrar el cliente en epoll.
                 * EPOLLET (Edge Triggered): notifica UNA sola vez cuando
                 * llegan datos, no repetidamente mientras haya datos.
                 * Requiere leer hasta EAGAIN para no perder eventos. */
                struct epoll_event cev;
                cev.events  = EPOLLIN | EPOLLET;
                cev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);

            } else {
                /* ── Datos listos de un cliente ─────────────────────
                 * Leemos la solicitud HTTP completa en el buffer */
                char buf[RECV_BUF];
                ssize_t len = recv(fd, buf, sizeof(buf) - 1, 0);

                if (len <= 0) {
                    /* len == 0 → cliente cerró la conexión
                     * len <  0 → error (ej: ECONNRESET)
                     * En ambos casos eliminamos el fd de epoll y cerramos */
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                buf[len] = '\0';

                /* Parsear la solicitud (http.c) */
                http_request_t req;
                http_status_t status = http_parse_request(buf, &req);

                /* Generar la respuesta (http.c) */
                if (status != HTTP_200)
                    http_send_error(fd, status);
                else
                    http_send_response(fd, &req, www_root);

                /* Conexiones persistentes (keep-alive por defecto en HTTP/1.1)
                 * Solo cerramos si el cliente envió "Connection: close" */
                if (strcasecmp(req.connection, "close") == 0) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
        }
    }

    close(epfd);
    close(server_fd);
    return 0;
}
