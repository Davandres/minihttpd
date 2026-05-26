# Compilador y flags
CC      = gcc
# -Wall: activa todos los warnings importantes
# -Wextra: warnings adicionales
# -g: incluye símbolos de debug (para usar con gdb)
# -I include: le dice al compilador dónde buscar los .h
CFLAGS  = -Wall -Wextra -g -I include

# Nombre del ejecutable final
TARGET  = minihttpd

# Lista de archivos objeto que se generarán desde src/
SRCS    = src/main.c src/server.c src/http.c src/mime.c src/files.c
OBJS    = $(SRCS:.c=.o)      # reemplaza .c por .o en cada nombre

# Regla principal: enlaza todos los .o para producir el ejecutable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Regla genérica: cómo compilar cualquier .c → .o
# $< = el archivo fuente (.c), $@ = el objetivo (.o)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpia los archivos generados
clean:
	rm -f $(OBJS) $(TARGET)

# "phony" indica que 'clean' no es un archivo real
.PHONY: clean
