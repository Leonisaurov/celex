#include "cell.h"
#include <stdlib.h>
#include <string.h>

Cell new_cell() {
    Cell cell;
    cell.content = NULL;

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

    free(meta);
}

void set_cell_text(MetaCell *meta, const char *content) {
    if (meta == NULL) {
        return;
    }

    if (content == NULL) {
        content = "";
    }

    size_t length = strlen(content);
    char *buffer = (char *) realloc(meta->cell.content, length + 1);
    if (buffer == NULL) {
        return;
    }

    memcpy(buffer, content, length + 1);
    meta->cell.content = buffer;
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
