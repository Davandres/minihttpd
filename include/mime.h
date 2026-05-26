#ifndef MIME_H
#define MIME_H

// Dado un nombre de archivo como "index.html",
// devuelve el tipo MIME correspondiente, ej: "text/html"
// Si la extensión no se reconoce, devuelve "application/octet-stream"
const char *mime_get_type(const char *filename);

#endif
