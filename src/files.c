#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

file_content* read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    
    // Obtener tamaño del archivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    
    // Asignar memoria para el contenido
    file_content *fc = malloc(sizeof(file_content));
    if (!fc) {
        fclose(file);
        return NULL;
    }
    
    fc->data = malloc(file_size + 1);
    if (!fc->data) {
        free(fc);
        fclose(file);
        return NULL;
    }
    
    // Leer el archivo
    size_t bytes_read = fread(fc->data, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(fc->data);
        free(fc);
        return NULL;
    }
    
    fc->data[file_size] = '\0';
    fc->size = file_size;
    
    return fc;
}

void free_file_content(file_content *fc) {
    if (fc) {
        free(fc->data);
        free(fc);
    }
}

int is_safe_path(const char *base_dir, const char *requested_path) {
    // Usar realpath para resolver la ruta real y prevenir directory traversal
    char full_path[2048];
    char resolved_path[2048];
    char resolved_base[2048];
    
    // Construir la ruta completa
    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, requested_path);
    
    // Obtener la ruta real resuelta
    if (realpath(full_path, resolved_path) == NULL) {
        // Si la ruta no existe, verificar que no intente salir del directorio base
        char *normalized = realpath(base_dir, resolved_base);
        if (!normalized) return 0;
        
        // Verificar que la ruta solicitada no contenga ".."
        if (strstr(requested_path, "..") != NULL) {
            return 0;
        }
        
        return 1;
    }
    
    // Obtener la ruta real del directorio base
    if (realpath(base_dir, resolved_base) == NULL) {
        return 0;
    }
    
    // Verificar que la ruta resuelta esté dentro del directorio base
    size_t base_len = strlen(resolved_base);
    if (strncmp(resolved_path, resolved_base, base_len) != 0) {
        return 0;
    }
    
    // Verificar que no sea exactamente igual al directorio base (para evitar listar directorios)
    if (strlen(resolved_path) == base_len) {
        return 1;
    }
    
    // Verificar que el siguiente carácter sea '/'
    if (resolved_path[base_len] != '/') {
        return 0;
    }
    
    return 1;
}

int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}
