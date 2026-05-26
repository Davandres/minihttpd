#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

// Límites de seguridad — evitan buffer overflows y ataques DoS
#define MAX_URI_LEN       2048
#define MAX_HEADER_LEN    8192
#define MAX_METHOD_LEN    16

// Representa una solicitud HTTP parseada
typedef struct {
    char method[MAX_METHOD_LEN];  // "GET", "POST", etc.
    char uri[MAX_URI_LEN];        // "/index.html"
    char host[256];               // valor del header Host:
    char connection[32];          // "keep-alive" o "close"
} http_request_t;

// Códigos de estado HTTP que soportamos
typedef enum {
    HTTP_200 = 200,
    HTTP_400 = 400,
    HTTP_403 = 403,
    HTTP_404 = 404,
    HTTP_405 = 405,
    HTTP_500 = 500
} http_status_t;

// Parsea el texto crudo de una solicitud HTTP en una estructura http_request_t
// Devuelve HTTP_200 si es válida, o el código de error correspondiente
http_status_t http_parse_request(const char *raw, http_request_t *req);

// Construye y envía la respuesta HTTP completa al cliente (fd = file descriptor)
// www_root es la raíz del directorio de archivos estáticos
void http_send_response(int fd, const http_request_t *req,
                        const char *www_root);

// Envía una respuesta de error simple (sin cuerpo de archivo)
void http_send_error(int fd, http_status_t status);

#endif
