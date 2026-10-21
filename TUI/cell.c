#include "cell.h"
#include <stdlib.h>
#include <string.h>

Cell new_cell() {
    Cell cell;
    cell.content = NULL;
    cell.value = NULL;

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

static void replace_text(string *target, const char *text) {
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

void append_cell_content(MetaCell *meta, char character) {
    if (meta == NULL) {
        return;
    }

    size_t length = meta->cell.content == NULL ? 0 : strlen(meta->cell.content);
    char *buffer = (char *) realloc(meta->cell.content, length + 2);
    if (buffer == NULL) {
        return;
    }

    buffer[length] = character;
    buffer[length + 1] = '\0';
    meta->cell.content = buffer;
}

void backspace_cell_content(MetaCell *meta) {
    if (meta == NULL || meta->cell.content == NULL) {
        return;
    }

    size_t length = strlen(meta->cell.content);
    if (length == 0) {
        return;
    }

    meta->cell.content[length - 1] = '\0';
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
