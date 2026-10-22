#include "cell_manager.h"
#include "hashmap.h"
#include "theme.h"
#include "tools.h"
#include "tui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Para mirar el reloj entre los fotogramas del deslizamiento.
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

#include "cell.h"
#include "ref.h"
#include "tools.h"
#include "version.h"

// El fotograma anterior, para redibujar solo lo que cambio.
static void free_previous(void);

// Hacen falta antes de estar definidas: soltar las celdas de una hoja y cambiar
// de hoja.
static int free_cell_entry(void *const context, struct hashmap_element_s *const element);
void sheet_select(CellManager *manager, int index);

// La hoja que se esta viendo, en las funciones que tienen el gestor a mano. Todo
// lo que es propio de una hoja (celdas, cursor, vista, historial) se mira aqui.
#define SHEET (sheet_state(manager))

// El margen de la izquierda: las cuatro primeras columnas son para el numero
// de fila y la quinta queda de aire, para que una celda cortada por la
// izquierda no se pegue al numero.
#define HEADER_WIDTH 5
#define HEADER_HEIGHT 1
#define STRIDE_X (CELL_SIZE_X + 1)
#define STRIDE_Y (CELL_SIZE_Y + 1)
#define STATUS_HEIGHT 1
// Las hojas tambien tienen su fila, encima de la barra de estado.
#define SHEET_BAR_HEIGHT 1
// Y las herramientas la suya, encima de las hojas, cuando estan puestas.
#define TOOLS_HEIGHT 1

// Las filas de abajo. La rejilla acaba donde empieza lo demas: la fila de las
// herramientas, la de las pestanas y la de estado. Escondiendo las herramientas
// la rejilla gana una fila.
int grid_bottom(const CellManager *manager, int height) {
    return height - STATUS_HEIGHT - SHEET_BAR_HEIGHT - (manager->showTools ? TOOLS_HEIGHT : 0);
}

int tools_row(const CellManager *manager, int height) {
    return grid_bottom(manager, height);
}

int sheet_row(int height) {
    return height - STATUS_HEIGHT - SHEET_BAR_HEIGHT;
}

// La pantalla se dibuja por columnas, no por bytes: un caracter de la caja
// ocupa tres bytes en UTF-8 pero una sola columna. Cada celda del buffer
// guarda un caracter completo (GLYPH_BYTES) y despues el estilo con el que se
// pinta, con tres bytes de relleno para que todas ocupen lo mismo.
#define GLYPH_BYTES 4
#define CELL_BYTES 8

// El estilo de una celda del buffer, que va justo despues del caracter.
#define CELL_STYLE(cell) ((int) (unsigned char) (cell)[GLYPH_BYTES])

#define BOX_H "\xE2\x94\x80"
#define BOX_V "\xE2\x94\x82"
#define BOX_CR "\xE2\x94\xBC"

// La celda copiada se marca con un borde punteado, como las hormigas
// marchantes de las hojas de calculo, para saber que celda se copio.
#define BOX_DASH_H "\xE2\x94\x84"
#define BOX_DASH_V "\xE2\x94\x86"

// El fotograma que se esta pintando. Se reutiliza entre fotogramas: pintar uno
// detras de otro no tiene que costar asignar la pantalla entera cada vez.
static char *paint_buffer = NULL;
static size_t paint_buffer_cells = 0;

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

static void put_box(char *buffer, int width, int height, int x, int y, const char *glyph, int style) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    char *cell = cell_at(buffer, width, x, y);

    // La celda se limpia entera, para que comparar dos fotogramas sea
    // comparar los bytes de la celda: el caracter y su estilo.
    memset(cell, 0, CELL_BYTES);

    memcpy(cell, glyph, 3);
    cell[3] = '\0';
    cell[GLYPH_BYTES] = (char) (unsigned char) style;
}

// Deja un espacio con el estilo pedido, para pintar de color un sitio donde no
// hay nada que ver: el bloque de la celda seleccionada.
static void put_blank(char *buffer, int width, int height, int x, int y, int style) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    char *cell = cell_at(buffer, width, x, y);

    memset(cell, 0, CELL_BYTES);

    cell[0] = ' ';
    cell[GLYPH_BYTES] = (char) (unsigned char) style;
}

static void put_char(char *buffer, int width, int height, int x, int y, const char *text, size_t size, int style) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    char *cell = cell_at(buffer, width, x, y);

    memset(cell, 0, CELL_BYTES);

    // Un caracter que no cabe en la celda se dibuja como interrogacion.
    if (size >= GLYPH_BYTES) {
        cell[0] = '?';
        cell[1] = '\0';
        cell[GLYPH_BYTES] = (char) (unsigned char) style;
        return;
    }

    memcpy(cell, text, size);
    cell[size] = '\0';
    cell[GLYPH_BYTES] = (char) (unsigned char) style;
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

// Se quita de en medio la oferta del tabulador: al escribir, borrar o cambiar de
// celda, la palabra que se estaba completando ya no es la que era.
static void drop_completion(CellManager *manager) {
    manager->completion.active = false;
    manager->completion.list.count = 0;
    manager->completion.index = 0;
}

void edit_reset(CellManager *manager) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);

    SHEET->editOffset = strlen(cell_content(meta));

    drop_completion(manager);
}

void edit_move_cursor(CellManager *manager, int delta) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);
    const char *content = cell_content(meta);

    if (delta < 0) {
        SHEET->editOffset = previous_offset(content, SHEET->editOffset);
    } else {
        SHEET->editOffset = next_offset(content, SHEET->editOffset);
    }
}

void edit_to_start(CellManager *manager) {
    SHEET->editOffset = 0;
}

void edit_to_end(CellManager *manager) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);

    SHEET->editOffset = strlen(cell_content(meta));
}

void edit_insert(CellManager *manager, const char *text, size_t length) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);

    drop_completion(manager);

    if (meta == NULL) {
        // La celda todavia no existe: se crea vacia para poder escribir en
        // ella.
        set_cell_content(manager, SHEET->cursor.x, SHEET->cursor.y, "");

        meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);
        if (meta == NULL) {
            return;
        }
    }

    char *old = duplicate_text(cell_content(meta));

    insert_cell_text(meta, SHEET->editOffset, text, length);
    SHEET->editOffset += length;

    record_change(manager, SHEET->cursor.x, SHEET->cursor.y, old);

    free(old);
}

void edit_backspace(CellManager *manager) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);

    drop_completion(manager);

    if (meta == NULL || SHEET->editOffset == 0) {
        return;
    }

    size_t from = previous_offset(cell_content(meta), SHEET->editOffset);

    char *old = duplicate_text(cell_content(meta));

    delete_cell_text(meta, from, SHEET->editOffset - from);
    SHEET->editOffset = from;

    record_change(manager, SHEET->cursor.x, SHEET->cursor.y, old);

    free(old);
}

void edit_delete(CellManager *manager) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);

    drop_completion(manager);

    if (meta == NULL) {
        return;
    }

    const char *content = cell_content(meta);
    size_t to = next_offset(content, SHEET->editOffset);

    if (to == SHEET->editOffset) {
        return;
    }

    char *old = duplicate_text(content);

    delete_cell_text(meta, SHEET->editOffset, to - SHEET->editOffset);

    record_change(manager, SHEET->cursor.x, SHEET->cursor.y, old);

    free(old);
}

// El autocompletado. La palabra que se esta escribiendo es el trozo de letras,
// numeros y guiones bajos que queda delante del cursor de texto.
static bool is_word_character(unsigned char character) {
    return isalnum(character) != 0 || character == '_';
}

// Donde empieza la palabra que acaba en la posicion pedida.
static size_t word_start(const char *text, size_t offset) {
    while (offset > 0 && is_word_character((unsigned char) text[offset - 1])) {
        offset--;
    }

    return offset;
}

// Lo que hay entre dos posiciones del contenido, como palabra suelta.
static void word_between(const char *text, size_t from, size_t to, char *into, size_t size) {
    size_t length = to > from ? to - from : 0;

    if (length >= size) {
        length = size - 1;
    }

    memcpy(into, text + from, length);
    into[length] = '\0';
}

