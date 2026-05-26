#ifndef FILES_H
#define FILES_H

#include <stddef.h>

// Abre el archivo en `path` de forma segura (modo lectura)
// Escribe el tamaño del archivo en *size
// Devuelve el file descriptor abierto, o -1 en error
// IMPORTANTE: el llamador debe cerrar el fd con close()
int file_open(const char *path, size_t *size);

#endif
