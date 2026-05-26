#ifndef FILES_H
#define FILES_H

#include <stddef.h>

// Estructura para contenido de archivo
typedef struct {
    char *data;
    size_t size;
} file_content;

// Funciones de manejo de archivos
file_content* read_file(const char *path);
void free_file_content(file_content *fc);
int is_safe_path(const char *base_dir, const char *requested_path);
int file_exists(const char *path);

#endif
