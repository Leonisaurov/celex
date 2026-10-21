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

#define HEADER_WIDTH 4
#define HEADER_HEIGHT 1
#define STRIDE_X (CELL_SIZE_X + 1)
#define STRIDE_Y (CELL_SIZE_Y + 1)
#define STATUS_HEIGHT 1

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

static void put_box(char *buffer, int width, int x, int y, const char *glyph) {
    if (x < 0 || x + 3 > width) {
        return;
    }

    memcpy(buffer + y * width + x, glyph, 3);
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

    set_cell_text(meta, "");
}

void append_cell_character(CellManager *manager, unsigned int x, unsigned int y, char character) {
    MetaCell *meta = get_cell(manager, x, y);

    if (meta == NULL) {
        set_cell_content(manager, x, y, "");
        meta = get_cell(manager, x, y);
    }

    append_cell_content(meta, character);
}

void backspace_cell(CellManager *manager, unsigned int x, unsigned int y) {
    MetaCell *meta = get_cell(manager, x, y);

    if (meta == NULL) {
        return;
    }

    backspace_cell_content(meta);
}

void set_message(CellManager *manager, const char *message) {
    if (message == NULL) {
        manager->message[0] = '\0';
        return;
    }

    snprintf(manager->message, MESSAGE_SIZE, "%s", message);
}

static void update_camera(CellManager *manager, int columns, int rows) {
    if ((int) manager->cursor.x < manager->camera.x) {
        manager->camera.x = manager->cursor.x;
    }
    if ((int) manager->cursor.x >= (int) manager->camera.x + columns) {
        manager->camera.x = manager->cursor.x - columns + 1;
    }
    if ((int) manager->cursor.y < manager->camera.y) {
        manager->camera.y = manager->cursor.y;
    }
    if ((int) manager->cursor.y >= (int) manager->camera.y + rows) {
        manager->camera.y = manager->cursor.y - rows + 1;
    }
}

static void print_row(int y, const char *buffer, int length, int highlight_start, int highlight_end) {
    move_cursor(0, (uint16_t) y);

    if (highlight_start < 0) {
        fwrite(buffer, 1, (size_t) length, stdout);
        return;
    }

    fwrite(buffer, 1, (size_t) highlight_start, stdout);
    printf("\x1b[7m");
    fwrite(buffer + highlight_start, 1, (size_t) (highlight_end - highlight_start), stdout);
    printf("\x1b[0m");
    fwrite(buffer + highlight_end, 1, (size_t) (length - highlight_end), stdout);
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

    char *buffer = (char *) malloc((size_t) width * (size_t) height);
    if (buffer == NULL) {
        return;
    }
    memset(buffer, ' ', (size_t) width * (size_t) height);

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
            if (x < width) {
                buffer[x] = name[i];
            }
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
            if (x >= 0 && x < width && y < status_y) {
                buffer[y * width + x] = label[i];
            }
        }
    }

    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            unsigned int x = (unsigned int) (manager->camera.x + column);
            unsigned int y = (unsigned int) (manager->camera.y + row);

            MetaCell *meta = get_cell(manager, x, y);
            if (meta == NULL) {
                continue;
            }

            const char *content = cell_content(meta);
            if (manager->showValues) {
                content = cell_value(meta);
            }

            if (content[0] == '\0') {
                continue;
            }

            int base_x = HEADER_WIDTH + column * STRIDE_X + 1;
            int base_y = HEADER_HEIGHT + row * STRIDE_Y + 1;
            size_t length = strlen(content);
            size_t offset = length > CELL_CAPACITY ? length - CELL_CAPACITY : 0;

            for (int i = 0; content[offset + i] != '\0'; i++) {
                int px = base_x + (i % CELL_SIZE_X);
                int py = base_y + (i / CELL_SIZE_X);

                if (px >= width || py >= status_y) {
                    continue;
                }

                buffer[py * width + px] = content[offset + i];
            }
        }
    }

    int cursor_column = (int) manager->cursor.x - manager->camera.x;
    int cursor_row = (int) manager->cursor.y - manager->camera.y;
    int highlight_x = HEADER_WIDTH + cursor_column * STRIDE_X + 1;
    int highlight_y = HEADER_HEIGHT + cursor_row * STRIDE_Y + 1;

    clear_screen();

    for (int y = 0; y < status_y; y++) {
        int start = -1;
        int end = -1;

        if (y >= highlight_y && y < highlight_y + CELL_SIZE_Y) {
            start = highlight_x;
            end = highlight_x + CELL_SIZE_X;
            if (start < 0) start = 0;
            if (end > width) end = width;
            if (start > width) start = width;
            if (end < start) end = start;
        }

        print_row(y, buffer + y * width, width, start, end);
    }

    free(buffer);
    flush();
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
    snprintf(status, sizeof(status), " %s  %-6s  %-16s  %s", ref, mode, content, path == NULL ? "[no file]" : path);
    free(ref);

    if (manager->showValues && value[0] != '\0') {
        int offset = (int) strlen(status) + 1;
        if (offset < width) {
            move_cursor((uint16_t) offset, (uint16_t) y);
            fwrite(value, 1, strlen(value) < (size_t) (width - offset) ? strlen(value) : (size_t) (width - offset), stdout);
        }
    }

    move_cursor(0, (uint16_t) y);
    printf("\x1b[7m");
    fwrite(status, 1, strlen(status) < (size_t) width ? strlen(status) : (size_t) width, stdout);
    printf("\x1b[0m");

    if (manager->message[0] != '\0') {
        int offset = (int) strlen(status) + 1;
        if (offset < width) {
            move_cursor((uint16_t) offset, (uint16_t) y);
            fwrite(manager->message, 1, strlen(manager->message) < (size_t) (width - offset) ? strlen(manager->message) : (size_t) (width - offset), stdout);
        }
    }

    flush();
}
