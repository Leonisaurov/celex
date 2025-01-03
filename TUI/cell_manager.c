#include "cell_manager.h"
#include "hashmap.h"
#include "tui.h"
#include <stdint.h>
#include <stdio.h>

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
                else
                    printf(" ");
            }
        }
    }

    flush();
}
