#ifndef CELEX_TUI_CELL
#define CELEX_TUI_CELL
#include <stddef.h>

#define CELL_SIZE_X 4
#define CELL_SIZE_Y 2
#define CELL_CAPACITY (CELL_SIZE_X * CELL_SIZE_Y)

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
