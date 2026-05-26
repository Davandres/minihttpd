#ifndef SERVER_H
#define SERVER_H

#include <sys/epoll.h>

#define MAX_EVENTS 100
#define PORT 8080
#define BUFFER_SIZE 16384
#define MAX_CLIENTS 1000

// Estructura para manejar conexiones de clientes
typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    int buffer_length;
    int keep_alive;
} client_info;

// Estructura del servidor
typedef struct {
    int server_fd;
    int epoll_fd;
    struct epoll_event events[MAX_EVENTS];
    client_info clients[MAX_CLIENTS];
    char root_dir[256];
} server;

// Funciones del servidor
int init_server(server *srv, int port, const char *root_dir);
void run_server(server *srv);
void cleanup_server(server *srv);

#endif
