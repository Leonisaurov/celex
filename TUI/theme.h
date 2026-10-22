#ifndef CELEX_TUI_THEME
#define CELEX_TUI_THEME

// Las partes de la pantalla que pueden llevar color. Cada una tiene su
// secuencia ANSI en el tema que este puesto.
typedef enum {
    STYLE_NONE,     // el vacio de la pantalla, y el hueco del cursor de texto
    STYLE_GRID,     // las rayas de la rejilla
    STYLE_HEADER,   // las letras de las columnas y los numeros de las filas
    STYLE_TEXT,     // el fuente de una celda
    STYLE_VALUE,    // el valor de una celda, cuando se muestran los valores
    STYLE_CURSOR,   // la celda seleccionada
    STYLE_COPIED,   // el borde punteado de la celda copiada
    STYLE_STATUS,   // la barra de estado
    STYLE_MESSAGE,  // lo que la barra dice a la derecha
    STYLE_ERROR,    // lo que la barra dice cuando algo no salio
    STYLE_COUNT
} ScreenStyle;

// El nombre del tema que esta puesto, para decirlo en la barra.
const char *theme_name(void);

// La secuencia ANSI de un estilo. Siempre empieza apagando lo que hubiera,
// asi que cambiar de estilo es escribir una sola secuencia.
//
// Ademas de los estilos del tema hay sitio para los que pone el formato de una
// celda: se piden con style_custom, y el numero que devuelven vale mientras dura
// el programa, para que el numero guardado en el fotograma signifique siempre lo
// mismo.
const char *style_sequence(int style);

// Guarda una secuencia propia y devuelve su numero, o el estilo del texto si ya
// no queda sitio.
int style_custom(const char *sequence);

// Olvida las secuencias propias: las del tema anterior no valen con el nuevo.
void theme_reset_custom(void);

// Pone el tema que sigue en la lista. Devuelve el nombre del que ha quedado.
const char *theme_next(void);

// Pone el tema que se llama asi, como lo escribe el archivo del editor.
// Devuelve si lo encontro.
int theme_set_by_name(const char *name);

// Cuantos temas hay, para poder dar la vuelta a la lista.
int theme_count(void);

#endif
