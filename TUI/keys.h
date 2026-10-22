#ifndef CELEX_TUI_KEYS
#define CELEX_TUI_KEYS

#include <stdbool.h>
#include "cell_manager.h"

#define KEY_CTRL_S 19
#define KEY_CTRL_R 18
#define KEY_CTRL_Y 25
#define KEY_CTRL_Z 26
#define KEY_CTRL_C 3
#define KEY_CTRL_V 22
#define KEY_CTRL_L 12

// Lo que el editor recuerda entre pulsaciones de teclas.
typedef struct {
    // El archivo de la hoja, que puede ser NULL.
    const char *path;
    // Si el editor sigue vivo.
    bool running;
    // Que se pidio salir y falta la confirmacion.
    bool quitting;
    // Que se pidio recargar y falta la confirmacion.
    bool reloading;
} Editor;

// Vuelve a calcular la hoja, para que los valores correspondan con el
// contenido de las celdas. Devuelve true si dejo un mensaje en la barra.
bool recalculate_sheet(CellManager *manager);

// Maneja una tecla. Devuelve false si el editor debe terminar.
bool handle_key(Editor *editor, CellManager *manager, int key);

// Aplica un pegado: el texto entra en la celda, o en el texto que se esta
// editando.
bool handle_paste(CellManager *manager, const char *text);

#endif
