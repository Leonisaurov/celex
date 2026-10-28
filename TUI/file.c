#include "file.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "document.h"
#include "cell.h"
#include "ref.h"

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n')) {
        text[length - 1] = '\0';
        length--;
    }

    return text;
}

// El archivo entero, en un texto. Quien lo recibe lo suelta. Si no puede abrirlo,
// dice si es que no existe o es que no se puede leer.
static char *read_all(const char *path, int *missing) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (missing != NULL) {
            *missing = errno == ENOENT;
        }

        return NULL;
    }

    size_t capacity = 4096;
    size_t length = 0;
    char *text = (char *) malloc(capacity);

    if (text == NULL) {
        fclose(file);
        return NULL;
    }

    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        size_t size = strlen(buffer);

        if (length + size + 1 > capacity) {
            while (length + size + 1 > capacity) {
                capacity *= 2;
            }

            char *grown = (char *) realloc(text, capacity);
            if (grown == NULL) {
                free(text);
                fclose(file);
                return NULL;
            }

            text = grown;
        }

        memcpy(text + length, buffer, size + 1);
        length += size;
    }

    fclose(file);

    return text;
}

// El nombre del archivo, sin camino ni extension, para bautizar la hoja que se
// importa de un guion.
static void file_stem(const char *path, char *into, size_t size) {
    const char *name = path;

    for (const char *at = path; *at != '\0'; at++) {
        if (*at == '/' || *at == '\\') {
            name = at + 1;
        }
    }

    const char *dot = strrchr(name, '.');
    size_t length = dot != NULL ? (size_t) (dot - name) : strlen(name);

    if (length >= size) {
        length = size - 1;
    }

    memcpy(into, name, length);
    into[length] = '\0';
}

// El guion del lenguaje, linea por linea: cada asignacion a una referencia es el
// fuente de una celda. Lo demas se ignora y se cuenta, porque un guion puede ser
// un programa con print, if o for, y eso no son celdas.
static int load_script(char *text, CellManager *manager, const char *path, int *ignored) {
    int skipped = 0;

    // La hoja se llama como el archivo, para reconocerla en las pestanas.
    char name[SHEET_NAME_SIZE];
    file_stem(path, name, sizeof(name));

    if (name[0] != '\0') {
        sheet_rename(manager, 0, name);
    }

    char *line = text;

    while (line != NULL) {
        char *next = strchr(line, '\n');

        if (next != NULL) {
            *next = '\0';
            next++;
        }

        char *trimmed = trim(line);

        if (trimmed[0] != '\0') {
            char *separator = strchr(trimmed, '=');

            if (separator == NULL) {
                skipped++;
            } else {
                *separator = '\0';
                char *reference = trim(trimmed);
                char *value = trim(separator + 1);

                unsigned int x = 0;
                unsigned int y = 0;

                // Una asignacion a una variable no es una celda: el guion puede
                // tener su programa, y aqui solo se importan las celdas.
                if (!ref_parse(reference, &x, &y)) {
                    skipped++;
                } else {
                    set_cell_content(manager, x, y, value);
                }
            }
        }

        line = next;
    }

    if (ignored != NULL) {
        *ignored = skipped;
    }

    return FILE_LOADED;
}

int load_file(const char *path, CellManager *manager, int *ignored) {
    if (ignored != NULL) {
        *ignored = 0;
    }

    int missing = 0;
    char *text = read_all(path, &missing);

    if (text == NULL) {
        // Si es que no existe, hoja nueva. Si existe pero no se puede leer, un
        // error de verdad: no quiero que se pise un archivo que si esta ahi.
        return missing ? FILE_NOT_FOUND : FILE_ERROR;
    }

    // El documento del editor trae su cabecera. Si no la tiene, es un guion del
    // lenguaje, y se importa como una sola hoja.
    int skipped = 0;

    if (document_load(text, manager, &skipped)) {
        free(text);

        if (ignored != NULL) {
            *ignored = skipped;
        }

        return FILE_LOADED;
    }

    // De un guion se importa una hoja: lo que hubiera en el editor no tiene nada
    // que ver con el archivo que se esta abriendo.
    workbook_clear(manager);

    int status = load_script(text, manager, path, &skipped);

    free(text);

    if (ignored != NULL) {
        *ignored = skipped;
    }

    return status;
}

typedef struct {
    unsigned int x;
    unsigned int y;
    const char *content;
} FileEntry;

typedef struct {
    FileEntry *items;
    size_t count;
    size_t capacity;
} FileEntries;

static int compare_entries(const void *a, const void *b) {
    const FileEntry *left = (const FileEntry *) a;
    const FileEntry *right = (const FileEntry *) b;

    // Columna y luego fila: el lenguaje no tiene referencias adelantadas,
    // asi que una celda tiene que venir despues de las celdas de las que
    // depende. Con este orden, una celda ve a las que estan encima en su
    // misma columna y a las de las columnas de la izquierda.
    if (left->x != right->x) {
        return left->x < right->x ? -1 : 1;
    }

    if (left->y != right->y) {
        return left->y < right->y ? -1 : 1;
    }

    return 0;
}

static int collect_entry(void *const context, struct hashmap_element_s *const element) {
    FileEntries *entries = (FileEntries *) context;
    MetaCell *meta = (MetaCell *) element->data;

    if (entries->count == entries->capacity) {
        size_t capacity = entries->capacity == 0 ? 16 : entries->capacity * 2;
        FileEntry *items = (FileEntry *) realloc(entries->items, capacity * sizeof(FileEntry));
        if (items == NULL) {
            return -1;
        }

        entries->items = items;
        entries->capacity = capacity;
    }

    entries->items[entries->count].x = meta->x;
    entries->items[entries->count].y = meta->y;
    entries->items[entries->count].content = cell_content(meta);
    entries->count++;

    return 0;
}

char *sheet_source(CellManager *manager) {
    FileEntries entries = {NULL, 0, 0};
    hashmap_iterate_pairs(&sheet_state(manager)->map, collect_entry, &entries);

    qsort(entries.items, entries.count, sizeof(FileEntry), compare_entries);

    size_t length = 1;
    for (size_t i = 0; i < entries.count; i++) {
        // Una celda sin contenido no necesita una linea en el archivo.
        if (entries.items[i].content[0] == '\0') {
            continue;
        }

        length += strlen(entries.items[i].content) + 16;
    }

    char *source = (char *) malloc(length);
    if (source == NULL) {
        free(entries.items);
        return NULL;
    }

    source[0] = '\0';
    for (size_t i = 0; i < entries.count; i++) {
        if (entries.items[i].content[0] == '\0') {
            continue;
        }

        char *reference = ref_to_string(entries.items[i].x, entries.items[i].y);
        strcat(source, reference);
        strcat(source, " = ");
        strcat(source, entries.items[i].content);
        strcat(source, "\n");
        free(reference);
    }

    free(entries.items);

    return source;
}

static int write_text(const char *path, char *text) {
    if (text == NULL) {
        return FILE_ERROR;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        free(text);
        return FILE_ERROR;
    }

    fputs(text, file);
    fclose(file);
    free(text);

    return FILE_LOADED;
}

// El documento del editor: la pantalla y todas las hojas, en el formato .clex.
int save_document(const char *path, CellManager *manager) {
    return write_text(path, document_source(manager));
}

// La hoja que se esta viendo como guion del lenguaje: sin colores, sin nombres de
// hoja y sin nada que no sea celex script, para poder editarlo a mano o correrlo
// con el interprete.
int save_script(const char *path, CellManager *manager) {
    return write_text(path, sheet_source(manager));
}
