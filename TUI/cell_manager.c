#include "cell_manager.h"
#include "hashmap.h"
#include "tui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cell.h"
#include "ref.h"
#include "version.h"

// El fotograma anterior, para redibujar solo lo que cambio.
static void free_previous(void);

#define HEADER_WIDTH 4
#define HEADER_HEIGHT 1
#define STRIDE_X (CELL_SIZE_X + 1)
#define STRIDE_Y (CELL_SIZE_Y + 1)
#define STATUS_HEIGHT 1

// La pantalla se dibuja por columnas, no por bytes: un caracter de la caja
// ocupa tres bytes en UTF-8 pero una sola columna. Cada celda del buffer
// guarda un caracter completo.
#define CELL_BYTES 4

#define BOX_H "\xE2\x94\x80"
#define BOX_V "\xE2\x94\x82"
#define BOX_TL "\xE2\x94\x8C"
#define BOX_TR "\xE2\x94\x90"
#define BOX_BL "\xE2\x94\x94"
#define BOX_BR "\xE2\x94\x98"
#define BOX_LT "\xE2\x94\x9C"
#define BOX_RT "\xE2\x94\xA4"
#define BOX_TD "\xE2\x94\xAC"
#define BOX_BD "\xE2\x94\xB4"
#define BOX_CR "\xE2\x94\xBC"

// Bytes del caracter UTF-8 que empieza en text.
static size_t utf8_size(const char *text) {
    unsigned char first = (unsigned char) text[0];

    if (first < 0x80) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    if ((first & 0xF8) == 0xF0) return 4;

    return 1;
}

// Caracteres de un texto en UTF-8, que es lo que ocupa en la pantalla.
static int utf8_columns(const char *text) {
    int columns = 0;

    for (size_t i = 0; text[i] != '\0'; i += utf8_size(text + i)) {
        columns++;
    }

    return columns;
}

static char *cell_at(char *buffer, int width, int x, int y) {
    return buffer + ((size_t) y * (size_t) width + (size_t) x) * CELL_BYTES;
}

static void put_box(char *buffer, int width, int x, int y, const char *glyph) {
    if (x < 0 || x >= width) {
        return;
    }

    char *cell = cell_at(buffer, width, x, y);

    // La celda se limpia entera, para que comparar dos fotogramas sea
    // comparar los bytes de la celda.
    memset(cell, 0, CELL_BYTES);

    memcpy(cell, glyph, 3);
    cell[3] = '\0';
}

static void put_char(char *buffer, int width, int x, int y, const char *text, size_t size) {
    if (x < 0 || x >= width) {
        return;
    }

    char *cell = cell_at(buffer, width, x, y);

    memset(cell, 0, CELL_BYTES);

    // Un caracter que no cabe en la celda se dibuja como interrogacion.
    if (size >= CELL_BYTES) {
        cell[0] = '?';
        cell[1] = '\0';
        return;
    }

    memcpy(cell, text, size);
    cell[size] = '\0';
}

static char *duplicate_text(const char *text) {
    if (text == NULL) {
        text = "";
    }

    size_t length = strlen(text);
    char *copy = (char *) malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);

    return copy;
}

static void history_push(History *history, unsigned int x, unsigned int y, const char *oldContent, const char *newContent) {
    if (history->count == history->capacity) {
        size_t capacity = history->capacity == 0 ? 8 : history->capacity * 2;
        HistoryEntry *items = (HistoryEntry *) realloc(history->items, capacity * sizeof(HistoryEntry));
        if (items == NULL) {
            return;
        }

        history->items = items;
        history->capacity = capacity;
    }

    HistoryEntry *entry = &history->items[history->count++];

    entry->x = x;
    entry->y = y;
    entry->oldContent = duplicate_text(oldContent);
    entry->newContent = duplicate_text(newContent);
}

static void history_clear(History *history) {
    for (size_t i = 0; i < history->count; i++) {
        free(history->items[i].oldContent);
        free(history->items[i].newContent);
    }

    free(history->items);

    history->items = NULL;
    history->count = 0;
    history->capacity = 0;
}

// El contenido que tiene ahora la celda.
static const char *content_of(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell *meta = get_cell(manager, x, y);

    return cell_content(meta);
}