// Lo que se recoge de la hoja para completar: el prefijo que se esta escribiendo
// y la lista donde guardar lo que empieza igual.
typedef struct {
    const char *prefix;
    CompleteList *list;
} CompleteCollect;

static int collect_word(void *const context, struct hashmap_element_s *const element) {
    CompleteCollect *collect = (CompleteCollect *) context;
    const char *text = cell_content((MetaCell *) element->data);

    size_t i = 0;

    while (text[i] != '\0') {
        while (text[i] != '\0' && !is_word_character((unsigned char) text[i])) {
            i++;
        }

        size_t from = i;

        while (text[i] != '\0' && is_word_character((unsigned char) text[i])) {
            i++;
        }

        if (i == from) {
            continue;
        }

        char word[COMPLETE_WORD];
        word_between(text, from, i, word, sizeof(word));

        // Lo que ya esta escrito no se ofrece: no aporta nada.
        if (strcmp(word, collect->prefix) == 0) {
            continue;
        }

        if (complete_starts_with(word, collect->prefix)) {
            complete_add(collect->list, word);
        }
    }

    return 0;
}

// Las palabras que pueden empezar como la que se esta escribiendo: primero las
// del lenguaje, que son siempre las mismas, y despues las que ya hay en la hoja.
static void complete_lookup(CellManager *manager, const char *prefix, CompleteList *list) {
    list->count = 0;

    if (prefix[0] == '\0') {
        return;
    }

    for (int i = 0; i < complete_keyword_count(); i++) {
        const char *word = complete_keyword(i);

        if (strcmp(word, prefix) != 0 && complete_starts_with(word, prefix)) {
            complete_add(list, word);
        }
    }

    CompleteCollect collect = { prefix, list };

    hashmap_iterate_pairs(&SHEET->map, collect_word, &collect);
}

// Cambia la palabra que se esta escribiendo por otra.
static void complete_replace(CellManager *manager, size_t from, const char *word) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);
    const char *text = cell_content(meta);

    char *old = duplicate_text(text);

    delete_cell_text(meta, from, SHEET->editOffset - from);
    insert_cell_text(meta, from, word, strlen(word));
    SHEET->editOffset = from + strlen(word);

    record_change(manager, SHEET->cursor.x, SHEET->cursor.y, old);

    free(old);
}

void complete_tab(CellManager *manager) {
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);
    const char *text = cell_content(meta);

    Completion *completion = &manager->completion;

    // Sin oferta delante: se busca la palabra y lo que pueda completarse.
    if (!completion->active) {
        size_t start = word_start(text, SHEET->editOffset);

        char prefix[COMPLETE_WORD];
        word_between(text, start, SHEET->editOffset, prefix, sizeof(prefix));

        CompleteList list;
        complete_lookup(manager, prefix, &list);

        if (list.count == 0) {
            return;
        }

        strcpy(completion->origin, prefix);
        completion->start = start;
        completion->list = list;
        completion->index = 0;

        // Con una sola candidata no hay que elegir: se escribe y se acaba.
        if (list.count == 1) {
            complete_replace(manager, start, list.words[0]);
            completion->active = false;
            completion->list.count = 0;
            return;
        }

        // Con varias, se escribe lo que todas tienen al principio, que es lo
        // unico que no puede equivocar. El indice en -1 dice que se esta
        // mostrando ese trozo, y que el primer tabulador tiene que llevar a la
        // primera palabra.
        char common[COMPLETE_WORD];
        complete_common_prefix(&list, common, sizeof(common));

        if (strcmp(common, prefix) != 0) {
            complete_replace(manager, start, common);
        }

        completion->index = -1;
        completion->active = true;

        return;
    }

    // Con la oferta delante, el tabulador pasa a la siguiente palabra. Al dar la
    // vuelta se vuelve a lo que estaba escrito, para poder seguir escribiendo.
    completion->index++;

    size_t start = completion->start;

    if (completion->index >= completion->list.count) {
        complete_replace(manager, start, completion->origin);
        completion->active = false;
        completion->list.count = 0;
        completion->index = 0;
        return;
    }

    complete_replace(manager, start, completion->list.words[completion->index]);
}

int completion_text(CellManager *manager, char *into, size_t size) {
    Completion *completion = &manager->completion;

    into[0] = '\0';

    if (!completion->active) {
        return 0;
    }

    for (int i = 0; i < completion->list.count; i++) {
        size_t length = strlen(into);

        if (length + COMPLETE_WORD + 4 >= size) {
            break;
        }

        // La que esta puesta ahora se marca entre llaves.
        snprintf(into + length, size - length, i == completion->index ? " {%s}" : " %s", completion->list.words[i]);
    }

    return 1;
}

// El color de un numero de paleta, en secuencia ANSI: del 1 al 8 los oscuros,
// del 9 al 16 los brillantes. `back` pide el fondo en vez del delante.
static void color_sequence(int color, int back, char *into, size_t size) {
    into[0] = '\0';

    if (color <= 0) {
        return;
    }

    if (color <= 8) {
        snprintf(into, size, "\x1b[%dm", (back ? 40 : 30) + color - 1);
        return;
    }

    snprintf(into, size, "\x1b[%dm", (back ? 100 : 90) + color - 9);
}

// El estilo con el que se pinta una celda: el del tema, con lo que diga su
// formato por encima. Sin formato, el estilo del tema tal cual. La alineacion no
// cuenta aqui: mueve el texto, no lo pinta.
static int format_style(CellFormat format, int base) {
    if (format.bold == 0 && format.underline == 0 && format.fg == 0 && format.bg == 0) {
        return base;
    }

    char foreground[16];
    char background[16];

    color_sequence(format.fg, 0, foreground, sizeof(foreground));
    color_sequence(format.bg, 1, background, sizeof(background));

    char sequence[160];

    snprintf(sequence, sizeof(sequence), "%s%s%s%s%s",
             style_sequence(base),
             format.bold != 0 ? "\x1b[1m" : "",
             format.underline != 0 ? "\x1b[4m" : "",
             foreground, background);

    return style_custom(sequence);
}

// Si un texto es un numero, para alinearlo por defecto como en una hoja de
// calculo. Un fuente de formula no lo es: empieza con el igual.
static int looks_numeric(const char *text) {
    int digits = 0;

    for (; *text != '\0'; text++) {
        if (*text >= '0' && *text <= '9') {
            digits = 1;
            continue;
        }

        if (*text == '.' || *text == ',' || *text == '-' || *text == '+') {
            continue;
        }

        return 0;
    }

    return digits;
}

// Cuanto se corre una linea dentro de la celda, segun su alineacion. Lo que
// sobra por la derecha se deja delante del texto, y lo de el centro se parte.
// Sin decir nada, la celda se alinea como en una hoja de calculo.
int align_pad(int align, const char *text, int lineLength) {
    if (align == ALIGN_DEFAULT) {
        align = looks_numeric(text) ? ALIGN_RIGHT : ALIGN_LEFT;
    }

    int free = CELL_SIZE_X - lineLength;

    if (free <= 0) {
        return 0;
    }

    if (align == ALIGN_RIGHT) {
        return free;
    }

    if (align == ALIGN_CENTER) {
        return free / 2;
    }

    return 0;
}

// El formato de la celda del cursor, para cambiarlo. La celda aparece si no
// estaba: se puede dar formato a un sitio vacio de la misma forma que a uno con
// algo escrito.
CellFormat *cursor_format(CellManager *manager) {
    MetaCell *meta = get_or_create_cell(manager, SHEET->cursor.x, SHEET->cursor.y);

    if (meta == NULL) {
        return NULL;
    }

    SHEET->dirty = true;

    return &meta->cell.format;
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
    history_clear(&SHEET->redo);

    SHEET->dirty = true;

    // El valor viejo ya no vale.
    invalidate_value(manager, x, y);

    if (SHEET->undo.count > 0) {
        HistoryEntry *last = &SHEET->undo.items[SHEET->undo.count - 1];

        if (last->x == x && last->y == y) {
            free(last->newContent);
            last->newContent = duplicate_text(content_of(manager, x, y));

            return;
        }
    }

    history_push(&SHEET->undo, x, y, oldContent, content_of(manager, x, y));
}

