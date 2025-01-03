#ifndef CELEX_TUI_CELL_MANAGER
#define CELEX_TUI_CELL_MANAGER
#include "hashmap.h"
#include "tui.h"

typedef struct {
    struct hashmap_s map;
    TermSize size;
    TermSize cursor;
} CellManager;

CellManager new_cell_manager(TermSize size);
void free_cell_manager(CellManager *manager);

void print_cell_manager(CellManager *manager);
#endif