// Guarda el cambio de una celda, para poder deshacerlo.
static void record_change(CellManager *manager, unsigned int x, unsigned int y, const char *oldContent);

// La posicion del caracter anterior, en bytes.
static size_t previous_offset(const char *text, size_t offset) {
    if (offset == 0) {
        return 0;
    }

    size_t position = offset - 1;

    // Los bytes de continuacion de un UTF-8 van hacia atras.
    while (position > 0 && ((unsigned char) text[position] & 0xC0) == 0x80) {
        position--;
    }

    return position;
}

// La posicion del caracter siguiente, en bytes.
static size_t next_offset(const char *text, size_t offset) {
    size_t length = strlen(text);

    if (offset >= length) {
        return length;
    }

    return offset + utf8_size(text + offset);
}

// El numero de caracteres del texto.
static size_t char_count(const char *text) {
    size_t count = 0;

    for (size_t i = 0; text[i] != '\0'; i += utf8_size(text + i)) {
        count++;
    }

    return count;
}

// El numero de caracteres que hay antes de una posicion, en bytes.
static size_t char_index(const char *text, size_t offset) {
    size_t count = 0;

    for (size_t i = 0; i < offset && text[i] != '\0'; i += utf8_size(text + i)) {
        count++;
    }

    return count;
}

void edit_reset(CellManager *manager) {
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);

    manager->editOffset = strlen(cell_content(meta));
}

void edit_move_cursor(CellManager *manager, int delta) {
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);
    const char *content = cell_content(meta);

    if (delta < 0) {
        manager->editOffset = previous_offset(content, manager->editOffset);
    } else {
        manager->editOffset = next_offset(content, manager->editOffset);
    }
}

void edit_to_start(CellManager *manager) {
    manager->editOffset = 0;
}

void edit_to_end(CellManager *manager) {
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);

    manager->editOffset = strlen(cell_content(meta));
}

void edit_insert(CellManager *manager, const char *text, size_t length) {
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);

    if (meta == NULL) {
        // La celda todavia no existe: se crea vacia para poder escribir en
        // ella.
        set_cell_content(manager, manager->cursor.x, manager->cursor.y, "");

        meta = get_cell(manager, manager->cursor.x, manager->cursor.y);
        if (meta == NULL) {
            return;
        }
    }

    char *old = duplicate_text(cell_content(meta));

    insert_cell_text(meta, manager->editOffset, text, length);
    manager->editOffset += length;

    record_change(manager, manager->cursor.x, manager->cursor.y, old);

    free(old);
}

void edit_backspace(CellManager *manager) {
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);

    if (meta == NULL || manager->editOffset == 0) {
        return;
    }

    size_t from = previous_offset(cell_content(meta), manager->editOffset);

    char *old = duplicate_text(cell_content(meta));

    delete_cell_text(meta, from, manager->editOffset - from);
    manager->editOffset = from;

    record_change(manager, manager->cursor.x, manager->cursor.y, old);

    free(old);
}

void edit_delete(CellManager *manager) {
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);

    if (meta == NULL) {
        return;
    }

    const char *content = cell_content(meta);
    size_t to = next_offset(content, manager->editOffset);

    if (to == manager->editOffset) {
        return;
    }

    char *old = duplicate_text(content);

    delete_cell_text(meta, manager->editOffset, to - manager->editOffset);

    record_change(manager, manager->cursor.x, manager->cursor.y, old);

    free(old);
}

// El valor de una celda se invalida cuando su contenido cambia: si no, se
// seguiria enseñando un valor viejo que ya no corresponde con lo que dice la
// celda.
static void invalidate_value(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell *meta = get_cell(manager, x, y);

    if (meta != NULL) {
        write_cell_value(meta, "");
    }
}

// Guarda el cambio de una celda. Los cambios seguidos de la misma celda se
// juntan, para que escribir una palabra sea un solo paso de deshacer.
static void record_change(CellManager *manager, unsigned int x, unsigned int y, const char *oldContent) {
    // Un cambio nuevo descarta lo que se iba a rehacer.
    history_clear(&manager->redo);

    manager->dirty = true;

    // El valor viejo ya no vale.
    invalidate_value(manager, x, y);

    if (manager->undo.count > 0) {
        HistoryEntry *last = &manager->undo.items[manager->undo.count - 1];

        if (last->x == x && last->y == y) {
            free(last->newContent);
            last->newContent = duplicate_text(content_of(manager, x, y));

            return;
        }
    }

    history_push(&manager->undo, x, y, oldContent, content_of(manager, x, y));
}

