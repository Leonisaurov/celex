#include "cell_manager.h"
#include "tui.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n\t%s <sourceFiles.cle>\n", argv[0]);
        return -1;
    }

    TermSize term_size = getTerminalSize();
    CellManager manager = new_cell_manager(term_size);

    open_screen();

    while(true) {
        print_cell_manager(&manager);
        char c[3] = { 0 };
        int n = read(STDIN_FILENO, c, 3);
        if (n < 1) break;

        if (n == 1 && c[0] == 27) break;
        switch (c[0]) {
            case 'a':
                if (manager.cursor.x > 0)
                    manager.cursor.x -= 1;
                break;
            case 'd':
                manager.cursor.x += 1;
                break;
            case 'w':
                if (manager.cursor.y > 0)
                    manager.cursor.y -= 1;
                break;
            case 's':
                manager.cursor.y += 1;
                break;
        }
    }

    close_screen();

    free_cell_manager(&manager);
    return 0;
}