static int clear_cell_content(void *const context, struct hashmap_element_s *const element) {
    (void) context;

    MetaCell *meta = (MetaCell *) element->data;

    set_cell_text(meta, "");
    write_cell_value(meta, "");

    return 0;
}

void clear_sheet(CellManager *manager) {
    hashmap_iterate_pairs(&SHEET->map, clear_cell_content, NULL);

    history_clear(&SHEET->undo);
    history_clear(&SHEET->redo);

    SHEET->dirty = false;
    SHEET->editing = false;
    SHEET->editOffset = 0;
}

void undo_edit(CellManager *manager) {
    if (SHEET->undo.count == 0) {
        return;
    }

    HistoryEntry *entry = &SHEET->undo.items[SHEET->undo.count - 1];
    SHEET->undo.count--;

    // Se vuelve al contenido anterior.
    set_cell_content(manager, entry->x, entry->y, entry->oldContent);

    // El valor que habia ya no corresponde al contenido.
    invalidate_value(manager, entry->x, entry->y);

    // El cursor de texto se queda al final del contenido nuevo.
    SHEET->editOffset = strlen(entry->oldContent);

    // El cambio se puede rehacer.
    history_push(&SHEET->redo, entry->x, entry->y, entry->oldContent, entry->newContent);

    free(entry->oldContent);
    free(entry->newContent);
}

void redo_edit(CellManager *manager) {
    if (SHEET->redo.count == 0) {
        return;
    }

    HistoryEntry *entry = &SHEET->redo.items[SHEET->redo.count - 1];
    SHEET->redo.count--;

    // Se vuelve al contenido nuevo.
    set_cell_content(manager, entry->x, entry->y, entry->newContent);

    // El valor que habia ya no corresponde al contenido.
    invalidate_value(manager, entry->x, entry->y);

    // El cursor de texto se queda al final del contenido nuevo.
    SHEET->editOffset = strlen(entry->newContent);

    history_push(&SHEET->undo, entry->x, entry->y, entry->oldContent, entry->newContent);

    free(entry->oldContent);
    free(entry->newContent);
}

bool can_undo(CellManager *manager) {
    return SHEET->undo.count > 0;
}

bool can_redo(CellManager *manager) {
    return SHEET->redo.count > 0;
}

// El portapapeles del editor vive en el gestor de celdas, para que el editor
// y las pruebas usen siempre el mismo. Guarda el contenido de la celda (su
// formula) y su valor (el resultado de evaluarla), y tambien de que celda
// viene, para poder marcarla.
void copy_cell_content(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell* meta = get_cell(manager, x, y);

    clipboard_set_cell(&manager->clipboard, cell_content(meta), cell_value(meta), x, y);
}

void paste_cell_content(CellManager *manager, unsigned int x, unsigned int y, const char *text) {
    if (text == NULL) {
        return;
    }

    // En una celda se pega la formula entera.
    edit_cell_content(manager, x, y, text);
}

