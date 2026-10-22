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
#define KEY_CTRL_T 20
#define KEY_CTRL_A 1
#define KEY_CTRL_D 4
#define KEY_CTRL_N 14
#define KEY_CTRL_P 16
#define KEY_CTRL_F 6
#define KEY_CTRL_E 5
#define KEY_CTRL_B 2
#define KEY_CTRL_O 15
#define KEY_CTRL_U 21
#define KEY_CTRL_K 11

// Lo que el editor recuerda entre pulsaciones de teclas.
typedef struct {
    // El archivo que se abrio, que puede ser NULL: un guion del lenguaje o un
    // documento del editor.
    const char *path;
    // El archivo donde guarda, que es un documento del editor aunque se abrio un
    // guion.
    const char *savePath;
    // Si el editor sigue vivo.
    bool running;
    // Que se pidio salir y falta la confirmacion.
    bool quitting;
    // Que se pidio recargar y falta la confirmacion.
    bool reloading;
    // El ultimo clic de la izquierda: cuando, y en que celda. Dos clics seguidos
    // en la misma celda son un doble clic, que es como se entra a editar.
    unsigned long long clickTime;
    unsigned int clickX;
    unsigned int clickY;
    // Si el ultimo clic ya fue el segundo de un doble.
    bool clickedTwice;
} Editor;

// Cuanto tiene que pasar entre dos clics en la misma celda para que sean un doble
// clic, en milisegundos. Es el tiempo de siempre de las ventanas.
#define CLICK_DOUBLE_MS 500

// Vuelve a calcular la hoja, para que los valores correspondan con el
// contenido de las celdas. Devuelve true si dejo un mensaje en la barra.
bool recalculate_sheet(CellManager *manager);

// Maneja una tecla. Devuelve false si el editor debe terminar.
bool handle_key(Editor *editor, CellManager *manager, int key);

// Maneja un suceso del raton: un clic en una celda mueve el cursor, en una
// pestana cambia de hoja y en un boton manda su tecla.
bool handle_mouse(Editor *editor, CellManager *manager, const Mouse *mouse);

// Aplica un pegado: el texto entra en la celda, o en el texto que se esta
// editando.
bool handle_paste(CellManager *manager, const char *text);

#endif