static int clear_cell_content(void *const context, struct hashmap_element_s *const element) {
    (void) context;

    MetaCell *meta = (MetaCell *) element->data;

    set_cell_text(meta, "");
    write_cell_value(meta, "");

    return 0;
}

void clear_sheet(CellManager *manager) {
    hashmap_iterate_pairs(&manager->map, clear_cell_content, NULL);

    history_clear(&manager->undo);
    history_clear(&manager->redo);

    manager->dirty = false;
    manager->editing = false;
    manager->editOffset = 0;
}

void undo_edit(CellManager *manager) {
    if (manager->undo.count == 0) {
        return;
    }

    HistoryEntry *entry = &manager->undo.items[manager->undo.count - 1];
    manager->undo.count--;

    // Se vuelve al contenido anterior.
    set_cell_content(manager, entry->x, entry->y, entry->oldContent);

    // El valor que habia ya no corresponde al contenido.
    invalidate_value(manager, entry->x, entry->y);

    // El cursor de texto se queda al final del contenido nuevo.
    manager->editOffset = strlen(entry->oldContent);

    // El cambio se puede rehacer.
    history_push(&manager->redo, entry->x, entry->y, entry->oldContent, entry->newContent);

    free(entry->oldContent);
    free(entry->newContent);
}

void redo_edit(CellManager *manager) {
    if (manager->redo.count == 0) {
        return;
    }

    HistoryEntry *entry = &manager->redo.items[manager->redo.count - 1];
    manager->redo.count--;

    // Se vuelve al contenido nuevo.
    set_cell_content(manager, entry->x, entry->y, entry->newContent);

    // El valor que habia ya no corresponde al contenido.
    invalidate_value(manager, entry->x, entry->y);

    // El cursor de texto se queda al final del contenido nuevo.
    manager->editOffset = strlen(entry->newContent);

    history_push(&manager->undo, entry->x, entry->y, entry->oldContent, entry->newContent);

    free(entry->oldContent);
    free(entry->newContent);
}

bool can_undo(CellManager *manager) {
    return manager->undo.count > 0;
}

bool can_redo(CellManager *manager) {
    return manager->redo.count > 0;
}

// El portapapeles del editor. Es propio, para que funcione igual en los dos
// sistemas. Guarda el contenido de la celda (su formula) y su valor (el
// resultado de evaluarla), porque al pegar dentro de una celda lo que se
// quiere es el resultado.
static char* clipboard = NULL;
static char* clipboard_value = NULL;

static void clipboard_set(const char* text, const char* value) {
    char* copy = duplicate_text(text);
    char* copy_value = duplicate_text(value);

    if (copy == NULL) {
        free(copy_value);
        return;
    }

    free(clipboard);
    free(clipboard_value);

    clipboard = copy;
    clipboard_value = copy_value;
}

void copy_cell_content(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell* meta = get_cell(manager, x, y);

    clipboard_set(cell_content(meta), cell_value(meta));
}

bool has_clipboard(void) {
    return clipboard != NULL;
}

void paste_cell_content(CellManager *manager, unsigned int x, unsigned int y) {
    if (clipboard == NULL) {
        return;
    }

    // En una celda se pega la formula entera.
    edit_cell_content(manager, x, y, clipboard);
}

void paste_cell_text(CellManager *manager, unsigned int x, unsigned int y) {
    if (clipboard == NULL) {
        return;
    }

    MetaCell* meta = get_or_create_cell(manager, x, y);
    if (meta == NULL) {
        return;
    }

    // Dentro de una celda se pega el resultado, que es lo que se quiere
    // escribir. Si la celda copiada no tenia valor, se pega su formula.
    const char* text = clipboard_value != NULL && clipboard_value[0] != '\0' ? clipboard_value : clipboard;

    char* old = duplicate_text(cell_content(meta));

    // El texto entra donde esta el cursor de la celda.
    insert_cell_text(meta, manager->editOffset, text, strlen(text));
    manager->editOffset += strlen(text);

    record_change(manager, x, y, old);

    free(old);
}

