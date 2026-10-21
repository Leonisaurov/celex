#include "cell_manager.h"
#include "file.h"
#include "ref.h"
#include "tui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "entry.h"

#define KEY_CTRL_S 19

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage:\n\t%s <sourceFiles.cle>\n", argv[0]);
        return -1;
    }

    const char *path = argv[1];

    TermSize size = getTerminalSize();
    CellManager manager = new_cell_manager(size);

    if (path != NULL) {
        int status = load_file(path, &manager);
        if (status == FILE_LOADED) {
            set_message(&manager, "file loaded");
        } else if (status == FILE_ERROR) {
            set_message(&manager, "cannot open file");
        }
    }

    open_screen();

    bool running = true;
    bool editing = false;

    while (running) {
        TermSize current = getTerminalSize();
        if (current.x != manager.size.x || current.y != manager.size.y) {
            manager.size = current;
        }

        print_cell_manager(&manager);
        print_status(&manager, editing ? "EDIT" : "NORMAL", path);

        int key = getkey();
        if (key < 0) {
            break;
        }

        bool saved = false;

        if (editing) {
            switch (key) {
                case KEY_ESC:
                    editing = false;
                    break;
                case KEY_ENTER:
                    editing = false;
                    if (manager.cursor.y < UINT16_MAX) manager.cursor.y++;
                    break;
                case KEY_BACKSPACE:
                    backspace_cell(&manager, manager.cursor.x, manager.cursor.y);
                    break;
                case KEY_UP:
                    editing = false;
                    if (manager.cursor.y > 0) manager.cursor.y--;
                    break;
                case KEY_DOWN:
                    editing = false;
                    if (manager.cursor.y < UINT16_MAX) manager.cursor.y++;
                    break;
                case KEY_LEFT:
                    editing = false;
                    if (manager.cursor.x > 0) manager.cursor.x--;
                    break;
                case KEY_RIGHT:
                    editing = false;
                    if (manager.cursor.x < UINT16_MAX) manager.cursor.x++;
                    break;
                default:
                    if (key >= 32 && key < 127) {
                        append_cell_character(&manager, manager.cursor.x, manager.cursor.y, (char) key);
                    }
                    break;
            }
        } else {
            switch (key) {
                case 'q':
                    running = false;
                    break;
                case 'h':
                case 'a':
                case KEY_LEFT:
                    if (manager.cursor.x > 0) manager.cursor.x--;
                    break;
                case 'l':
                case 'd':
                case KEY_RIGHT:
                    if (manager.cursor.x < UINT16_MAX) manager.cursor.x++;
                    break;
                case 'k':
                case 'w':
                case KEY_UP:
                    if (manager.cursor.y > 0) manager.cursor.y--;
                    break;
                case 'j':
                case 's':
                case KEY_DOWN:
                    if (manager.cursor.y < UINT16_MAX) manager.cursor.y++;
                    break;
                case 'i':
                case KEY_ENTER:
                    editing = true;
                    break;
                case 'x':
                case KEY_DELETE:
                    clear_cell(&manager, manager.cursor.x, manager.cursor.y);
                    break;
                case KEY_CTRL_S:
                    if (path == NULL) {
                        set_message(&manager, "no file to save");
                    } else if (save_file(path, &manager) == FILE_LOADED) {
                        set_message(&manager, "saved");
                        saved = true;
                    } else {
                        set_message(&manager, "cannot write file");
                    }
                    break;
                default:
                    if (key >= 32 && key < 127) {
                        editing = true;
                        clear_cell(&manager, manager.cursor.x, manager.cursor.y);
                        append_cell_character(&manager, manager.cursor.x, manager.cursor.y, (char) key);
                    }
                    break;
            }
        }

        if (!saved) {
            set_message(&manager, "");
        }
    }

    close_screen();

    free_cell_manager(&manager);
    return 0;
}
