#ifndef CELEX_TUI_TOOLS
#define CELEX_TUI_TOOLS

// Las herramientas, repartidas en secciones como la barra de un programa de
// ventanas, pero con nombres de lo que hay dentro y no de menu generico. Cada
// boton es la tecla que hace lo mismo: la barra no tiene ninguna orden propia,
// es lo visible de lo que ya se puede hacer a teclado, y por eso las dos formas
// nunca se contradicen.
typedef struct {
    const char *label;
    int key;
} MenuItem;

typedef struct {
    const char *label;
    const MenuItem *items;
    int count;
} MenuSection;

// Cuantas secciones hay, y la de la posicion de la lista.
int menu_count(void);
MenuSection menu_at(int index);

// El sitio maximo que hace falta para apuntar a todas las secciones: menu_fit
// no escribe fuera de ese sitio.
#define MENU_LIMIT 16

// El aire que queda a la izquierda de la primera seccion: la barra no empieza
// pegada al borde de la pantalla.
#define MENU_MARGIN 1

// El item de una seccion, o uno vacio si el numero no es de ninguno.
MenuItem menu_item_at(int section, int index);

// Cuantas secciones caben, con su nombre entero, en una fila de `width`
// columnas, y en que columna de la pantalla empieza cada una. La primera empieza
// despues del margen de la izquierda, asi que la barra no va pegada al borde.
// `starts` tiene sitio para MENU_LIMIT numeros, o NULL para solo contarlas.
// Devuelve las que caben.
int menu_fit(int width, int *starts);

// La seccion que esta en la columna `x` de la barra de una fila de `width`
// columnas, o -1 si no cae en ninguna (un hueco, o una que no cabia).
int menu_at_column(int width, int x);

// Como se escribe la tecla de un item en su popup: dos o tres letras, para que
// cada boton se anuncie con lo que hace a teclado. El texto es de un baul del
// propio menu: vale hasta la siguiente llamada.
const char *menu_hint(int key);

#endif