void edit_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *content) {
    MetaCell *meta = get_cell(manager, x, y);

    // El contenido de antes, que es vacio si la celda no existe todavia.
    char *old = duplicate_text(cell_content(meta));

    if (meta == NULL) {
        set_cell_content(manager, x, y, content);
    } else {
        set_cell_text(meta, content);
    }

    record_change(manager, x, y, old);

    free(old);
}

CellManager new_cell_manager(TermSize size) {
    CellManager manager;
    manager.size = size;
    manager.cursor = (TermSize) {
        .x = 0,
        .y = 0,
    };
    manager.camera = (TermSize) {
        .x = 0,
        .y = 0,
    };
    manager.message[0] = '\0';
    manager.showValues = false;
    manager.dirty = false;
    manager.editing = false;
    manager.editOffset = 0;
    manager.undo.items = NULL;
    manager.undo.count = 0;
    manager.undo.capacity = 0;
    manager.redo.items = NULL;
    manager.redo.count = 0;
    manager.redo.capacity = 0;

    if (0 != hashmap_create(35, &manager.map)) {
        // TODO! Error Handeling
    }

    return manager;
}

static int free_cell_entry(void *const context, struct hashmap_element_s *const element) {
    (void) context;

    if (element->key != NULL) {
        free((void *) element->key);
    }

    free_cell((MetaCell *) element->data);

    return 0;
}

void free_cell_manager(CellManager *manager) {
    hashmap_iterate_pairs(&manager->map, free_cell_entry, NULL);
    hashmap_destroy(&manager->map);

    history_clear(&manager->undo);
    history_clear(&manager->redo);

    if (clipboard != NULL) {
        free(clipboard);
        clipboard = NULL;
    }

    if (clipboard_value != NULL) {
        free(clipboard_value);
        clipboard_value = NULL;
    }

    free_previous();
}

static char *cell_key(unsigned int x, unsigned int y) {
    char *key = (char *) malloc(32);
    sprintf(key, "%u,%u", x, y);

    return key;
}

MetaCell *get_cell(CellManager *manager, unsigned int x, unsigned int y) {
    char key[32];
    sprintf(key, "%u,%u", x, y);

    return (MetaCell *) hashmap_get(&manager->map, key, (hashmap_uint32_t) strlen(key));
}

MetaCell *get_or_create_cell(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell *meta = get_cell(manager, x, y);

    if (meta != NULL) {
        return meta;
    }

    meta = (MetaCell *) malloc(sizeof(MetaCell));
    if (meta == NULL) {
        return NULL;
    }

    meta->cell = new_cell();
    meta->x = x;
    meta->y = y;

    char *key = cell_key(x, y);
    hashmap_put(&manager->map, key, (hashmap_uint32_t) strlen(key), meta);

    return meta;
}

void set_cell_value(CellManager *manager, unsigned int x, unsigned int y, const char *value) {
    MetaCell *meta = get_or_create_cell(manager, x, y);

    if (meta == NULL) {
        return;
    }

    write_cell_value(meta, value);
}

static int clear_value(void *const context, struct hashmap_element_s *const element) {
    (void) context;

    write_cell_value((MetaCell *) element->data, "");

    return 0;
}

void clear_cell_values(CellManager *manager) {
    hashmap_iterate_pairs(&manager->map, clear_value, NULL);
}

void set_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *content) {
    MetaCell *meta = get_cell(manager, x, y);

    if (meta == NULL) {
        meta = (MetaCell *) malloc(sizeof(MetaCell));
        if (meta == NULL) {
            return;
        }

        meta->cell = new_cell();
        meta->x = x;
        meta->y = y;

        char *key = cell_key(x, y);
        hashmap_put(&manager->map, key, (hashmap_uint32_t) strlen(key), meta);
    }

    set_cell_text(meta, content);
}

void clear_cell(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell *meta = get_cell(manager, x, y);

    if (meta == NULL) {
        return;
    }

    char *old = duplicate_text(cell_content(meta));

    set_cell_text(meta, "");
    record_change(manager, x, y, old);

    free(old);
}

