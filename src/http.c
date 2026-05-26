#include "http.h"
#include "files.h"
#include "mime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>         /* write(), close()   */
#include <limits.h>         /* PATH_MAX            */
#include <sys/stat.h>       /* stat(), S_ISDIR()   */
#include <sys/sendfile.h>   /* sendfile()          */

/* ─────────────────────────────────────────────────────────────────
 * HELPERS INTERNOS
 * ───────────────────────────────────────────────────────────────── */

/* Escribe todos los bytes en fd (write puede enviar menos que size) */
static void write_all(int fd, const char *buf, size_t size) {
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = write(fd, buf + sent, size - sent);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

/* Traduce un código de estado a su texto */
static const char *status_text(http_status_t s) {
    switch (s) {
        case HTTP_200: return "OK";
        case HTTP_400: return "Bad Request";
        case HTTP_403: return "Forbidden";
        case HTTP_404: return "Not Found";
        case HTTP_405: return "Method Not Allowed";
        case HTTP_500: return "Internal Server Error";
        default:       return "Unknown";
    }
}

/* ─────────────────────────────────────────────────────────────────
 * PARSING
 * ───────────────────────────────────────────────────────────────── */

http_status_t http_parse_request(const char *raw, http_request_t *req) {
    memset(req, 0, sizeof(*req));

    /* --- 1. Validar tamaño total del buffer --- */
    if (strlen(raw) > MAX_HEADER_LEN) return HTTP_400;

    /* Trabajamos sobre una copia para no modificar el original.
     *    snprintf SIEMPRE escribe \0 en la posición n-1 como máximo. */
    char buf[MAX_HEADER_LEN + 1];
    snprintf(buf, sizeof(buf), "%s", raw);

    /* --- 2. Aislar la Request Line: "METHOD URI HTTP/1.x\r\n" --- */
    char *line_end = strstr(buf, "\r\n");
    if (!line_end) return HTTP_400;
    *line_end = '\0';   /* terminamos la primera línea aquí */

    /* Extraemos los tres tokens de la request line.
     * Los anchos en sscanf previenen buffer overflow:
     *   %15s  → máximo 15 chars + \0 → cabe en method[MAX_METHOD_LEN=16]
     *   %2047s → máximo 2047 chars + \0 → cabe en uri[MAX_URI_LEN=2048] */
    char method[MAX_METHOD_LEN];
    char uri[MAX_URI_LEN];
    char version[16];

    if (sscanf(buf, "%15s %2047s %15s", method, uri, version) != 3)
        return HTTP_400;

    /* --- 3. Validar método --- */
    if (strcmp(method, "GET") != 0) return HTTP_405;

    /* --- 4. Validar URI --- */
    if (strlen(uri) >= MAX_URI_LEN) return HTTP_400;
    if (uri[0] != '/')              return HTTP_400;

    snprintf(req->method, sizeof(req->method), "%s", method);
    snprintf(req->uri,    sizeof(req->uri),    "%s", uri);

    /* --- 5. Parsear headers línea por línea --- */
    char *ptr = line_end + 2;   /* +2 salta el \r\n de la request line */

    while (*ptr && *ptr != '\r') {  /* línea vacía (\r\n) = fin de headers */
        char *end = strstr(ptr, "\r\n");
        if (!end) break;
        *end = '\0';    /* aislamos el header actual */

        /* Separamos "Nombre: valor" */
        char *colon = strchr(ptr, ':');
        if (colon) {
            *colon     = '\0';
            char *name  = ptr;
            char *value = colon + 1;

            /* Saltar espacios al inicio del valor */
            while (*value == ' ') value++;

            /* snprintf en lugar de strncpy para cada header */
            if (strcasecmp(name, "Host") == 0)
                snprintf(req->host, sizeof(req->host), "%s", value);
            else if (strcasecmp(name, "Connection") == 0)
                snprintf(req->connection, sizeof(req->connection), "%s", value);
        }

        ptr = end + 2;  /* avanzamos al siguiente header */
    }

    return HTTP_200;
}

/* ─────────────────────────────────────────────────────────────────
 * RESPUESTA DE ERROR
 * ───────────────────────────────────────────────────────────────── */

void http_send_error(int fd, http_status_t status) {
    const char *text = status_text(status);

    /* snprintf (nunca sprintf) — tamaño fijo, sin overflow posible */
    char response[512];
    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>%d %s</h1></body></html>",
        status, text, status, text);

    write_all(fd, response, strlen(response));
}

