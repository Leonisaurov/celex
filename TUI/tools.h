#ifndef CELEX_TUI_TOOLS
#define CELEX_TUI_TOOLS

// Las herramientas que se pueden tocar. Cada boton es la tecla que hace lo
// mismo: la barra no tiene ninguna orden propia, es lo visible de lo que ya se
// puede hacer a teclado, y por eso las dos formas nunca se contradicen.
typedef struct {
    const char *name;
    int key;
} Tool;

// El nombre de la fila, por delante de los botones.
#define TOOLS_LABEL "tools:"

// La herramienta de la posicion de la lista, con la tecla que manda.
Tool tool_at(int index);

// Cuantas herramientas hay, y el sitio maximo que hace falta para apuntar a
// todas. tools_fit no escribe fuera de ese sitio.
#define TOOL_LIMIT 32

int tool_count(void);

// Lo que ocupa el nombre de la fila, con el hueco que lleva detras.
int tools_label_width(void);

// Cuantas caben, con su nombre entero, en una fila de `width` columnas, y en que
// columna de la pantalla empieza cada una. `starts` tiene sitio para
// tool_count() numeros, o NULL para solo contarlas. Devuelve las que caben.
int tools_fit(int width, int *starts);

// La herramienta que esta en la columna `x` de una fila de `width` columnas, o
// -1 si no cae en ninguna (un hueco, o una que no cabia).
int tool_at_column(int width, int x);

#endif
