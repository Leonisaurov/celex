#ifndef CELEX_TUI_PATHS
#define CELEX_TUI_PATHS

#include <stddef.h>

// La carpeta de configuracion del usuario: `$HOME/.config/celex`. Se escribe en
// `out` (con `size` de espacio). Si no se encuentra la casa, cae en `.`.
void conf_dir(char *out, size_t size);

// La carpeta de los temas: `<conf>/themes`. Mismo contrato que conf_dir.
void conf_themes_dir(char *out, size_t size);

// El archivo de las preferencias del usuario: `<conf>/config`. Mismo contrato
// que conf_dir. No dice si existe: eso lo mira quien lo lee.
void conf_preferences_file(char *out, size_t size);

// Crea la carpeta de configuracion y la de temas si aun no existen. Devuelve 1
// si las dos estan disponibles, 0 si algo fallo.
int conf_ensure(void);

// Escribe una plantilla comentada en `<conf>/themes/_ejemplo.theme`, solo si no
// existe ya: nunca pisa lo que el usuario haya puesto. El guion bajo del nombre
// hace que el cargador la ignore, hasta que se renombre. Devuelve 1 si la creo.
int conf_seed_example(void);

// Llamada por cada archivo `*.theme` de una carpeta, con su camino entero ya
// montado. Se ignoran los que empiezan por `_` o por `.`, y las carpetas.
typedef void (*ThemeFileCb)(const char *path, void *ctx);

// Recorre la carpeta y llama a `cb` con cada `*.theme`. Devuelve cuantos llamo.
int conf_each_theme_file(const char *dir, ThemeFileCb cb, void *ctx);

#endif