void set_message(CellManager *manager, const char *message) {
    if (message == NULL) {
        manager->message[0] = '\0';
        return;
    }

    snprintf(manager->message, MESSAGE_SIZE, "%s", message);
}

static void update_camera(CellManager *manager, int columns, int rows) {
    if ((int) manager->cursor.x < (int) manager->camera.x) {
        manager->camera.x = manager->cursor.x;
    }
    if ((int) manager->cursor.x >= (int) manager->camera.x + columns) {
        manager->camera.x = manager->cursor.x - columns + 1;
    }
    if ((int) manager->cursor.y < (int) manager->camera.y) {
        manager->camera.y = manager->cursor.y;
    }
    if ((int) manager->cursor.y >= (int) manager->camera.y + rows) {
        manager->camera.y = manager->cursor.y - rows + 1;
    }
}

// Escribe las celdas de una fila, sin pasarse del ancho de la pantalla.
static void write_cells(const char *cells, int from, int to) {
    for (int x = from; x < to; x++) {
        const char *cell = cells + (size_t) x * CELL_BYTES;
        size_t length = strlen(cell);

        if (length > 0) {
            fwrite(cell, 1, length, stdout);
        }
    }
}

// Escribe un texto, recortado a un numero de columnas.
static void write_columns(const char *text, int columns) {
    int used = 0;

    for (size_t i = 0; text[i] != '\0' && used < columns; i += utf8_size(text + i)) {
        fwrite(text + i, 1, utf8_size(text + i), stdout);
        used++;
    }
}

// Escribe las celdas de [from, to) de una fila, con el resaltado en su sitio.
// El hueco es la columna del cursor de texto, que se dibuja sin resaltar para
// que se vea donde esta.
static void write_range(const char *cells, int from, int to, int highlight_start, int highlight_end, int hole) {
    int cursor = from;

    // La parte que va antes del resaltado.
    if (highlight_start > cursor && highlight_start < to) {
        write_cells(cells, cursor, highlight_start);
        cursor = highlight_start;
    }

    // La parte resaltada.
    if (highlight_end > cursor && highlight_start < to && highlight_end > from) {
        int start = highlight_start > cursor ? highlight_start : cursor;
        int end = highlight_end < to ? highlight_end : to;

        if (hole >= start && hole < end) {
            printf("\x1b[7m");
            write_cells(cells, start, hole);
            printf("\x1b[0m");
            write_cells(cells, hole, hole + 1);
            printf("\x1b[7m");
            write_cells(cells, hole + 1, end);
            printf("\x1b[0m");
        } else {
            printf("\x1b[7m");
            write_cells(cells, start, end);
            printf("\x1b[0m");
        }

        cursor = end;
    }

    write_cells(cells, cursor, to);
}

static void print_row(int y, const char *cells, int from, int to, int highlight_start, int highlight_end, int hole) {
    // El cursor va a la primera columna que se escribe, no a la primera de
    // la fila.
    move_cursor((uint16_t) from, (uint16_t) y);

    write_range(cells, from, to, highlight_start, highlight_end, hole);
}

// El ultimo fotograma, para redibujar solo lo que cambio.
static char *previous_buffer = NULL;
static int previous_width = 0;
static int previous_height = 0;
static int previous_highlight_x = 0;
static int previous_highlight_y = 0;
static int previous_text_cursor_x = -1;
static int previous_text_cursor_y = -1;
static bool previous_ready = false;

// La ultima barra de estado, para no redibujarla si no cambio.
static char previous_status[MESSAGE_SIZE * 2];
static char previous_extra[MESSAGE_SIZE * 2];
static bool previous_status_ready = false;

// El resaltado de una fila, o -1 si la fila no esta resaltada.
static void row_highlight(int y, int highlight_x, int highlight_y, int width, int *start, int *end) {
    *start = -1;
    *end = -1;

    if (y < highlight_y || y >= highlight_y + CELL_SIZE_Y) {
        return;
    }

    int from = highlight_x;
    int to = highlight_x + CELL_SIZE_X;

    if (from < 0) from = 0;
    if (to > width) to = width;
    if (from > width) from = width;
    if (to < from) to = from;

    *start = from;
    *end = to;
}

