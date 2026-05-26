#ifndef SERVER_H
#define SERVER_H

/* Inicializa el servidor: crea el socket con getaddrinfo,
 * hace bind y listen.
 * Devuelve el file descriptor del socket servidor, o -1 en error. */
int server_init(int port);

#endif
