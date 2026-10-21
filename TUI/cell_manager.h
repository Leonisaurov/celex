#ifndef CELEX_TUI_CELL_MANAGER
#define CELEX_TUI_CELL_MANAGER
#include <stdbool.h>
#include "hashmap.h"
#include "tui.h"
#include "cell.h"

#define MESSAGE_SIZE 128

// Un cambio en el contenido de una celda, para poder deshacerlo.
typedef struct {
    unsigned int x;
    unsigned int y;
    char* oldContent;
    char* newContent;
} HistoryEntry;

typedef struct {
    HistoryEntry* items;
    size_t count;
    size_t capacity;
} History;

typedef struct {
    struct hashmap_s map;
    TermSize size;
    TermSize cursor;
    TermSize camera;
    bool showValues;
    // Si la hoja cambio desde la ultima vez que se guardo.
    bool dirty;
    // Si se esta editando el contenido de una celda.
    bool editing;
    // Donde esta el cursor de texto dentro del contenido, en bytes.
    size_t editOffset;
    char message[MESSAGE_SIZE];
    History undo;
    History redo;
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

// Deshace el ultimo cambio y rehace el que se deshizo.
void undo_edit(CellManager *manager);
void redo_edit(CellManager *manager);
bool can_undo(CellManager *manager);
bool can_redo(CellManager *manager);

// Cambia el contenido de una celda y lo deja en el historial.
void edit_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *content);

// Copia el contenido de una celda al portapapeles del editor, y pega el
// contenido del portapapeles en una celda.
void copy_cell_content(CellManager *manager, unsigned int x, unsigned int y);
void paste_cell_content(CellManager *manager, unsigned int x, unsigned int y);
bool has_clipboard(void);

// El cursor de texto, dentro del contenido de la celda que se edita.
void edit_reset(CellManager *manager);
void edit_move_cursor(CellManager *manager, int delta);
void edit_to_start(CellManager *manager);
void edit_to_end(CellManager *manager);
void edit_insert(CellManager *manager, const char *text, size_t length);
void edit_backspace(CellManager *manager);
void edit_delete(CellManager *manager);

#endif
