#include "cell_manager.h"
#include "file.h"
#include "ref.h"
#include "script.h"
#include "tui.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "entry.h"

#define KEY_CTRL_S 19
#define KEY_CTRL_R 18
#define KEY_CTRL_Y 25
#define KEY_CTRL_Z 26
#define KEY_CTRL_C 3
#define KEY_CTRL_V 22

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

    open_screen();

    bool running = true;
    bool editing = false;
    bool quitting = false;

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

        // Cualquier otra tecla cancela la confirmacion de salida.
        if (key != 'q') {
            quitting = false;
        }

        bool keepMessage = false;

        if (key == KEY_CTRL_Z || key == KEY_CTRL_Y) {
            // Deshacer y rehacer funcionan tambien mientras se edita.
            if (key == KEY_CTRL_Z) {
                if (can_undo(&manager)) {
                    undo_edit(&manager);
                    set_message(&manager, "undone");
                } else {
                    set_message(&manager, "nothing to undo");
                }
            } else {
                if (can_redo(&manager)) {
                    redo_edit(&manager);
                    set_message(&manager, "redone");
                } else {
                    set_message(&manager, "nothing to redo");
                }
            }

            keepMessage = true;
        } else if (key == KEY_CTRL_C) {
            copy_cell_content(&manager, manager.cursor.x, manager.cursor.y);
            set_message(&manager, "copied");
            keepMessage = true;
        } else if (key == KEY_CTRL_V) {
            if (has_clipboard()) {
                paste_cell_content(&manager, manager.cursor.x, manager.cursor.y);
                set_message(&manager, "pasted");
            } else {
                set_message(&manager, "the clipboard is empty");
            }

            keepMessage = true;
        } else if (editing) {
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
                    // Si hay cambios sin guardar se pide confirmacion.
                    if (manager.dirty && !quitting) {
                        quitting = true;
                        set_message(&manager, "unsaved changes: press q again to quit");
                        keepMessage = true;
                    } else {
                        running = false;
                    }
                    break;
                case KEY_CTRL_C:
                    copy_cell_content(&manager, manager.cursor.x, manager.cursor.y);
                    set_message(&manager, "copied");
                    keepMessage = true;
                    break;
                case KEY_CTRL_V:
                    if (has_clipboard()) {
                        paste_cell_content(&manager, manager.cursor.x, manager.cursor.y);
                        set_message(&manager, "pasted");
                        keepMessage = true;
                    } else {
                        set_message(&manager, "the clipboard is empty");
                        keepMessage = true;
                    }
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
                case 'v':
                    manager.showValues = !manager.showValues;
                    break;
                case KEY_CTRL_R: {
                    char message[MESSAGE_SIZE];
                    int status = run_script(&manager, message, MESSAGE_SIZE);
                    if (status == SCRIPT_OK) {
                        manager.showValues = true;
                    }
                    set_message(&manager, message);
                    keepMessage = true;
                    break;
                }
                case KEY_CTRL_S:
                    if (path == NULL) {
                        set_message(&manager, "no file to save");
                    } else if (save_file(path, &manager) == FILE_LOADED) {
                        set_message(&manager, "saved");
                        manager.dirty = false;
                        keepMessage = true;
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

        if (!keepMessage) {
            set_message(&manager, "");
        }
    }

    close_screen();

    free_cell_manager(&manager);
    return 0;
}
