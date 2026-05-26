#include "http.h"
#include "files.h"
#include "mime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

// Función auxiliar para obtener la fecha actual en formato HTTP
static void get_http_date(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    strftime(buffer, size, "%a, %d %b %Y %H:%M:%S GMT", tm);
}

// Función auxiliar para buscar un header específico
static const char* find_header(http_request *req, const char *name) {
    for (int i = 0; i < req->header_count; i++) {
        const char *header = req->headers[i];
        size_t name_len = strlen(name);
        
        if (strncasecmp(header, name, name_len) == 0 && header[name_len] == ':') {
            // Saltar el nombre, los dos puntos y los espacios
            const char *value = header + name_len + 1;
            while (*value == ' ') value++;
            return value;
        }
    }
    return NULL;
}

int parse_http_request(const char *raw_request, http_request *req) {
    if (!raw_request || !req) return -1;
    
    // Inicializar la estructura
    memset(req, 0, sizeof(http_request));
    
    // Verificar tamaño máximo
    if (strlen(raw_request) > MAX_REQUEST_SIZE) {
        return -1;  // 400 Bad Request
    }
    
    // Copiar la solicitud para trabajar con ella
    char request_copy[MAX_REQUEST_SIZE];
    strncpy(request_copy, raw_request, MAX_REQUEST_SIZE - 1);
    request_copy[MAX_REQUEST_SIZE - 1] = '\0';
    
    // Separar líneas
    char *saveptr1, *line;
    int line_number = 0;
    
    // Parsear la línea de solicitud
    line = strtok_r(request_copy, "\r\n", &saveptr1);
    if (!line) return -1;
    
    // Parsear método, URI y versión
    char method[16], uri[MAX_URI_LENGTH], version[16];
    if (sscanf(line, "%15s %2047s %15s", method, uri, version) != 3) {
        return -1;
    }
    
    // Verificar longitud de la URI
    if (strlen(uri) > MAX_URI_LENGTH) {
        return -1;
    }
    
    // Verificar método
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        strcpy(req->method, method);  // Guardar para manejar error 405
        return -2;  // Method Not Allowed
    }
    
    // Guardar datos de la solicitud
    strncpy(req->method, method, MAX_METHOD_LENGTH - 1);
    strncpy(req->uri, uri, MAX_URI_LENGTH - 1);
    strncpy(req->version, version, sizeof(req->version) - 1);
    
    // Parsear headers
    while ((line = strtok_r(NULL, "\r\n", &saveptr1)) != NULL) {
        if (strlen(line) == 0) break;  // Línea vacía = fin de headers
        
        if (req->header_count >= MAX_HEADERS) {
            return -1;  // Demasiados headers
        }
        
        if (strlen(line) > MAX_HEADER_LENGTH) {
            return -1;  // Header demasiado largo
        }
        
        strncpy(req->headers[req->header_count], line, MAX_HEADER_LENGTH - 1);
        req->headers[req->header_count][MAX_HEADER_LENGTH - 1] = '\0';
        req->header_count++;
    }
    
    return 0;
}

void generate_http_response(http_request *req, http_response *res, const char *root_dir) {
    if (!req || !res) return;
    
    // Inicializar respuesta
    memset(res, 0, sizeof(http_response));
    res->keep_alive = 0;
    
    // Verificar método
    if (strcmp(req->method, "GET") != 0 && strcmp(req->method, "HEAD") != 0) {
        res->status_code = 405;
        strcpy(res->status_text, "Method Not Allowed");
        strcpy(res->content_type, "text/html");
        
        const char *body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        res->body = strdup(body);
        res->body_length = strlen(body);
        return;
    }
    
    // Verificar seguridad del path
    if (!is_safe_path(root_dir, req->uri)) {
        res->status_code = 403;
        strcpy(res->status_text, "Forbidden");
        strcpy(res->content_type, "text/html");
        
        const char *body = "<html><body><h1>403 Forbidden</h1></body></html>";
        res->body = strdup(body);
        res->body_length = strlen(body);
        return;
    }
    
    // Construir ruta completa
    char full_path[2048];
    if (strcmp(req->uri, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", root_dir);
    } else {
        // Eliminar el / inicial si existe
        const char *uri = req->uri;
        if (uri[0] == '/') uri++;
        snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, uri);
    }
    
    // Verificar si el archivo existe
    if (!file_exists(full_path)) {
        res->status_code = 404;
        strcpy(res->status_text, "Not Found");
        strcpy(res->content_type, "text/html");
        
        const char *body = "<html><body><h1>404 Not Found</h1></body></html>";
        res->body = strdup(body);
        res->body_length = strlen(body);
        return;
    }
    
    // Leer el archivo
    file_content *fc = read_file(full_path);
    if (!fc) {
        res->status_code = 500;
        strcpy(res->status_text, "Internal Server Error");
        strcpy(res->content_type, "text/html");
        
        const char *body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
        res->body = strdup(body);
        res->body_length = strlen(body);
        return;
    }
    
    // Configurar respuesta exitosa
    res->status_code = 200;
    strcpy(res->status_text, "OK");
    
    // Establecer tipo MIME
    const char *mime = get_mime_type(full_path);
    strcpy(res->content_type, mime);
    
    // Establecer cuerpo de la respuesta
    res->body = fc->data;
    res->body_length = fc->size;
    
    // Verificar si debe mantener la conexión viva
    const char *connection = find_header(req, "Connection");
    if (connection && strcasecmp(connection, "keep-alive") == 0) {
        res->keep_alive = 1;
    }
    
    // Liberar la estructura file_content (pero no los datos)
    free(fc);
}

void free_http_response(http_response *res) {
    if (res && res->body) {
        free(res->body);
        res->body = NULL;
    }
}
