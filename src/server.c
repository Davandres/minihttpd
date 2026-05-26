#include "server.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>          /* getaddrinfo(), freeaddrinfo() */

/* server_init: crea el socket, hace bind y listen. */
int server_init(int port) {

    /* getaddrinfo necesita el puerto como string */
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    /* hints: pedimos TCP, IPv4 o IPv6, en todas las interfaces */
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;    /* IPv4 o IPv6 automático   */
    hints.ai_socktype = SOCK_STREAM;  /* TCP                      */
    hints.ai_flags    = AI_PASSIVE;   /* escucha en 0.0.0.0       */

    struct addrinfo *res;
    int rc = getaddrinfo(NULL, port_str, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    /* Iteramos la lista hasta que socket() + bind() tengan éxito */
    int fd = -1;
    struct addrinfo *p;

    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        /* Evita "Address already in use" al reiniciar el servidor */
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0)
            break;      /* bind exitoso — salimos con fd válido */

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);  /* siempre liberar, incluso si hubo error */

    if (fd < 0) {
        perror("No se pudo hacer bind en ninguna dirección");
        return -1;
    }

    if (listen(fd, 128) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}
