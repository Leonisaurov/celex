#include "cell_manager.h"
#include "document.h"
#include "file.h"
#include "keys.h"
#include "tui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// El camino donde guarda el editor. Un guion del lenguaje se abre para mirarlo y
// para trabajar en el, pero lo que escribe el editor es su documento, al lado:
// asi el .cle queda limpio y manda quien lo edita a mano.
static void save_path_for(const char *path, char *into, size_t size) {
    into[0] = '\0';

    if (path == NULL) {
        return;
    }

    if (document_is_path(path)) {
        snprintf(into, size, "%s", path);
        return;
    }

    document_change_extension(path, ".clex", into, size);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n\t%s <sourceFiles.cle|documentFiles.clex>\n", argv[0]);
        return -1;
    }

    const char *path = argv[1];

    TermSize size = getTerminalSize();
    CellManager manager = new_cell_manager(size);

    char savePath[1024];
    save_path_for(path, savePath, sizeof(savePath));

    if (path != NULL) {
        int ignored = 0;
        int status = load_file(path, &manager, &ignored);
        char message[MESSAGE_SIZE];

        if (status == FILE_LOADED) {
            // Un documento se abre tal cual; un guion se importa, y se guarda al
            // lado, en el formato del editor.
            if (ignored > 0) {
                snprintf(message, sizeof(message), "loaded (%d lines ignored)", ignored);
            } else if (strcmp(path, savePath) != 0) {
                snprintf(message, sizeof(message), "imported: saves as %s", savePath);
            } else {
                snprintf(message, sizeof(message), "file loaded");
            }

            set_message(&manager, message);
        } else if (status == FILE_ERROR) {
            set_error_message(&manager, "cannot open file");
        } else {
            set_message(&manager, "new sheet");
        }
    }

    Editor editor = {
        .path = path,
        .savePath = savePath[0] != '\0' ? savePath : NULL,
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
        print_tools(&manager);
        print_sheets(&manager);
        print_status(&manager, sheet_state(&manager)->editing ? "EDIT" : "NORMAL", editor.savePath);

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
