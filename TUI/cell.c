#include "cell.h"
#include <stdlib.h>
#include <string.h>

// El tamano con el que arranca el editor: cuatro columnas y dos filas, como
// las celdas de siempre de una hoja de calculo.
int cellWidth = 4;
int cellHeight = 2;

Cell new_cell() {
    Cell cell;

    memset(&cell, 0, sizeof(cell));

    return cell;
}

void free_cell(MetaCell *meta) {
    if (meta == NULL) {
        return;
    }

    if (meta->cell.content != NULL) {
        free(meta->cell.content);
        meta->cell.content = NULL;
    }

    if (meta->cell.value != NULL) {
        free(meta->cell.value);
        meta->cell.value = NULL;
    }

    free(meta);
}

static void replace_text(char **target, const char *text) {
    if (text == NULL) {
        text = "";
    }

    size_t length = strlen(text);
    char *buffer = (char *) realloc(*target, length + 1);
    if (buffer == NULL) {
        return;
    }

    memcpy(buffer, text, length + 1);
    *target = buffer;
}

void set_cell_text(MetaCell *meta, const char *content) {
    if (meta == NULL) {
        return;
    }

    replace_text(&meta->cell.content, content);
}

void write_cell_value(MetaCell *meta, const char *value) {
    if (meta == NULL) {
        return;
    }

    replace_text(&meta->cell.value, value);
}

void insert_cell_text(MetaCell *meta, size_t offset, const char *text, size_t length) {
    if (meta == NULL || text == NULL || length == 0) {
        return;
    }

    size_t current = meta->cell.content == NULL ? 0 : strlen(meta->cell.content);

    if (offset > current) {
        offset = current;
    }

    char *buffer = (char *) realloc(meta->cell.content, current + length + 1);
    if (buffer == NULL) {
        return;
    }

    memmove(buffer + offset + length, buffer + offset, current - offset);
    memcpy(buffer + offset, text, length);
    buffer[current + length] = '\0';

    meta->cell.content = buffer;
}

void delete_cell_text(MetaCell *meta, size_t offset, size_t length) {
    if (meta == NULL || meta->cell.content == NULL || length == 0) {
        return;
    }

    size_t current = strlen(meta->cell.content);

    if (offset >= current) {
        return;
    }

    if (offset + length > current) {
        length = current - offset;
    }

    memmove(meta->cell.content + offset, meta->cell.content + offset + length, current - offset - length + 1);
}

const char *cell_content(MetaCell *meta) {
    if (meta == NULL || meta->cell.content == NULL) {
        return "";
    }

    return meta->cell.content;
}

const char *cell_value(MetaCell *meta) {
    if (meta == NULL || meta->cell.value == NULL) {
        return "";
    }

    return meta->cell.value;
}

// El formato de una celda. Una celda que no existe no tiene formato: el de
// empezar.
CellFormat cell_format(MetaCell *meta) {
    if (meta == NULL) {
        CellFormat none;

        memset(&none, 0, sizeof(none));

        return none;
    }

    return meta->cell.format;
}

void set_cell_format(MetaCell *meta, CellFormat format) {
    if (meta == NULL) {
        return;
    }

    meta->cell.format = format;
}
