#ifndef CELEX_TUI_CELL_MANAGER
#define CELEX_TUI_CELL_MANAGER
#include <stdbool.h>
#include "hashmap.h"
#include "tui.h"
#include "cell.h"

#define MESSAGE_SIZE 128

typedef struct {
    struct hashmap_s map;
    TermSize size;
    TermSize cursor;
    TermSize camera;
    bool showValues;
    char message[MESSAGE_SIZE];
} CellManager;

CellManager new_cell_manager(TermSize size);
void free_cell_manager(CellManager *manager);

MetaCell *get_cell(CellManager *manager, unsigned int x, unsigned int y);
MetaCell *get_or_create_cell(CellManager *manager, unsigned int x, unsigned int y);
void set_cell_value(CellManager *manager, unsigned int x, unsigned int y, const char *value);
void clear_cell_values(CellManager *manager);
void set_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *content);
void clear_cell(CellManager *manager, unsigned int x, unsigned int y);
void append_cell_character(CellManager *manager, unsigned int x, unsigned int y, char character);
void backspace_cell(CellManager *manager, unsigned int x, unsigned int y);
void set_message(CellManager *manager, const char *message);

void print_cell_manager(CellManager *manager);
void print_status(CellManager *manager, const char *mode, const char *path);

#endif
