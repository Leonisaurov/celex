#include "cell_manager.h"
#include "hashmap.h"
#include "tui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "cell.h"

CellManager new_cell_manager(TermSize size) {
    CellManager manager;
    manager.size = size;
    manager.cursor = (TermSize) {
        .y = 0,
        .x = 0,
    };

    if (0 != hashmap_create(35, &manager.map)) {
        // TODO! Error Handeling
    }

    return manager;
}

void free_cell_manager(CellManager *manager) {
    hashmap_destroy(&manager->map);
}

unsigned int getLen(const char* str) {
    unsigned int i = 0;
    while(str[i] != '\0') i++;
    return i;
}

char get_cell_content(MetaCell cell, unsigned int x, unsigned int y) {
    if (cell.x > 0 && cell.y > 0) {
            return ' ';
    }

    if (cell.x > 0) {
        return 'A' + (cell.x - 1) % ('Z' - 'A' + 1);
    }

    if (cell.y > 0)
        return '1' + (cell.y - 1) % (9);

    return ' ';
}

void print_cell_raw(char c, unsigned time, TermSize position, TermSize camera) {
    if (time > CELL_SIZE_X * CELL_SIZE_Y - 1)
        return;

    move_cursor(position.x * 5 + 1 - camera.x + time % CELL_SIZE_X, position.y * 3 + 1 - camera.y + time / CELL_SIZE_X);
    putchar(c);
}

void print_cell_manager(CellManager *manager) {
    clear_screen();

    for (uint16_t y = 0; y < manager->size.y; y++) {
        for (uint16_t x = 0; x < manager->size.x; x++) {
            if ((y + manager->cursor.y) % 3 == 0) {
                if ((x + manager->cursor.x) == 0) {
                    if ((y + manager->cursor.y) == 0)
                        printf("┌");
                    else
                        printf("├");
                } else if ((x + manager->cursor.x) % 5 == 0) {
                    if ((y + manager->cursor.y == 0))
                        printf("┬");
                    else
                        printf("┼");
                } else
                    printf("─");
            } else {
                if ((x + manager->cursor.x) % 5 == 0)
                    printf("│");
                else {
                    MetaCell cell = {
                        .x = (x + manager->cursor.x) / 5,
                        .y = (y + manager->cursor.y) / 3,
                        .cell = new_cell(),
                    };
                    putchar(get_cell_content(cell, (x + manager->cursor.x) % 5, (y + manager->cursor.y) % 3));
                }
            }
        }
    }

    flush();
}