static void free_previous(void) {
    if (previous_buffer == NULL) {
        return;
    }

    free(previous_buffer);
    previous_buffer = NULL;
    previous_width = 0;
    previous_height = 0;
    previous_ready = false;
}

void print_cell_manager(CellManager *manager) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;

    int status_y = height - STATUS_HEIGHT;
    int columns = (width - HEADER_WIDTH - 3) / STRIDE_X;
    int rows = (status_y - HEADER_HEIGHT - 1) / STRIDE_Y;

    if (columns < 1) columns = 1;
    if (rows < 1) rows = 1;

    update_camera(manager, columns, rows);

    size_t cells_count = (size_t) width * (size_t) height;
    char *buffer = (char *) malloc(cells_count * CELL_BYTES);
    if (buffer == NULL) {
        return;
    }

    // Cada celda empieza con un espacio, para que la fila tenga el ancho
    // de la pantalla.
    for (size_t i = 0; i < cells_count; i++) {
        char *cell = buffer + i * CELL_BYTES;
        memset(cell, 0, CELL_BYTES);
        cell[0] = ' ';
    }

    for (int row = 0; row <= rows; row++) {
        int y = HEADER_HEIGHT + row * STRIDE_Y;
        if (y >= status_y) break;

        for (int x = HEADER_WIDTH; x < width && x <= HEADER_WIDTH + columns * STRIDE_X; x++) {
            put_box(buffer, width, x, y, BOX_H);
        }
    }

    for (int column = 0; column <= columns; column++) {
        int x = HEADER_WIDTH + column * STRIDE_X;

        for (int y = HEADER_HEIGHT; y < status_y && y <= HEADER_HEIGHT + rows * STRIDE_Y; y++) {
            put_box(buffer, width, x, y, BOX_V);
        }
    }

    for (int column = 0; column <= columns; column++) {
        for (int row = 0; row <= rows; row++) {
            int x = HEADER_WIDTH + column * STRIDE_X;
            int y = HEADER_HEIGHT + row * STRIDE_Y;
            if (y >= status_y) continue;

            const char *corner;
            if (column == 0) {
                corner = row == 0 ? BOX_TL : (row == rows ? BOX_BL : BOX_LT);
            } else if (column == columns) {
                corner = row == 0 ? BOX_TR : (row == rows ? BOX_BR : BOX_RT);
            } else if (row == 0) {
                corner = BOX_TD;
            } else if (row == rows) {
                corner = BOX_BD;
            } else {
                corner = BOX_CR;
            }

            put_box(buffer, width, x, y, corner);
        }
    }

    for (int column = 0; column < columns; column++) {
        char *name = ref_column_name((unsigned int) (manager->camera.x + column));
        int length = (int) strlen(name);

        for (int i = 0; i < length; i++) {
            int x = HEADER_WIDTH + column * STRIDE_X + 1 + i;
            put_char(buffer, width, x, HEADER_HEIGHT - 1, name + i, 1);
        }
        free(name);
    }

    for (int row = 0; row < rows; row++) {
        char label[16];
        sprintf(label, "%u", (unsigned int) (manager->camera.y + row + 1));
        int length = (int) strlen(label);

        for (int i = 0; i < length; i++) {
            int x = HEADER_WIDTH - 1 - (length - 1 - i);
            int y = HEADER_HEIGHT + row * STRIDE_Y + 1;
            put_char(buffer, width, x, y, label + i, 1);
        }
    }

    // Donde queda el cursor de texto, en coordenadas de la pantalla.
    int text_cursor_x = -1;
    int text_cursor_y = -1;

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            unsigned int x = (unsigned int) (manager->camera.x + column);
            unsigned int y = (unsigned int) (manager->camera.y + row);

            MetaCell *meta = get_cell(manager, x, y);
            if (meta == NULL) {
                continue;
            }

            bool is_cursor = manager->editing && x == manager->cursor.x && y == manager->cursor.y;
            bool is_selected = x == manager->cursor.x && y == manager->cursor.y;

            const char *content = cell_content(meta);
            // La celda del cursor enseña su fuente y no su valor: asi se ve la
            // formula que tiene, y no solo el resultado. Mientras se edita, lo
            // que se ve es siempre lo que se esta escribiendo.
            if (manager->showValues && !is_cursor && !is_selected) {
                content = cell_value(meta);
            }

            if (content[0] == '\0') {
                continue;
            }

            int base_x = HEADER_WIDTH + column * STRIDE_X + 1;
            int base_y = HEADER_HEIGHT + row * STRIDE_Y + 1;

            // Si el texto no cabe, se ve la parte donde esta el cursor.
            size_t total = char_count(content);
            size_t cursor_char = is_cursor ? char_index(content, manager->editOffset) : 0;

            size_t start = 0;
            if (total > CELL_CAPACITY) {
                start = cursor_char >= CELL_CAPACITY ? cursor_char - CELL_CAPACITY + 1 : 0;

                size_t max_start = total - CELL_CAPACITY;
                if (start > max_start) start = max_start;
            }

            // Cada caracter ocupa una celda, no un byte.
            size_t index = 0;
            int used = 0;
            int skipped = 0;

            while (content[index] != '\0') {
                size_t size = utf8_size(content + index);

                if (skipped < (int) start) {
                    skipped++;
                    index += size;
                    continue;
                }

                if (used >= CELL_CAPACITY) {
                    break;
                }

                int px = base_x + (used % CELL_SIZE_X);
                int py = base_y + (used / CELL_SIZE_X);

                if (py >= status_y) {
                    break;
                }

                put_char(buffer, width, px, py, content + index, size);

                index += size;
                used++;
            }

            if (is_cursor) {
                int position = (int) cursor_char - (int) start;

                if (position < 0) position = 0;
                if (position >= CELL_CAPACITY) position = CELL_CAPACITY - 1;

                text_cursor_x = base_x + (position % CELL_SIZE_X);
                text_cursor_y = base_y + (position / CELL_SIZE_X);
            }
        }
    }

    int cursor_column = (int) manager->cursor.x - manager->camera.x;
    int cursor_row = (int) manager->cursor.y - manager->camera.y;
    int highlight_x = HEADER_WIDTH + cursor_column * STRIDE_X + 1;
    int highlight_y = HEADER_HEIGHT + cursor_row * STRIDE_Y + 1;

    // Cuando cambia el tamano de la terminal hay que redibujar todo.
    bool full = !previous_ready || previous_width != width || previous_height != height;

    if (full) {
        free_previous();
        clear_screen();

        // La pantalla se limpio entera, asi que la barra de estado tambien
        // se tiene que redibujar.
        previous_status_ready = false;
    }

    for (int y = 0; y < status_y; y++) {
        const char *row = buffer + (size_t) y * (size_t) width * CELL_BYTES;
        const char *old_row = previous_ready ? previous_buffer + (size_t) y * (size_t) previous_width * CELL_BYTES : NULL;

        int start = -1;
        int end = -1;
        row_highlight(y, highlight_x, highlight_y, width, &start, &end);

        int old_start = -1;
        int old_end = -1;
        if (previous_ready) {
            row_highlight(y, previous_highlight_x, previous_highlight_y, previous_width, &old_start, &old_end);
        }

        // Las columnas que cambian de contenido.
        int from = 0;
        int to = width;

        if (!full && old_row != NULL) {
            from = width;
            to = 0;

            for (int x = 0; x < width; x++) {
                if (memcmp(row + (size_t) x * CELL_BYTES, old_row + (size_t) x * CELL_BYTES, CELL_BYTES) != 0) {
                    if (x < from) from = x;
                    if (x + 1 > to) to = x + 1;
                }
            }
        }

        // El resaltado viejo y el nuevo tienen que quedar tapados, porque el
        // inverso de la terminal se queda pegado.
        if (old_start >= 0) {
            if (from > old_start) from = old_start;
            if (to < old_end) to = old_end;
        }

        if (start >= 0) {
            if (from > start) from = start;
            if (to < end) to = end;
        }

        // Si el cursor de texto se movio, las filas donde estaba y donde esta
        // se redibujan enteras, para que no quede el hueco del cursor viejo.
        if (text_cursor_x != previous_text_cursor_x || text_cursor_y != previous_text_cursor_y) {
            if (y == text_cursor_y || y == previous_text_cursor_y) {
                from = 0;
                to = width;
            }
        }

        int hole = (y == text_cursor_y) ? text_cursor_x : -1;

        if (to > from) {
            print_row(y, row, from, to, start, end, hole);
        }
    }

    // Se guarda el fotograma para la proxima vez.
    if (!previous_ready || previous_width != width || previous_height != height) {
        previous_buffer = (char *) malloc(cells_count * CELL_BYTES);
        if (previous_buffer == NULL) {
            free(buffer);
            flush();
            return;
        }

        previous_width = width;
        previous_height = height;
    }

    memcpy(previous_buffer, buffer, cells_count * CELL_BYTES);

    previous_highlight_x = highlight_x;
    previous_highlight_y = highlight_y;
    previous_text_cursor_x = text_cursor_x;
    previous_text_cursor_y = text_cursor_y;
    previous_ready = true;

    free(buffer);
    flush();
}

