#include "server.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// Función para hacer un socket no bloqueante
static int make_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Función para manejar una nueva conexión
static void handle_new_connection(server *srv) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(srv->server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        perror("accept");
        return;
    }
    
    // Hacer el socket no bloqueante
    if (make_non_blocking(client_fd) == -1) {
        close(client_fd);
        return;
    }
    
    // Configurar TCP_NODELAY para mejor rendimiento
    int opt = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    // Agregar a epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered
    ev.data.fd = client_fd;
    
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl");
        close(client_fd);
        return;
    }
    
    // Guardar información del cliente
    if (client_fd < MAX_CLIENTS) {
        srv->clients[client_fd].fd = client_fd;
        srv->clients[client_fd].buffer_length = 0;
        srv->clients[client_fd].keep_alive = 0;
    }
    
    printf("Nueva conexión: %d\n", client_fd);
}

// Función para manejar datos del cliente
static void handle_client_data(server *srv, int client_fd) {
    client_info *client = &srv->clients[client_fd];
    char buf[4096];
    
    while (1) {
        ssize_t count = recv(client_fd, buf, sizeof(buf), 0);
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No hay más datos por ahora
                break;
            }
            // Error real
            close(client_fd);
            epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            printf("Cliente desconectado (error): %d\n", client_fd);
            return;
        } else if (count == 0) {
            // Conexión cerrada
            close(client_fd);
            epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            printf("Cliente desconectado: %d\n", client_fd);
            return;
        }
        
        // Acumular datos en el buffer
        if (client->buffer_length + count < BUFFER_SIZE) {
            memcpy(client->buffer + client->buffer_length, buf, count);
            client->buffer_length += count;
            client->buffer[client->buffer_length] = '\0';
        }
    }
    
    // Verificar si tenemos una solicitud completa
    char *end_of_request = strstr(client->buffer, "\r\n\r\n");
    if (end_of_request) {
        // Tenemos una solicitud completa
        http_request req;
        http_response res;
        
        int parse_result = parse_http_request(client->buffer, &req);
        
        if (parse_result == 0) {
            generate_http_response(&req, &res, srv->root_dir);
        } else if (parse_result == -2) {
            // Method Not Allowed
            memset(&res, 0, sizeof(res));
            res.status_code = 405;
            strcpy(res.status_text, "Method Not Allowed");
            strcpy(res.content_type, "text/html");
            const char *body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
            res.body = strdup(body);
            res.body_length = strlen(body);
        } else {
            // Bad Request
            memset(&res, 0, sizeof(res));
            res.status_code = 400;
            strcpy(res.status_text, "Bad Request");
            strcpy(res.content_type, "text/html");
            const char *body = "<html><body><h1>400 Bad Request</h1></body></html>";
            res.body = strdup(body);
            res.body_length = strlen(body);
        }
        
        // Construir y enviar respuesta
        char response_header[4096];
        char date[128];
        get_http_date(date, sizeof(date));
        
        int header_len = snprintf(response_header, sizeof(response_header),
            "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Server: MiniHTTPd/1.0\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: %s\r\n"
            "\r\n",
            res.status_code, res.status_text,
            date,
            res.content_type,
            res.body_length,
            res.keep_alive ? "keep-alive" : "close"
        );
        
        // Enviar header
        send(client_fd, response_header, header_len, 0);
        
        // Enviar body (solo si no es HEAD y hay body)
        if (strcmp(req.method, "HEAD") != 0 && res.body && res.body_length > 0) {
            send(client_fd, res.body, res.body_length, 0);
        }
        
        // Liberar recursos de la respuesta
        free_http_response(&res);
        
        // Limpiar buffer del cliente
        client->buffer_length = 0;
        memset(client->buffer, 0, BUFFER_SIZE);
        
        // Manejar conexión persistente
        if (!res.keep_alive) {
            close(client_fd);
            epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            printf("Conexión cerrada: %d\n", client_fd);
        }
    }
}

int init_server(server *srv, int port, const char *root_dir) {
    if (!srv) return -1;
    
    // Inicializar estructura
    memset(srv, 0, sizeof(server));
    strncpy(srv->root_dir, root_dir, sizeof(srv->root_dir) - 1);
    
    // Crear socket
    srv->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->server_fd == -1) {
        perror("socket");
        return -1;
    }
    
    // Configurar opciones del socket
    int opt = 1;
    if (setsockopt(srv->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        return -1;
    }
    
    // Hacer el socket no bloqueante
    if (make_non_blocking(srv->server_fd) == -1) {
        perror("fcntl");
        return -1;
    }
    
    // Configurar dirección del servidor
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    // Bind
    if (bind(srv->server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return -1;
    }
    
    // Listen
    if (listen(srv->server_fd, SOMAXCONN) == -1) {
        perror("listen");
        return -1;
    }
    
    // Crear epoll
    srv->epoll_fd = epoll_create1(0);
    if (srv->epoll_fd == -1) {
        perror("epoll_create1");
        return -1;
    }
    
    // Agregar socket del servidor a epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = srv->server_fd;
    
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->server_fd, &ev) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    
    // Inicializar array de clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
        srv->clients[i].fd = -1;
    }
    
    printf("Servidor inicializado en puerto %d\n", port);
    printf("Sirviendo archivos desde: %s\n", root_dir);
    
    return 0;
}

void run_server(server *srv) {
    if (!srv) return;
    
    printf("Servidor en ejecución...\n");
    
    while (1) {
        int n = epoll_wait(srv->epoll_fd, srv->events, MAX_EVENTS, -1);
        
        for (int i = 0; i < n; i++) {
            if (srv->events[i].data.fd == srv->server_fd) {
                // Nueva conexión
                handle_new_connection(srv);
            } else {
                // Datos de cliente existente
                handle_client_data(srv, srv->events[i].data.fd);
            }
        }
    }
}

void cleanup_server(server *srv) {
    if (!srv) return;
    
    close(srv->server_fd);
    close(srv->epoll_fd);
    
    printf("Servidor detenido\n");
}

// Necesitamos la función get_http_date también en server.c
static void get_http_date(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    strftime(buffer, size, "%a, %d %b %Y %H:%M:%S GMT", tm);
}
