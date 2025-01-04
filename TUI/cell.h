#ifndef CELEX_TUI_CELL
#define CELEX_TUI_CELL

#include "string.h"
#define CELL_SIZE_X 4
#define CELL_SIZE_Y 2

typedef struct {
    string content;
} Cell;

typedef struct {
    Cell cell;
    unsigned int x;
    unsigned int y;
} MetaCell;

Cell new_cell();

#endif
