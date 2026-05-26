#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

volatile sig_atomic_t keep_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        keep_running = 0;
    }
}

int main(int argc, char *argv[]) {
    // Configurar manejador de señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configuración por defecto
    int port = 8080;
    const char *root_dir = "www";
    
    // Parsear argumentos de línea de comandos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            root_dir = argv[++i];
        }
    }
    
    // Inicializar servidor
    server srv;
    if (init_server(&srv, port, root_dir) != 0) {
        fprintf(stderr, "Error al inicializar el servidor\n");
        return EXIT_FAILURE;
    }
    
    // Ejecutar servidor
    printf("MiniHTTPd - Servidor HTTP/1.1 básico\n");
    printf("Presiona Ctrl+C para detener\n");
    
    while (keep_running) {
        // En lugar de run_server que tiene bucle infinito,
        // usamos epoll_wait con timeout para poder verificar keep_running
        int n = epoll_wait(srv.epoll_fd, srv.events, MAX_EVENTS, 1000);
        
        for (int i = 0; i < n; i++) {
            if (srv.events[i].data.fd == srv.server_fd) {
                // Nueva conexión
                handle_new_connection(&srv);
            } else {
                // Datos de cliente existente
                handle_client_data(&srv, srv.events[i].data.fd);
            }
        }
    }
    
    // Limpiar y salir
    printf("\nDeteniendo servidor...\n");
    cleanup_server(&srv);
    
    return EXIT_SUCCESS;
}

// Necesitamos declarar las funciones que usamos de server.c
extern void handle_new_connection(server *srv);
extern void handle_client_data(server *srv, int client_fd);
