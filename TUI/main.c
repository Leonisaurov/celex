#include "cell_manager.h"
#include "file.h"
#include "keys.h"
#include "tui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n\t%s <sourceFiles.cle>\n", argv[0]);
        return -1;
    }

    const char *path = argv[1];

    TermSize size = getTerminalSize();
    CellManager manager = new_cell_manager(size);

    if (path != NULL) {
        int ignored = 0;
        int status = load_file(path, &manager, &ignored);
        if (status == FILE_LOADED) {
            char message[MESSAGE_SIZE];
            snprintf(message, MESSAGE_SIZE, "file loaded%s", ignored > 0 ? " (some lines ignored)" : "");
            set_message(&manager, message);
        } else if (status == FILE_ERROR) {
            set_message(&manager, "cannot open file");
        } else {
            set_message(&manager, "new sheet");
        }
    }

    Editor editor = {
        .path = path,
        .running = true,
        .quitting = false,
        .reloading = false,
    };

    open_screen();

    while (editor.running) {
        TermSize current = getTerminalSize();
        if (current.x != manager.size.x || current.y != manager.size.y) {
            manager.size = current;
        }

        print_cell_manager(&manager);
        print_status(&manager, manager.editing ? "EDIT" : "NORMAL", path);

        int key = getkey();
        if (key < 0) {
            break;
        }

        handle_key(&editor, &manager, key);
    }

    close_screen();

    free_cell_manager(&manager);
    return 0;
}