void paste_cell_text(CellManager *manager, unsigned int x, unsigned int y, const char *text) {
    if (text == NULL) {
        return;
    }

    MetaCell* meta = get_or_create_cell(manager, x, y);
    if (meta == NULL) {
        return;
    }

    char* old = duplicate_text(cell_content(meta));

    // El texto entra donde esta el cursor de la celda.
    insert_cell_text(meta, SHEET->editOffset, text, strlen(text));
    SHEET->editOffset += strlen(text);

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

// El estado de una hoja recien hecha: vacia, con el cursor en A1, la vista sin
// deslizar y nada que deshacer.
static SheetState new_sheet_state(void) {
    SheetState state;

    memset(&state, 0, sizeof(state));

    if (0 != hashmap_create(35, &state.map)) {
        // TODO! Error Handeling
    }

    return state;
}

Sheet *current_sheet(CellManager *manager) {
    return &manager->sheets[manager->active];
}

// La hoja numero index, para mirar su nombre o su estado sin estar viendola.
Sheet *sheet_at(CellManager *manager, int index) {
    if (index < 0 || (size_t) index >= manager->sheetCount) {
        return NULL;
    }

    return &manager->sheets[index];
}

SheetState *sheet_state(CellManager *manager) {
    return &manager->sheets[manager->active].state;
}

int sheet_count(CellManager *manager) {
    return (int) manager->sheetCount;
}

int sheet_active(CellManager *manager) {
    return manager->active;
}

const char *sheet_name_at(CellManager *manager, int index) {
    if (index < 0 || (size_t) index >= manager->sheetCount) {
        return "";
    }

    return manager->sheets[index].name;
}

// Se sueltan las celdas de una hoja y sus historiales.
static void sheet_destroy(SheetState *state) {
    hashmap_iterate_pairs(&state->map, free_cell_entry, NULL);
    hashmap_destroy(&state->map);

    history_clear(&state->undo);
    history_clear(&state->redo);

    memset(state, 0, sizeof(*state));
}

// Si alguna hoja, menos el hueco numero excepto, se llama ya asi.
static bool sheet_name_taken(CellManager *manager, const char *name, int except) {
    for (size_t i = 0; i < manager->sheetCount; i++) {
        if ((int) i != except && strcmp(manager->sheets[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

// Mete una hoja en el hueco numero index, moviendo las de detras. Devuelve el
// hueco, o -1 si no hubo sitio.
static int sheet_insert(CellManager *manager, int index, const char *name) {
    if (index < 0 || (size_t) index > manager->sheetCount) {
        index = (int) manager->sheetCount;
    }

    if (manager->sheetCount == manager->sheetCapacity) {
        size_t capacity = manager->sheetCapacity == 0 ? 4 : manager->sheetCapacity * 2;
        Sheet *sheets = (Sheet *) realloc(manager->sheets, capacity * sizeof(Sheet));
        if (sheets == NULL) {
            return -1;
        }

        manager->sheets = sheets;
        manager->sheetCapacity = capacity;
    }

    for (size_t i = manager->sheetCount; i > (size_t) index; i--) {
        manager->sheets[i] = manager->sheets[i - 1];
    }

    memset(&manager->sheets[index], 0, sizeof(Sheet));

    if (name == NULL || name[0] == '\0') {
        // El nombre de encima, sin repetir ninguno de los que ya hay: dos hojas
        // con el mismo nombre no se pueden distinguir en las pestanas.
        int number = (int) manager->sheetCount + 1;

        do {
            snprintf(manager->sheets[index].name, SHEET_NAME_SIZE, "Sheet%d", number++);
        } while (sheet_name_taken(manager, manager->sheets[index].name, index));
    } else {
        snprintf(manager->sheets[index].name, SHEET_NAME_SIZE, "%s", name);
    }

    manager->sheets[index].state = new_sheet_state();
    manager->sheetCount++;

    // Las hojas que estaban detras del hueco suben un numero, y la que se esta
    // viendo tambien.
    if (index <= manager->active) {
        manager->active++;
    }

    return index;
}

int sheet_add(CellManager *manager, const char *name) {
    // La hoja nueva va justo detras de la que se esta viendo, que es donde se
    // espera en una hoja de calculo.
    int index = sheet_insert(manager, manager->active + 1, name);

    if (index < 0) {
        return -1;
    }

    sheet_select(manager, index);

    return index;
}

void sheet_rename(CellManager *manager, int index, const char *name) {
    if (index < 0 || (size_t) index >= manager->sheetCount || name == NULL || name[0] == '\0') {
        return;
    }

    // Dos hojas con el mismo nombre no se pueden distinguir en las pestanas.
    if (sheet_name_taken(manager, name, index)) {
        return;
    }

    snprintf(manager->sheets[index].name, SHEET_NAME_SIZE, "%s", name);

    invalidate_screen();
}

void sheet_select(CellManager *manager, int index) {
    if (index < 0 || (size_t) index >= manager->sheetCount || index == manager->active) {
        return;
    }

    manager->active = index;

    // El nombre que se estuviera escribiendo era de la otra hoja.
    manager->renaming = false;
    manager->renameText[0] = '\0';

    SheetState *state = sheet_state(manager);

    // La hoja que se coge no viene editando, y la oferta de palabras del
    // tabulador era de la otra hoja.
    state->editing = false;
    state->editOffset = 0;

    manager->completion.active = false;
    manager->completion.list.count = 0;

    // La marca de la celda copiada es de una hoja: al cambiar se quita, aunque
    // el portapapeles se quede con lo copiado.
    clipboard_forget_cell(&manager->clipboard);

    invalidate_screen();
}

void sheet_remove(CellManager *manager, int index) {
    // La ultima hoja no se quita: una hoja de calculo sin hojas no es una hoja
    // de calculo.
    if (index < 0 || (size_t) index >= manager->sheetCount || manager->sheetCount <= 1) {
        return;
    }

    sheet_destroy(&manager->sheets[index].state);

    for (size_t i = (size_t) index; i + 1 < manager->sheetCount; i++) {
        manager->sheets[i] = manager->sheets[i + 1];
    }

    manager->sheetCount--;

    if (index < manager->active) {
        manager->active--;
    } else if (manager->active >= (int) manager->sheetCount) {
        manager->active = (int) manager->sheetCount - 1;
    }

    sheet_state(manager)->editing = false;
    manager->completion.active = false;

    clipboard_forget_cell(&manager->clipboard);
    invalidate_screen();
}

// Alguna hoja tiene cambios sin guardar, aunque no sean de la que se esta
// viendo.
bool sheet_any_dirty(CellManager *manager) {
    for (int i = 0; i < sheet_count(manager); i++) {
        if (manager->sheets[i].state.dirty) {
            return true;
        }
    }

    return false;
}

// Todas las hojas pasan a estar guardadas, que es lo que son despues de escribir
// el archivo entero.
void sheet_mark_saved(CellManager *manager) {
    for (size_t i = 0; i < manager->sheetCount; i++) {
        manager->sheets[i].state.dirty = false;
    }
}

// Deja el archivo como estaba al abrirse: una sola hoja, sin celdas y con el
// nombre de empezar. Es lo que hace falta antes de volver a leer el archivo,
// porque lo que habia puede ser de mas hojas.
void workbook_clear(CellManager *manager) {
    while (manager->sheetCount > 1) {
        sheet_remove(manager, (int) manager->sheetCount - 1);
    }

    sheet_select(manager, 0);
    clear_sheet(manager);

    strcpy(manager->sheets[0].name, "Sheet1");

    invalidate_screen();
}

CellManager new_cell_manager(TermSize size) {
    CellManager manager;

    memset(&manager, 0, sizeof(manager));

    manager.size = size;
    clipboard_init(&manager.clipboard);

    // Se empieza con una sola hoja, y la ultima no se puede quitar: el gestor
    // siempre tiene una que ver.
    manager.sheetCapacity = 4;
    manager.sheets = (Sheet *) calloc(manager.sheetCapacity, sizeof(Sheet));
    manager.sheetCount = 1;
    manager.active = 0;
    manager.showTools = true;

    strcpy(manager.sheets[0].name, "Sheet1");
    manager.sheets[0].state = new_sheet_state();

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
    // Se sueltan todas las hojas, no solo la que se estaba viendo.
    for (size_t i = 0; i < manager->sheetCount; i++) {
        sheet_destroy(&manager->sheets[i].state);
    }

    free(manager->sheets);
    manager->sheets = NULL;
    manager->sheetCount = 0;
    manager->sheetCapacity = 0;

    clipboard_free(&manager->clipboard);

    free_previous();

    free(paint_buffer);
    paint_buffer = NULL;
    paint_buffer_cells = 0;
}

static char *cell_key(unsigned int x, unsigned int y) {
    char *key = (char *) malloc(32);
    sprintf(key, "%u,%u", x, y);

    return key;
}

MetaCell *get_cell(CellManager *manager, unsigned int x, unsigned int y) {
    char key[32];
    sprintf(key, "%u,%u", x, y);

    return (MetaCell *) hashmap_get(&SHEET->map, key, (hashmap_uint32_t) strlen(key));
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
    hashmap_put(&SHEET->map, key, (hashmap_uint32_t) strlen(key), meta);

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
    hashmap_iterate_pairs(&SHEET->map, clear_value, NULL);
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
        hashmap_put(&SHEET->map, key, (hashmap_uint32_t) strlen(key), meta);
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
    manager->messageIsError = false;

    if (message == NULL) {
        manager->message[0] = '\0';
        return;
    }

    snprintf(manager->message, MESSAGE_SIZE, "%s", message);
}

// Lo mismo, pero avisando de que lo que paso no salio: la barra lo pinta con
// el estilo de los errores, para que se vea sin tener que leerlo.
void set_error_message(CellManager *manager, const char *message) {
    set_message(manager, message);
    manager->messageIsError = true;
}

// Desplaza la vista lo justo para que la celda del cursor se vea entera. El
// desplazamiento va de caracter en caracter, no de celda en celda: al llegar
// al borde, la vista se desliza y en el lado de enfrente queda una celda
// cortada, en vez de saltar toda la rejilla un ancho de celda.
static void update_view(CellManager *manager, int viewWidth, int viewHeight) {
    // Las columnas de los bordes de la celda del cursor, en coordenadas de la
    // rejilla (0 es la primera columna util despues del margen).
    int left = (int) SHEET->cursor.x * STRIDE_X;
    int right = left + CELL_SIZE_X;

    if (left - SHEET->originX < 0) {
        SHEET->originX = left;
    }

    if (right - SHEET->originX > viewWidth - 1) {
        SHEET->originX = right - (viewWidth - 1);

        // Si la celda no cabe entera, se alinea a la izquierda.
        if (left - SHEET->originX < 0) {
            SHEET->originX = left;
        }
    }

    int top = (int) SHEET->cursor.y * STRIDE_Y;
    int bottom = top + CELL_SIZE_Y;

    if (top - SHEET->originY < 0) {
        SHEET->originY = top;
    }

    if (bottom - SHEET->originY > viewHeight - 1) {
        SHEET->originY = bottom - (viewHeight - 1);

        if (top - SHEET->originY < 0) {
            SHEET->originY = top;
        }
    }

    if (SHEET->originX < 0) SHEET->originX = 0;
    if (SHEET->originY < 0) SHEET->originY = 0;
}

// La primera celda que toca la ventana por la izquierda (o por arriba). Puede
// estar cortada: se ve si su borde del otro lado sigue dentro.
static int first_cell(int origin, int cellSize, int stride) {
    int from = origin - cellSize;

    if (from <= 0) {
        return 0;
    }

    return (from + stride - 1) / stride;
}

// La ultima celda que toca la ventana.
static int last_cell(int origin, int viewSize, int stride) {
    int to = origin + viewSize - 1;

    if (to < 0) {
        return -1;
    }

    return to / stride;
}

// Escribe las celdas de una fila, sin pasarse del ancho de la pantalla, y
// cambiando de estilo cuando hace falta. Cada secuencia de estilo empieza
// apagando lo que hubiera, asi que cambiar de estilo cuesta una sola.
//
// Lo que se compara es la secuencia, no el estilo: dos estilos pueden pintarse
// igual, y en un tema sin color casi todos son el mismo apagado.
static void write_cells(const char *cells, int from, int to) {
    const char *pending = NULL;

    for (int x = from; x < to; x++) {
        const char *cell = cells + (size_t) x * CELL_BYTES;
        const char *sequence = style_sequence(CELL_STYLE(cell));
        size_t length = strlen(cell);

        if (pending == NULL || strcmp(pending, sequence) != 0) {
            fputs(sequence, stdout);
            pending = sequence;
        }

        if (length > 0) {
            fwrite(cell, 1, length, stdout);
        }
    }

    // Se apaga el ultimo estilo, para que no se quede pegado en lo que se
    // escriba despues.
    if (pending != NULL && strcmp(pending, style_sequence(STYLE_NONE)) != 0) {
        fputs(style_sequence(STYLE_NONE), stdout);
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

static void print_row(int y, const char *cells, int from, int to) {
    // El cursor va a la primera columna que se escribe, no a la primera de
    // la fila.
    move_cursor((uint16_t) from, (uint16_t) y);

    write_cells(cells, from, to);
}

// El ultimo fotograma, para redibujar solo lo que cambio.
static char *previous_buffer = NULL;
static int previous_width = 0;
static int previous_height = 0;
static bool previous_ready = false;

// La ultima barra de estado, para no redibujarla si no cambio.
static char previous_status[MESSAGE_SIZE * 2];
static char previous_extra[MESSAGE_SIZE * 2];
static bool previous_status_ready = false;
// Si la ultima vez se pudo escribir la ruta del archivo al final de la barra.
static bool previous_path_shown = false;

// La ultima linea de las pestañas, para no redibujarla si no cambio.
static char previous_sheets[MESSAGE_SIZE * 4];
static int previous_sheets_active = -1;
static bool previous_sheets_ready = false;

// El ultimo ancho con el que se pintaron las herramientas: la fila solo depende
// del ancho, asi que con el mismo no se vuelve a escribir. -1 cuando no esta
// puesta, para que al volver a ponerla se pinte.
static int previous_tools_width = -1;

// Cambia el tamano de las celdas, con los limites puestos.
void set_cell_size(int width, int height) {
    if (width < CELL_MIN_WIDTH) width = CELL_MIN_WIDTH;
    if (width > CELL_MAX_WIDTH) width = CELL_MAX_WIDTH;
    if (height < CELL_MIN_HEIGHT) height = CELL_MIN_HEIGHT;
    if (height > CELL_MAX_HEIGHT) height = CELL_MAX_HEIGHT;

    if (width == cellWidth && height == cellHeight) {
        return;
    }

    cellWidth = width;
    cellHeight = height;

    // Con otro tamano todo cambia de sitio: se olvida el fotograma anterior,
    // para volver a pintar la rejilla entera en vez de ir corrigiendo lo que
    // se fue moviendo.
    invalidate_screen();
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

// Dibuja el borde punteado de la celda copiada, si se ve en la pantalla.
static void draw_copied_cell(CellManager *manager, char *buffer, int width, int height) {
    if (!clipboard_has_cell(&manager->clipboard)) {
        return;
    }

    unsigned int cellX = 0;
    unsigned int cellY = 0;
    clipboard_cell(&manager->clipboard, &cellX, &cellY);

    int left = HEADER_WIDTH + (int) cellX * STRIDE_X - SHEET->originX;
    int top = HEADER_HEIGHT + (int) cellY * STRIDE_Y - SHEET->originY;
    int right = left + CELL_SIZE_X;
    int bottom = top + CELL_SIZE_Y;

    // Los lados de arriba y de abajo.
    for (int x = left; x <= right; x++) {
        put_box(buffer, width, height, x, top, BOX_DASH_H, STYLE_COPIED);
        put_box(buffer, width, height, x, bottom, BOX_DASH_H, STYLE_COPIED);
    }

    // Los lados de la izquierda y de la derecha.
    for (int y = top + 1; y < bottom; y++) {
        put_box(buffer, width, height, left, y, BOX_DASH_V, STYLE_COPIED);
        put_box(buffer, width, height, right, y, BOX_DASH_V, STYLE_COPIED);
    }
}

// Pinta la rejilla desde el origen de la vista que tenga el gestor ahora
// mismo. Se puede llamar varias veces por tecla, con origenes intermedios,
// para que el desplazamiento se vea como un deslizamiento.
static void paint_grid(CellManager *manager) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;

    int status_y = grid_bottom(manager, height);

    // Cuanto sitio tiene la rejilla en la pantalla. La ultima fila es para la
    // barra de estado, y el margen de la izquierda y la fila de arriba son para
    // los numeros de fila y las letras de columna.
    int viewWidth = width - HEADER_WIDTH;
    int viewHeight = status_y - HEADER_HEIGHT;

    if (viewWidth < STRIDE_X) viewWidth = STRIDE_X;
    if (viewHeight < STRIDE_Y) viewHeight = STRIDE_Y;

    // El bufer del fotograma se reutiliza, y solo se hace mas grande si la
    // pantalla crece.
    size_t cells_count = (size_t) width * (size_t) height;

    if (paint_buffer == NULL || paint_buffer_cells < cells_count) {
        free(paint_buffer);
        paint_buffer = (char *) malloc(cells_count * CELL_BYTES);
        paint_buffer_cells = paint_buffer != NULL ? cells_count : 0;
    }

    char *buffer = paint_buffer;
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

    // Las celdas que tocan la ventana, incluidas las que quedan cortadas en
    // los bordes.
    int firstColumn = first_cell(SHEET->originX, CELL_SIZE_X, STRIDE_X);
    int lastColumn = last_cell(SHEET->originX, viewWidth, STRIDE_X);
    int firstRow = first_cell(SHEET->originY, CELL_SIZE_Y, STRIDE_Y);
    int lastRow = last_cell(SHEET->originY, viewHeight, STRIDE_Y);

    // Las rayas verticales, una por borde de columna.
    for (int column = firstColumn; column <= lastColumn; column++) {
        int x = HEADER_WIDTH + column * STRIDE_X - SHEET->originX;

        if (x < HEADER_WIDTH) continue;

        for (int y = HEADER_HEIGHT; y < status_y; y++) {
            put_box(buffer, width, height, x, y, BOX_V, STYLE_GRID);
        }
    }

    // Las rayas horizontales, una por borde de fila.
    for (int row = firstRow; row <= lastRow; row++) {
        int y = HEADER_HEIGHT + row * STRIDE_Y - SHEET->originY;

        if (y < HEADER_HEIGHT || y >= status_y) continue;

        for (int x = HEADER_WIDTH; x < width; x++) {
            put_box(buffer, width, height, x, y, BOX_H, STYLE_GRID);
        }
    }

    // Los cruces van al final, para que tapen las rayas.
    for (int column = firstColumn; column <= lastColumn; column++) {
        int x = HEADER_WIDTH + column * STRIDE_X - SHEET->originX;

        if (x < HEADER_WIDTH) continue;

        for (int row = firstRow; row <= lastRow; row++) {
            int y = HEADER_HEIGHT + row * STRIDE_Y - SHEET->originY;

            put_box(buffer, width, height, x, y, BOX_CR, STYLE_GRID);
        }
    }

    // La celda copiada queda con un borde punteado, para saber cual es.
    draw_copied_cell(manager, buffer, width, height);

    // El bloque de la celda seleccionada, pintado antes que su contenido: asi
    // lo que se ve dentro es un espacio con el estilo de la seleccion cuando la
    // celda esta vacia, que es como se nota que esa es.
    int highlight_x = HEADER_WIDTH + (int) SHEET->cursor.x * STRIDE_X + 1 - SHEET->originX;
    int highlight_y = HEADER_HEIGHT + (int) SHEET->cursor.y * STRIDE_Y + 1 - SHEET->originY;

    for (int row = 0; row < CELL_SIZE_Y; row++) {
        for (int column = 0; column < CELL_SIZE_X; column++) {
            put_blank(buffer, width, height, highlight_x + column, highlight_y + row, STYLE_CURSOR);
        }
    }

    for (int column = firstColumn; column <= lastColumn; column++) {
        char *name = ref_column_name((unsigned int) column);
        int length = (int) strlen(name);

        for (int i = 0; i < length; i++) {
            int x = HEADER_WIDTH + column * STRIDE_X + 1 - SHEET->originX + i;

            // Lo que se sale por la izquierda no se ve: el margen es de los
            // numeros de fila.
            if (x < HEADER_WIDTH) continue;

            put_char(buffer, width, height, x, HEADER_HEIGHT - 1, name + i, 1, STYLE_HEADER);
        }
        free(name);
    }

    for (int row = firstRow; row <= lastRow; row++) {
        char label[16];
        sprintf(label, "%u", (unsigned int) (row + 1));
        int length = (int) strlen(label);

        int y = HEADER_HEIGHT + row * STRIDE_Y + 1 - SHEET->originY;

        // El numero se queda arriba en cuanto asoma la fila, para que se pueda
        // leer aunque la celda este cortada.
        if (y < HEADER_HEIGHT) y = HEADER_HEIGHT;
        if (y >= status_y) continue;

        for (int i = 0; i < length; i++) {
            // Alineado a la derecha, dejando libre la columna del separador.
            int x = HEADER_WIDTH - 2 - (length - 1 - i);
            put_char(buffer, width, height, x, y, label + i, 1, STYLE_HEADER);
        }
    }

    // Donde queda el cursor de texto, en coordenadas de la pantalla.
    int text_cursor_x = -1;
    int text_cursor_y = -1;

    for (int row = firstRow; row <= lastRow; row++) {
        for (int column = firstColumn; column <= lastColumn; column++) {
            unsigned int x = (unsigned int) column;
            unsigned int y = (unsigned int) row;

            MetaCell *meta = get_cell(manager, x, y);
            if (meta == NULL) {
                continue;
            }

            bool is_cursor = SHEET->editing && x == SHEET->cursor.x && y == SHEET->cursor.y;
            bool is_selected = x == SHEET->cursor.x && y == SHEET->cursor.y;

            const char *content = cell_content(meta);
            // La celda del cursor enseña su fuente y no su valor: asi se ve la
            // formula que tiene, y no solo el resultado. Mientras se edita, lo
            // que se ve es siempre lo que se esta escribiendo.
            if (manager->showValues && !is_cursor && !is_selected) {
                content = cell_value(meta);
            }

            // La celda seleccionada lleva el estilo de la seleccion, para que
            // el bloque se vea entero; lo demas es fuente o valor, segun lo que
            // se muestre. Encima de eso manda su formato, si lo tiene.
            int style = STYLE_TEXT;

            if (is_selected) {
                style = STYLE_CURSOR;
            } else if (manager->showValues) {
                style = STYLE_VALUE;
            }

            CellFormat format = cell_format(meta);

            style = format_style(format, style);

            if (content[0] == '\0') {
                continue;
            }

            if (content[0] == '\0') {
                continue;
            }

            // Donde empieza el contenido de la celda en la pantalla. Puede
            // estar cortado por el borde de la ventana.
            int base_x = HEADER_WIDTH + column * STRIDE_X + 1 - SHEET->originX;
            int base_y = HEADER_HEIGHT + row * STRIDE_Y + 1 - SHEET->originY;

            // Si el texto no cabe, se ve la parte donde esta el cursor. Lo que
            // cabe en la celda se mira como un numero sin signo, que es lo que
            // son los caracteres contados.
            size_t total = char_count(content);
            size_t cursor_char = is_cursor ? char_index(content, SHEET->editOffset) : 0;
            size_t capacity = (size_t) CELL_CAPACITY;

            size_t start = 0;
            if (total > capacity) {
                start = cursor_char >= capacity ? cursor_char - capacity + 1 : 0;

                size_t max_start = total - capacity;
                if (start > max_start) start = max_start;
            }

            // Cada caracter ocupa una celda, no un byte. Cuantos se ven, para
            // poder alinear cada linea.
            size_t visible = total > start ? total - start : 0;

            if (visible > capacity) {
                visible = capacity;
            }

            size_t index = 0;
            int used = 0;
            int skipped = 0;
            int pad = 0;

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

                // Al empezar linea se mira cuanto sobra en ella, que es lo que
                // manda la alineacion.
                if (used % CELL_SIZE_X == 0) {
                    int lineLength = (int) (visible - (size_t) used);

                    if (lineLength > CELL_SIZE_X) {
                        lineLength = CELL_SIZE_X;
                    }

                    pad = align_pad(format.align, content, lineLength);
                }

                int px = base_x + pad + (used % CELL_SIZE_X);
                int py = base_y + (used / CELL_SIZE_X);

                if (py >= status_y) {
                    break;
                }

                // Lo que se sale por arriba o por la izquierda se corta: el
                // margen y la fila de las letras no se pisan.
                if (px >= HEADER_WIDTH && py >= HEADER_HEIGHT) {
                    put_char(buffer, width, height, px, py, content + index, size, style);
                }

                index += size;
                used++;
            }

            if (is_cursor) {
                int position = (int) cursor_char - (int) start;

                if (position < 0) position = 0;
                if (position >= CELL_CAPACITY) position = CELL_CAPACITY - 1;

                // El hueco del cursor de texto se alinea como la linea en la
                // que cae.
                int line = position / CELL_SIZE_X;
                int lineLength = (int) (visible > (size_t) (line * CELL_SIZE_X) ? visible - (size_t) (line * CELL_SIZE_X) : 0);

                if (lineLength > CELL_SIZE_X) {
                    lineLength = CELL_SIZE_X;
                }

                text_cursor_x = base_x + align_pad(format.align, content, lineLength) + (position % CELL_SIZE_X);
                text_cursor_y = base_y + line;
            }
        }
    }

    // El hueco del cursor de texto: la celda donde entra lo que se escriba se
    // queda sin el estilo de la seleccion, para que se vea donde esta dentro del
    // bloque.
    if (text_cursor_x >= 0 && text_cursor_y >= 0 && text_cursor_y < status_y) {
        char *cell = cell_at(buffer, width, text_cursor_x, text_cursor_y);
        cell[GLYPH_BYTES] = (char) (unsigned char) STYLE_NONE;
    }

    // Cuando cambia el tamano de la terminal hay que redibujar todo.
    bool full = !previous_ready || previous_width != width || previous_height != height;

    if (full) {
        free_previous();
        clear_screen();

        // La pantalla se limpio entera, asi que las barras tambien se tienen
        // que redibujar.
        previous_status_ready = false;
        previous_sheets_ready = false;
        previous_tools_width = -1;
    }

    for (int y = 0; y < status_y; y++) {
        const char *row = buffer + (size_t) y * (size_t) width * CELL_BYTES;
        const char *old_row = previous_ready ? previous_buffer + (size_t) y * (size_t) previous_width * CELL_BYTES : NULL;

        // Las columnas que cambian de caracter o de estilo.
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

        if (to > from) {
            print_row(y, row, from, to);
        }
    }

    // Se guarda el fotograma para la proxima vez.
    if (!previous_ready || previous_width != width || previous_height != height) {
        free(previous_buffer);
        previous_buffer = (char *) malloc(cells_count * CELL_BYTES);
        if (previous_buffer == NULL) {
            previous_ready = false;
            flush();
            return;
        }

        previous_width = width;
        previous_height = height;
    }

    memcpy(previous_buffer, buffer, cells_count * CELL_BYTES);

    previous_ready = true;

    flush();
}

// Olvida el fotograma anterior, para que el proximo se pinte entero. Hace falta
// cuando cambia algo que no esta en el fotograma, como los colores: el buffer
// guarda el estilo de cada celda, no la secuencia, asi que al mudar de tema las
// celdas no cambian de bytes y el redibujado por diferencias no pintaria nada.
void invalidate_screen(void) {
    free_previous();
    previous_status_ready = false;
    previous_sheets_ready = false;

    // Las herramientas llevan colores del tema en su propia fila, asi que con un
    // tema nuevo se vuelven a escribir.
    previous_tools_width = -1;
}

// Cuantos fotogramas se pintan en medio de un desplazamiento, y cuanto se
// espera entre ellos. La terminal pinta a su ritmo, de unas sesenta veces por
// segundo: fotogramas mas juntos que eso se pierden, y costarian bytes de mas.
#define PAN_MAX_FRAMES 2
#define PAN_MAX_DISTANCE 8
#define PAN_FRAME_MICROSECONDS 9000

// Espera microsegundos, mirando el reloj. Sleep de Windows no baja de los 15
// ms, que seria demasiado para un deslizamiento.
static void pan_wait(int microseconds) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    unsigned long long until = (unsigned long long) counter.QuadPart
        + (unsigned long long) microseconds * (unsigned long long) frequency.QuadPart / 1000000ULL;

    do {
        QueryPerformanceCounter(&counter);
    } while ((unsigned long long) counter.QuadPart < until);
#else
    struct timeval wait;

    wait.tv_sec = microseconds / 1000000;
    wait.tv_usec = (microseconds % 1000000);

    select(0, NULL, NULL, NULL, &wait);
#endif
}

// Cuantos fotogramas se pintan en medio de un desplazamiento de dx columnas y
// dy filas. Cero es no deslizar: pintar de golpe. No se desliza cuando no hay
// nada que recorrer, cuando el salto es de una sola columna (ya esta en su
// sitio de una), ni cuando es tan grande que animarlo dejaria al editor detras
// de las teclas.
int pan_frames(int dx, int dy) {
    int steps = dx < 0 ? -dx : dx;

    if ((dy < 0 ? -dy : dy) > steps) {
        steps = dy < 0 ? -dy : dy;
    }

    if (steps < 2 || steps > PAN_MAX_DISTANCE) {
        return 0;
    }

    if (steps - 1 > PAN_MAX_FRAMES) {
        return PAN_MAX_FRAMES;
    }

    return steps - 1;
}
// Deja el origen de la vista donde tiene que estar para que la celda del
// cursor se vea, y devuelve cuanto se ha movido desde donde estaba.
static void pan_view(CellManager *manager, int *dx, int *dy) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;

    int viewWidth = width - HEADER_WIDTH;
    int viewHeight = grid_bottom(manager, height) - HEADER_HEIGHT;

    if (viewWidth < STRIDE_X) viewWidth = STRIDE_X;
    if (viewHeight < STRIDE_Y) viewHeight = STRIDE_Y;

    int oldX = SHEET->originX;
    int oldY = SHEET->originY;

    update_view(manager, viewWidth, viewHeight);

    *dx = SHEET->originX - oldX;
    *dy = SHEET->originY - oldY;

    SHEET->originX = oldX;
    SHEET->originY = oldY;
}

// Que hoja esta en la columna x de la fila de las pestanas, o -1 si la columna
// no cae en ninguna. Recorre la fila igual que la pinta print_sheets, para que
// lo que se clica sea lo que se ve.
int sheet_at_column(CellManager *manager, int x) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int sheets = sheet_count(manager);

    if (x < 0 || x >= width || sheets == 0) {
        return -1;
    }

    int *sizes = (int *) malloc((size_t) sheets * sizeof(int));

    if (sizes == NULL) {
        return -1;
    }

    for (int i = 0; i < sheets; i++) {
        const char *name = sheet_name_at(manager, i);

        // Con el Ctrl+F, la pestana que se esta viendo lleva lo que se escribe, y
        // ahi es donde se clica.
        if (manager->renaming && i == manager->active) {
            name = manager->renameText;
        }

        sizes[i] = utf8_columns(name);
    }

    int first = 0;
    int last = sheets;

    sheet_bar_range(sizes, sheets, manager->active, width, &first, &last);

    int used = 0;

    if (first > 0) {
        used = 1;
    }

    int found = -1;

    for (int i = first; i < last; i++) {
        if (i > first) {
            used++;
        }

        if (x >= used && x < used + sizes[i]) {
            found = i;
            break;
        }

        used += sizes[i];
    }

    free(sizes);

    return found;
}

// En que celda de la hoja cae una columna y una fila de la pantalla. Devuelve 0
// si no cae en ninguna: el margen de los numeros, la linea del separador, la
// cabecera de las letras, o lo que queda detras de la rejilla.
int cell_at_screen(CellManager *manager, int x, int y, unsigned int *cellX, unsigned int *cellY) {
    int height = manager->size.y < 6 ? 6 : manager->size.y;

    // El margen de la izquierda, la cabecera de arriba y las filas de abajo no
    // son celdas: el desplazamiento de la vista no cuenta con ellos.
    if (x < HEADER_WIDTH || y < HEADER_HEIGHT || y >= grid_bottom(manager, height)) {
        return 0;
    }

    // La primera columna de una celda esta detras del separador, y lo mismo pasa
    // con las filas: se resta ese separador antes de repartir.
    int column = x - HEADER_WIDTH + SHEET->originX - 1;
    int row = y - HEADER_HEIGHT + SHEET->originY - 1;

    if (column < 0 || row < 0) {
        return 0;
    }

    // Dentro de cada bloque de STRIDE, los primeros CELL_SIZE son la celda y el
    // ultimo es la raya de la rejilla.
    if (column % STRIDE_X >= CELL_SIZE_X || row % STRIDE_Y >= CELL_SIZE_Y) {
        return 0;
    }

    if (cellX != NULL) {
        *cellX = (unsigned int) (column / STRIDE_X);
    }

    if (cellY != NULL) {
        *cellY = (unsigned int) (row / STRIDE_Y);
    }

    return 1;
}

void print_cell_manager(CellManager *manager) {
    int dx = 0;
    int dy = 0;

    pan_view(manager, &dx, &dy);

    // Un desplazamiento de varias columnas se pinta poco a poco: en los
    // fotogramas del medio se ven las celdas cortadas por los bordes, que es lo
    // que hace que se note el deslizamiento.
    int frames = pan_frames(dx, dy);

    if (frames > 0) {
        int oldX = SHEET->originX;
        int oldY = SHEET->originY;

        for (int frame = 1; frame <= frames; frame++) {
            SHEET->originX = oldX + (int) ((long long) dx * frame / (frames + 1));
            SHEET->originY = oldY + (int) ((long long) dy * frame / (frames + 1));

            paint_grid(manager);
            pan_wait(PAN_FRAME_MICROSECONDS);
        }
    }

    SHEET->originX += dx;
    SHEET->originY += dy;

    paint_grid(manager);
}

// Cuantas pestañas se ven, y cuales. `sizes` son las columnas que pesa el nombre
// de cada hoja, y `width` lo que mide la pantalla. La hoja que se esta viendo
// entra siempre, y a su alrededor se ponen las demas hasta llenar la fila: asi,
// con muchas hojas, la que interesa nunca se cae por el borde.
void sheet_bar_range(const int *sizes, int count, int active, int width, int *first, int *last) {
    // Se deja sitio para la raya entre pestañas y para el aviso de las que se
    // quedan fuera.
    int budget = width - 2;

    if (count <= 0) {
        *first = 0;
        *last = 0;

        return;
    }

    if (active < 0) active = 0;
    if (active >= count) active = count - 1;

    int from = active;
    int to = active + 1;
    int room = sizes[active];

    for (;;) {
        bool grew = false;

        if (from > 0 && room + sizes[from - 1] + 1 <= budget) {
            from--;
            room += sizes[from] + 1;
            grew = true;
        }

        if (to < count && room + sizes[to] + 1 <= budget) {
            room += sizes[to] + 1;
            to++;
            grew = true;
        }

        if (!grew) {
            break;
        }
    }

    *first = from;
    *last = to;
}

// Las pestañas de las hojas, en su fila, encima de la barra de estado. La que
// se esta viendo va marcada, como la celda seleccionada.
void print_sheets(CellManager *manager) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;
    int y = sheet_row(height);

    // Las hojas unidas, para poder comparar con la linea de antes.
    char joined[MESSAGE_SIZE * 4];
    joined[0] = '\0';

    for (int i = 0; i < sheet_count(manager); i++) {
        size_t length = strlen(joined);

        if (length + SHEET_NAME_SIZE + 4 >= sizeof(joined)) {
            break;
        }

        snprintf(joined + length, sizeof(joined) - length, "%s%s", i == 0 ? "" : "|", sheet_name_at(manager, i));
    }

    if (manager->renaming) {
        size_t length = strlen(joined);

        // Lo que se esta escribiendo tambien cuenta como cambio de linea, para
        // que se vea mientras se teclea.
        snprintf(joined + length, sizeof(joined) - length, "|*%s", manager->renameText);
    }

    if (previous_sheets_ready &&
        strcmp(joined, previous_sheets) == 0 &&
        manager->active == previous_sheets_active) {
        return;
    }

    move_cursor(0, (uint16_t) y);

    int sheets = sheet_count(manager);
    int *sizes = (int *) malloc((size_t) (sheets > 0 ? sheets : 1) * sizeof(int));

    if (sizes == NULL) {
        return;
    }

    for (int i = 0; i < sheets; i++) {
        sizes[i] = utf8_columns(sheet_name_at(manager, i));
    }

    int first = 0;
    int last = sheets;

    sheet_bar_range(sizes, sheets, manager->active, width, &first, &last);

    free(sizes);

    int used = 0;

    // Aviso de que quedan hojas por la izquierda.
    if (first > 0) {
        fputs(style_sequence(STYLE_GRID), stdout);
        fputc('<', stdout);
        used++;
    }

    for (int i = first; i < last; i++) {
        const char *name = sheet_name_at(manager, i);
        char typing[SHEET_NAME_SIZE];

        // Con el Ctrl+F, la pestana que se esta viendo muestra lo que se esta
        // escribiendo, en vez del nombre que tenia.
        if (manager->renaming && i == manager->active) {
            snprintf(typing, sizeof(typing), "%s", manager->renameText);

            if (typing[0] == '\0') {
                strcpy(typing, " ");
            }

            name = typing;
        }

        if (i > first) {
            fputs(style_sequence(STYLE_GRID), stdout);
            fputc('|', stdout);
            used++;
        }

        fputs(style_sequence(i == manager->active ? STYLE_CURSOR : STYLE_HEADER), stdout);

        write_columns(name, width - 1 - used);
        used += utf8_columns(name);

        if (used > width - 1) {
            used = width - 1;
        }
    }

    // El resto de la fila se pone en blanco, para tapar lo que hubiera de antes.
    fputs(style_sequence(STYLE_NONE), stdout);

    for (; used < width; used++) {
        fputc(' ', stdout);
    }

    snprintf(previous_sheets, sizeof(previous_sheets), "%s", joined);
    previous_sheets_active = manager->active;
    previous_sheets_ready = true;

    flush();
}

// Las herramientas, en su fila, encima de las pestañas. Cada boton es una tecla
// escrita: lo que se hace con un clic es lo mismo que con el teclado, y lo que no
// cabe en el ancho de la pantalla se queda sin pintar, no cortado.
void print_tools(CellManager *manager) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;

    if (!manager->showTools) {
        // Sin la fila, la rejilla se la queda: se redibuja entera al cambiar, asi
        // que aqui no hace falta escribir nada.
        previous_tools_width = -1;

        return;
    }

    if (previous_tools_width == width) {
        return;
    }

    previous_tools_width = width;

    int starts[TOOL_LIMIT];
    int fitted = tools_fit(width, starts);

    move_cursor(0, (uint16_t) tools_row(manager, height));

    // El nombre de la fila, y despues los botones, uno detras de otro y donde
    // dice tools_fit: el mismo sitio es el que se clica.
    fputs(style_sequence(STYLE_GRID), stdout);
    fputs(TOOLS_LABEL, stdout);

    int used = tools_label_width();

    for (int i = 0; i < fitted; i++) {
        const char *name = tool_at(i).name;

        while (used < starts[i]) {
            fputc(' ', stdout);
            used++;
        }

        // El boton es un espacio, el nombre, y otro espacio: el aire de los lados
        // también se clica, como en cualquier boton.
        fputs(style_sequence(STYLE_GRID), stdout);
        fputc(' ', stdout);

        fputs(style_sequence(STYLE_HEADER), stdout);
        fputs(name, stdout);

        fputs(style_sequence(STYLE_GRID), stdout);
        fputc(' ', stdout);

        used += (int) strlen(name) + 2;
    }

    // El resto de la fila, en blanco, para tapar lo que hubiera de antes.
    fputs(style_sequence(STYLE_NONE), stdout);

    for (; used < width; used++) {
        fputc(' ', stdout);
    }

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

    char *ref = ref_to_string(SHEET->cursor.x, SHEET->cursor.y);
    MetaCell *meta = get_cell(manager, SHEET->cursor.x, SHEET->cursor.y);
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
    // El tamano de las celdas, que se cambia con las teclas del corchete. El
    // tema no se pone: la barra va justa de sitio en una pantalla de ochenta
    // columnas, y el nombre se dice al cambiar.
    char size_text[16];
    snprintf(size_text, sizeof(size_text), "%dx%d", cellWidth, cellHeight);
    append_padded(status, sizeof(status), size_text, 4);
    append_text(status, sizeof(status), "  ");
    // La celda copiada, para saber que hay en el portapapeles.
    char copied[16];
    copied[0] = '\0';

    if (clipboard_has_cell(&manager->clipboard)) {
        unsigned int cellX = 0;
        unsigned int cellY = 0;
        clipboard_cell(&manager->clipboard, &cellX, &cellY);

        char *copiedRef = ref_to_string(cellX, cellY);
        snprintf(copied, sizeof(copied), "CP %s", copiedRef);
        free(copiedRef);
    }

    append_padded(status, sizeof(status), copied, 8);

    free(ref);

    // El valor o el mensaje van al final de la linea, encima de la barra.
    char extra[MESSAGE_SIZE * 2];
    extra[0] = '\0';

    // Lo que ofrece el tabulador tiene prioridad: mientras se completa, es lo
    // que interesa ver.
    char offered[MESSAGE_SIZE * 2];
    bool offering = completion_text(manager, offered, sizeof(offered)) != 0;

    if (manager->message[0] != '\0') {
        append_text(extra, sizeof(extra), manager->message);
    } else if (offering) {
        append_text(extra, sizeof(extra), offered);
    } else if (SHEET->editing) {
        // Mientras se escribe, la barra deja ver el texto entero: en la rejilla
        // solo se ve el trozo que cabe en la celda.
        append_text(extra, sizeof(extra), content);
    } else if (manager->showValues && value[0] != '\0') {
        append_text(extra, sizeof(extra), value);
    }

    // Si la barra no cambio no se redibuja. Cambia tambien cuando el nombre del
    // archivo pasa a caber o a no caber, aunque el resto sea igual.
    int offset = utf8_columns(status) + 1;
    int extraColumns = utf8_columns(extra);

    // La ruta del archivo va al final de la linea, y solo si cabe despues del
    // mensaje: el mensaje es lo que pasa, y la ruta es lo que esta. Se pone el
    // nombre, no la ruta entera, que casi nunca cabe y lo que interesa es ver
    // que hoja es la que esta abierta.
    const char *shownPath = path == NULL ? "[no file]" : path;
    const char *name = shownPath;

    for (const char *at = shownPath; *at != '\0'; at++) {
        if (*at == '/' || *at == '\\') {
            name = at + 1;
        }
    }

    int pathColumns = utf8_columns(name);
    int pathAt = width - pathColumns - 1;
    bool showPath = pathAt > offset + extraColumns && pathAt < width;

    if (previous_status_ready &&
        strcmp(status, previous_status) == 0 &&
        strcmp(extra, previous_extra) == 0 &&
        showPath == previous_path_shown) {
        return;
    }

    move_cursor(0, (uint16_t) y);
    fputs(style_sequence(STYLE_STATUS), stdout);
    write_columns(status, width);

    // Un error se pinta como un error, un aviso como un aviso, y el valor de la
    // celda como los valores de la rejilla.
    ScreenStyle extraStyle = STYLE_VALUE;

    if (manager->message[0] != '\0') {
        extraStyle = manager->messageIsError ? STYLE_ERROR : STYLE_MESSAGE;
    } else if (offering || SHEET->editing) {
        extraStyle = STYLE_MESSAGE;
    }

    if (offset < width) {
        // El valor o el mensaje van despues de los campos.
        int was = utf8_columns(previous_extra);

        move_cursor((uint16_t) offset, (uint16_t) y);
        fputs(style_sequence(extraStyle), stdout);
        write_columns(extra, width - offset);

        for (int i = extraColumns; i < was && offset + i < width; i++) {
            fputc(' ', stdout);
        }
    }

    // El nombre del archivo, a la derecha del todo.
    if (showPath) {
        move_cursor((uint16_t) pathAt, (uint16_t) y);
        fputs(style_sequence(STYLE_STATUS), stdout);
        write_columns(name, width - pathAt);
    }

    // Se apaga el ultimo estilo, para que no se quede pegado en la siguiente
    // cosa que se escriba.
    fputs(style_sequence(STYLE_NONE), stdout);

    snprintf(previous_status, sizeof(previous_status), "%s", status);
    snprintf(previous_extra, sizeof(previous_extra), "%s", extra);
    previous_status_ready = true;
    previous_path_shown = showPath;

    flush();
}
