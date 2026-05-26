# MiniHTTPd

Servidor HTTP/1.1 básico implementado en C para Linux. 



## Características

- Sirve archivos estáticos: HTML, CSS, JavaScript, imágenes
- Procesa el método `GET` y analiza encabezados básicos (`Host`, `Connection`, `User-Agent`)
- Múltiples clientes simultáneos mediante `epoll` (edge-triggered)
- Conexiones persistentes HTTP/1.1 (`keep-alive`)
- Tipos MIME automáticos según extensión de archivo
- Envío de archivos con `sendfile()` (zero-copy)
- Respuestas de error con códigos de estado HTTP estándar
- Protección contra vulnerabilidades comunes



## Estructura del proyecto

```
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── http.h        # estructuras y funciones HTTP
│   ├── server.h      # inicialización del socket
│   ├── mime.h        # detección de tipos MIME
│   └── files.h       # acceso al sistema de archivos
├── src/
│   ├── main.c        # punto de entrada + bucle de eventos (epoll)
│   ├── server.c      # socket, getaddrinfo, bind, listen
│   ├── http.c        # parsing de solicitudes y generación de respuestas
│   ├── mime.c        # tabla de tipos MIME
│   └── files.c       # apertura segura de archivos con fstat
└── www/
    ├── index.html    # página principal de prueba
    ├── style.css     # estilos
    └── image.png     # imagen de prueba
```

---

## Arquitectura

### Flujo de una solicitud

```
Cliente (navegador / curl)
        │
        │  TCP connect
        ▼
   [main.c — epoll]
        │
        │  accept() → nuevo file descriptor
        │  recv()   → datos crudos HTTP
        ▼
   [http.c — http_parse_request()]
        │  valida método, URI, headers
        ▼
   [http.c — http_send_response()]
        │
        ├─ realpath()          → previene Directory Traversal
        ├─ [mime.c]            → detecta Content-Type
        ├─ [files.c]           → abre archivo con open() + fstat()
        └─ sendfile()          → envía archivo al socket (zero-copy)
```

### Módulos

| Archivo | Responsabilidad |
|---|---|
| `main.c` | Punto de entrada, bucle de eventos con `epoll` |
| `server.c` | Creación del socket con `getaddrinfo`, `bind`, `listen` |
| `http.c` | Parsing de solicitudes GET y generación de respuestas |
| `mime.c` | Mapeo de extensiones a tipos MIME |
| `files.c` | Apertura segura de archivos estáticos |

---

## Requisitos

- Linux (epoll y sendfile son APIs específicas de Linux)
- GCC
- Make

---

## Compilación

```bash
make
```

Para limpiar los archivos compilados:

```bash
make clean
```

---

## Uso

```bash
./minihttpd <puerto> <directorio_www>
```

**Ejemplo:**

```bash
./minihttpd 8080 www
```

El servidor queda escuchando en `http://localhost:8080`.

---

## Pruebas

### Solicitud normal

```bash
curl -v http://localhost:8080/
curl -v http://localhost:8080/style.css
```

### Archivo no encontrado (404)

```bash
curl -v http://localhost:8080/noexiste.html
```

### Método no permitido (405)

```bash
curl -v -X POST http://localhost:8080/
```

### Directory Traversal — debe responder 403

```bash
curl -v "http://localhost:8080/../../etc/passwd"
```

### Conexión persistente

```bash
curl -v --http1.1 -H "Connection: keep-alive" http://localhost:8080/
```

---

## Tipos MIME soportados

| Extensión | Tipo MIME |
|---|---|
| `.html` | `text/html` |
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.png` | `image/png` |
| `.jpg` / `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.ico` | `image/x-icon` |
| `.txt` | `text/plain` |
| otros | `application/octet-stream` |

---

## Códigos de estado HTTP

| Código | Significado | Cuándo se usa |
|---|---|---|
| `200 OK` | Éxito | Archivo encontrado y enviado correctamente |
| `400 Bad Request` | Solicitud inválida | URI demasiado larga, formato incorrecto |
| `403 Forbidden` | Acceso denegado | Intento de Directory Traversal detectado |
| `404 Not Found` | No encontrado | El archivo solicitado no existe |
| `405 Method Not Allowed` | Método no permitido | Cualquier método distinto de GET |
| `500 Internal Server Error` | Error del servidor | Fallo al leer el sistema de archivos |

---

## Seguridad

### Directory Traversal
Solicitudes como `GET /../../etc/passwd` son neutralizadas mediante `realpath()`, que resuelve rutas relativas y simbólicas a su ruta absoluta real. Luego se verifica que esa ruta comience con el directorio raíz del servidor (`www_root`). Si no, se responde con `403 Forbidden`.

```c
// Ejemplo de la protección en http.c
realpath(path, real_path);
if (strncmp(real_path, real_root, strlen(real_root)) != 0)
    http_send_error(fd, HTTP_403);
```

### Buffer Overflows
Se prohíbe el uso de `strcpy` y `sprintf`. En su lugar se usa `snprintf` en todas las copias de cadenas, garantizando un límite estricto y la terminación en `\0` en todo momento.

### Métodos inválidos
Solo se acepta `GET`. Cualquier otro método (`POST`, `DELETE`, `PUT`, etc.) recibe una respuesta `405 Method Not Allowed` inmediata.

### Límites de tamaño
Las solicitudes que superen `MAX_HEADER_LEN` (8192 bytes) o `MAX_URI_LEN` (2048 bytes) reciben `400 Bad Request`, previniendo ataques de denegación de servicio por solicitudes gigantes.

### TOCTOU en archivos
Se usa `fstat()` sobre el file descriptor ya abierto en lugar de `stat()` sobre la ruta, evitando condiciones de carrera entre la comprobación y el uso del archivo.

---

## Decisiones de diseño

**`epoll` en modo edge-triggered (`EPOLLET`):** notifica una sola vez cuando llegan datos, en lugar de repetidamente. Requiere poner los sockets en modo no-bloqueante y leer hasta obtener `EAGAIN`, pero escala mejor con muchas conexiones simultáneas.

**`sendfile()` (zero-copy):** transfiere el archivo directamente del buffer del kernel al socket sin pasar por espacio de usuario. Elimina dos copias de memoria y reduce las llamadas al sistema, mejorando el rendimiento especialmente con archivos grandes.

**`getaddrinfo()`:** soporta IPv4 e IPv6 automáticamente sin código específico para cada protocolo. La función itera la lista de candidatos devuelta hasta encontrar una dirección con la que `bind()` tenga éxito.

---

## Limitaciones

- Solo soporta el método `GET`
- No implementa chunked transfer encoding
- No soporta TLS/HTTPS
- No implementa logging a archivo
- Diseñado para uso educativo, no para producción

---

## Autor
Autor: David Guachamín
Proyecto — MiniHTTPd  
Materia: Computación Distribuida
