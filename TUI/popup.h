#ifndef CELEX_TUI_POPUP
#define CELEX_TUI_POPUP

#include <stdbool.h>

// La ventana flotante: un solo popup puesto a la vez, encima de la rejilla.
// No es una fila mas — se pinta dentro del fotograma, y por eso se redibuja
// solo cuando cambia, como lo demas.

// Lo maximo que puede medir el texto de una fila, y el nombre de un archivo.
#define POPUP_ITEM 64
#define POPUP_TEXT 1024

// Cuantas filas puede traer. Los items del menu caben todos, y los temas de
// sobra: una lista mas larga se quedaria corta, no fuera de sitio.
#define POPUP_LIMIT 32

typedef enum {
    POPUP_NONE,
    POPUP_MENU,   // el desplegable de una seccion de la barra
    POPUP_LIST,   // una lista para elegir: los temas
    POPUP_INPUT   // un campo de texto: guardar con otro nombre, abrir
} PopupKind;

// Lo que hace Enter, segun para que se abrio el popup.
typedef enum {
    POP_TARGET_NONE,
    POP_TARGET_THEME,
    POP_TARGET_SAVE_AS,
    POP_TARGET_OPEN
} PopupTarget;

typedef struct {
    bool active;
    PopupKind kind;
    PopupTarget target;

    // La seccion de la barra, cuando es un desplegable.
    int section;

    // Lo que se ve dentro: filas con su tecla, para que el boton siga siendo
    // su tecla tambien flotando.
    int count;
    int selected;

    // Lo que mide la tecla mas larga de la lista, para reservar su sitio a la
    // derecha de las filas. Cero cuando el popup no lleva pistas.
    int hintWidth;

    // El tema que habia antes de abrir la ventana de temas: la vista previa se
    // deshace al cancelar, como en cualquier ventana de elegir.
    int restoreTo;
    char labels[POPUP_LIMIT][POPUP_ITEM];
    int keys[POPUP_LIMIT];

    // El rectangulo, calculado al abrir y recortado a la pantalla.
    int x;
    int y;
    int w;
    int h;

    // Lo que se esta escribiendo en un campo de texto.
    char text[POPUP_TEXT];
} Popup;

// Cierra el popup, si estaba puesto.
void popup_close(Popup *popup);

// Cierra el popup dejando lo que estaba puesto antes de abrirlo: lo que hace
// Escape, un clic fuera o una tecla que no es de la ventana.
void popup_cancel(Popup *popup);

// El desplegable de una seccion de la barra, colgando de su nombre.
void popup_open_menu(Popup *popup, int section, int width, int gridBottom);

// Una lista para elegir, centrada en la rejilla. `current` es la fila puesta al
// abrir, o -1 para quedarse en la primera.
void popup_open_list(Popup *popup, PopupTarget target, const char *const *labels, int count, int current, int width, int gridBottom);

// El campo de texto, centrado, con lo que habia escrito ya por delante.
void popup_open_input(Popup *popup, PopupTarget target, const char *initial, int width, int gridBottom);

// El rectangulo otra vez, con la pantalla de ahora: lo que se ensancho o estro
// se recoloca sin cerrar nada.
void popup_place(Popup *popup, int width, int gridBottom);

// Si un punto cae dentro del popup, y en que fila de su lista (-1 si cae en el
// marco o en el titulo).
int popup_at_cell(Popup *popup, int x, int y, int *item);

#endif
