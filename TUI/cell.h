#ifndef CELEX_TUI_CELL
#define CELEX_TUI_CELL
#include <stddef.h>

// El tamano de una celda en la pantalla: cuantas columnas y cuantas filas
// ocupa. No esta fijado al compilar: se cambia en marcha con las teclas del
// mas y el menos, para ver formulas largas o mas hojas en la pantalla.
extern int cellWidth;
extern int cellHeight;

#define CELL_SIZE_X (cellWidth)
#define CELL_SIZE_Y (cellHeight)
#define CELL_CAPACITY (cellWidth * cellHeight)

// Limites del tamano, para que una celda quepa en la pantalla mas pequena y
// no se coma toda una hoja la mas grande.
#define CELL_MIN_WIDTH 2
#define CELL_MAX_WIDTH 40
#define CELL_MIN_HEIGHT 1
#define CELL_MAX_HEIGHT 8

typedef struct {
    char *content;
    char *value;
} Cell;

typedef struct {
    Cell cell;
    unsigned int x;
    unsigned int y;
} MetaCell;

Cell new_cell();
void free_cell(MetaCell *meta);

void set_cell_text(MetaCell *meta, const char *content);
void write_cell_value(MetaCell *meta, const char *value);

// Inserta texto en una posicion del contenido.
void insert_cell_text(MetaCell *meta, size_t offset, const char *text, size_t length);

// Borra un trozo del contenido.
void delete_cell_text(MetaCell *meta, size_t offset, size_t length);

const char *cell_content(MetaCell *meta);
const char *cell_value(MetaCell *meta);

#endif