/* ─────────────────────────────────────────────────────────────────
 * RESPUESTA EXITOSA
 * ───────────────────────────────────────────────────────────────── */

void http_send_response(int fd, const http_request_t *req,
                        const char *www_root) {

    /* ── Seguridad: construir la ruta completa ───────────────────── */
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s", www_root, req->uri);

    /* realpath() resuelve '..' y symlinks → ruta absoluta real.
     * Esto neutraliza ataques de Directory Traversal como:
     *   GET /../../etc/passwd  →  realpath devuelve /etc/passwd */
    char real_path[PATH_MAX];
    if (!realpath(path, real_path)) {
        http_send_error(fd, HTTP_404);
        return;
    }

    /* ── Seguridad: verificar que la ruta esté dentro de www_root ── */
    char real_root[PATH_MAX];
    if (!realpath(www_root, real_root)) {
        http_send_error(fd, HTTP_500);
        return;
    }

    /* Si real_path no empieza con real_root → Directory Traversal → 403 */
    if (strncmp(real_path, real_root, strlen(real_root)) != 0) {
        http_send_error(fd, HTTP_403);
        return;
    }

    /* ── Si la URI apunta a un directorio, servir index.html ─────── */
    struct stat st;
    if (stat(real_path, &st) != 0) {
        http_send_error(fd, HTTP_404);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        /* strncat con espacio restante disponible — sin overflow */
        strncat(real_path, "/index.html",
                PATH_MAX - strlen(real_path) - 1);

        /* Re-verificar que el index.html exista */
        if (stat(real_path, &st) != 0) {
            http_send_error(fd, HTTP_404);
            return;
        }
    }

    /* ── Abrir archivo con file_open() y obtener tamaño ─────────── */
    size_t file_size = 0;
    int file_fd = file_open(real_path, &file_size);
    if (file_fd < 0) {
        http_send_error(fd, HTTP_404);
        return;
    }

    /* ── Determinar Content-Type según extensión ─────────────────── */
    const char *mime = mime_get_type(real_path);

    /* ── Conexión persistente o no ───────────────────────────────── */
    /* HTTP/1.1 es keep-alive por defecto; cerramos solo si el cliente
     * envió "Connection: close" explícitamente */
    int keep_alive = (strcasecmp(req->connection, "close") != 0);

    /* ── Construir y enviar headers HTTP ─────────────────────────── */
    /* snprintf — límite estricto, garantiza \0, sin overflow */
    char headers[512];
    snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        mime,
        file_size,
        keep_alive ? "keep-alive" : "close");

    write_all(fd, headers, strlen(headers));

    /* ── Enviar cuerpo con sendfile() (zero-copy) ────────────────── */
    /* sendfile(out_fd, in_fd, &offset, count):
     *   - Transfiere bytes directamente de file_fd al socket fd
     *   - Sin pasar por espacio de usuario (zero-copy)
     *   - offset es actualizado automáticamente en cada llamada
     *   - Puede enviar menos bytes que count, necesitamos el loop */
    off_t  offset    = 0;
    size_t remaining = file_size;

    while (remaining > 0) {
        ssize_t sent = sendfile(fd, file_fd, &offset, remaining);
        if (sent <= 0) break;   /* error o cliente cerró la conexión */
        remaining -= (size_t)sent;
    }

    /* Cerramos el fd del archivo — el socket (fd) lo gestiona server.c */
    close(file_fd);
}
