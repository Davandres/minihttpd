#include "mime.h"
#include <string.h>   // strrchr, strcmp

// Tabla de extensiones conocidas
// Usamos un array de structs — simple, eficiente para pocas entradas
static const struct {
    const char *ext;
    const char *type;
} mime_table[] = {
    {".html", "text/html"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".ico",  "image/x-icon"},
    {".txt",  "text/plain"},
};

// Número de entradas en la tabla (calculado en tiempo de compilación)
#define MIME_TABLE_SIZE (sizeof(mime_table) / sizeof(mime_table[0]))

const char *mime_get_type(const char *filename) {
    if (!filename) return "application/octet-stream";

    // strrchr encuentra la ÚLTIMA ocurrencia de '.'
    // Así "archivo.min.js" → extensión ".js" correctamente
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";

    for (size_t i = 0; i < MIME_TABLE_SIZE; i++) {
        if (strcmp(ext, mime_table[i].ext) == 0) {
            return mime_table[i].type;
        }
    }

    // Tipo desconocido: el navegador lo descargará en vez de renderizarlo
    return "application/octet-stream";
}
