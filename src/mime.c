#include "mime.h"
#include <string.h>

// Tabla de tipos MIME
typedef struct {
    const char *extension;
    const char *mime_type;
} mime_entry;

static const mime_entry mime_types[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain"},
    {".pdf", "application/pdf"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {NULL, "application/octet-stream"}  // Tipo por defecto
};

const char* get_mime_type(const char *filename) {
    // Encontrar la última ocurrencia del punto
    const char *dot = strrchr(filename, '.');
    if (!dot) {
        return "application/octet-stream";
    }
    
    // Buscar en la tabla de tipos MIME
    for (int i = 0; mime_types[i].extension != NULL; i++) {
        if (strcasecmp(dot, mime_types[i].extension) == 0) {
            return mime_types[i].mime_type;
        }
    }
    
    return "application/octet-stream";
}
