#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define MAX_REQUEST_SIZE 8192
#define MAX_HEADERS 50
#define MAX_HEADER_LENGTH 1024
#define MAX_URI_LENGTH 2048
#define MAX_METHOD_LENGTH 16

// Estructura para una solicitud HTTP
typedef struct {
    char method[MAX_METHOD_LENGTH];
    char uri[MAX_URI_LENGTH];
    char version[16];
    char headers[MAX_HEADERS][MAX_HEADER_LENGTH];
    int header_count;
    char body[MAX_REQUEST_SIZE];
    int body_length;
} http_request;

// Estructura para una respuesta HTTP
typedef struct {
    int status_code;
    char status_text[64];
    char content_type[128];
    char *body;
    size_t body_length;
    int keep_alive;
} http_response;

// Funciones de parsing y generación HTTP
int parse_http_request(const char *raw_request, http_request *req);
void generate_http_response(http_request *req, http_response *res, const char *root_dir);
void free_http_response(http_response *res);

#endif
