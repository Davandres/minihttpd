#include "files.h"

#include <sys/types.h>
#include <sys/stat.h>   // stat(), struct stat
#include <fcntl.h>      // open(), O_RDONLY
#include <unistd.h>     // close()

int file_open(const char *path, size_t *size) {

    // Abrimos en modo solo lectura
    // O_RDONLY es suficiente; sendfile() solo necesita leer
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    // stat() sobre el fd ya abierto para obtener el tamaño real del archivo
    // Usamos fstat (sobre fd) en vez de stat (sobre path) por dos razones:
    //   1. Evita TOCTOU (Time-Of-Check-Time-Of-Use): el archivo no puede
    //      cambiar entre el open() y el fstat() porque ya lo tenemos abierto
    //   2. Es ligeramente más eficiente (no resuelve la ruta de nuevo)
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }

    // Verificamos que sea un archivo regular
    // No queremos que alguien pida un socket Unix o un device file
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return -1;
    }

    *size = (size_t)st.st_size;
    return fd;
}
