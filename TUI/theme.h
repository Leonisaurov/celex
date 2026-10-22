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
const char *style_sequence(ScreenStyle style);

// Pone el tema que sigue en la lista. Devuelve el nombre del que ha quedado.
const char *theme_next(void);

// Cuantos temas hay, para poder dar la vuelta a la lista.
int theme_count(void);

#endif