static void append_text(char *status, size_t size, const char *text) {
    size_t length = strlen(status);

    if (length + 1 >= size) {
        return;
    }

    snprintf(status + length, size - length, "%s", text);
}

// Agrega el texto y lo rellena con espacios hasta ocupar las columnas
// pedidas.
static void append_padded(char *status, size_t size, const char *text, int columns) {
    append_text(status, size, text);

    int used = utf8_columns(text);

    for (int i = used; i < columns; i++) {
        size_t length = strlen(status);

        if (length + 1 >= size) {
            break;
        }

        status[length] = ' ';
        status[length + 1] = '\0';
    }
}

void print_status(CellManager *manager, const char *mode, const char *path) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;
    int y = height - STATUS_HEIGHT;

    char *ref = ref_to_string(manager->cursor.x, manager->cursor.y);
    MetaCell *meta = get_cell(manager, manager->cursor.x, manager->cursor.y);
    const char *content = cell_content(meta);
    const char *value = cell_value(meta);

    char status[MESSAGE_SIZE * 2];
    status[0] = '\0';

    append_text(status, sizeof(status), " ");
    // Con que compilacion se esta trabajando.
    append_padded(status, sizeof(status), CELEX_BUILD, 8);
    append_text(status, sizeof(status), "  ");
    append_text(status, sizeof(status), ref);
    append_text(status, sizeof(status), "  ");
    append_padded(status, sizeof(status), mode, 6);
    append_text(status, sizeof(status), "  ");
    // Si se ven los valores o el fuente de las celdas.
    append_padded(status, sizeof(status), manager->showValues ? "VAL" : "SRC", 3);
    append_text(status, sizeof(status), "  ");
    append_padded(status, sizeof(status), content, 16);
    append_text(status, sizeof(status), "  ");
    append_text(status, sizeof(status), path == NULL ? "[no file]" : path);

    free(ref);

    // El valor o el mensaje van al final de la linea, encima de la barra.
    char extra[MESSAGE_SIZE * 2];
    extra[0] = '\0';

    if (manager->message[0] != '\0') {
        append_text(extra, sizeof(extra), manager->message);
    } else if (manager->showValues && value[0] != '\0') {
        append_text(extra, sizeof(extra), value);
    }

    // Si la barra no cambio no se redibuja.
    if (previous_status_ready &&
        strcmp(status, previous_status) == 0 &&
        strcmp(extra, previous_extra) == 0) {
        return;
    }

    int offset = utf8_columns(status) + 1;

    move_cursor(0, (uint16_t) y);
    printf("\x1b[7m");
    write_columns(status, width);
    printf("\x1b[0m");

    if (offset < width) {
        // El valor o el mensaje van encima de la barra, y despues se ponen
        // espacios para tapar lo que habia antes.
        int columns = utf8_columns(extra);
        int was = utf8_columns(previous_extra);

        move_cursor((uint16_t) offset, (uint16_t) y);
        write_columns(extra, width - offset);

        for (int i = columns; i < was && offset + i < width; i++) {
            fputc(' ', stdout);
        }
    }

    snprintf(previous_status, sizeof(previous_status), "%s", status);
    snprintf(previous_extra, sizeof(previous_extra), "%s", extra);
    previous_status_ready = true;

    flush();
}
