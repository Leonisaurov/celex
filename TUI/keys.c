#include "keys.h"

#include "clipboard.h"
#include "document.h"
#include "file.h"
#include "ref.h"
#include "script.h"
#include "theme.h"
#include "tui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool recalculate_sheet(CellManager *manager) {
    char message[MESSAGE_SIZE];

    message[0] = '\0';

    if (run_script(manager, message, MESSAGE_SIZE) == SCRIPT_OK && message[0] == '\0') {
        // Todo se pudo calcular: no hay nada que avisar.
        return false;
    }

    set_error_message(manager, message);

    return true;
}

// El texto pegado no puede tener saltos de linea: en la hoja hay una linea
// por celda.
static char *clean_paste_text(const char *text) {
    size_t length = strlen(text);
    char *clean = (char *) malloc(length + 1);

    if (clean == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        char character = text[i];

        if (character == '\r' || character == '\n' || character == '\t') {
            character = ' ';
        }

        clean[i] = character;
    }

    clean[length] = '\0';

    return clean;
}

// Pega un texto en la celda del cursor, o en el texto que se esta editando.
// Devuelve true si la hoja no se pudo calcular.
static bool paste_text(CellManager *manager, const char *text) {
    char *clean = clean_paste_text(text);
    if (clean == NULL) {
        return false;
    }

    if (sheet_state(manager)->editing) {
        // Dentro de una celda: el texto entra donde esta el cursor de texto.
        paste_cell_text(manager, sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y, clean);
        free(clean);

        return false;
    }

    // En una celda: entra la formula entera y se calcula la hoja.
    paste_cell_content(manager, sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y, clean);
    free(clean);

    return recalculate_sheet(manager);
}

bool handle_paste(CellManager *manager, const char *text) {
    if (text == NULL || text[0] == '\0') {
        return true;
    }

    // Si el texto es el mismo que ya tiene el portapapeles, el portapapeles no
    // cambia: la celda copiada sigue marcada.
    if (!clipboard_is_text(&manager->clipboard, text)) {
        clipboard_set_text(&manager->clipboard, text);
    }

    bool error = paste_text(manager, text);

    if (!error) {
        set_message(manager, "pasted");
    }

    return true;
}

// Copia la celda del cursor, o el texto que se esta editando.
static void copy(CellManager *manager) {
    MetaCell *meta = get_cell(manager, sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y);

    if (sheet_state(manager)->editing) {
        // Dentro de una celda se copia el texto, como en cualquier editor de
        // texto.
        const char *text = cell_content(meta);

        clipboard_set_text(&manager->clipboard, text);
        system_clipboard_set(text);

        set_message(manager, "copied");
        return;
    }

    // Se copia la celda: su formula y su valor. La celda queda marcada, para
    // saber cual se copio.
    const char *text = cell_content(meta);
    const char *value = cell_value(meta);

    clipboard_set_cell(&manager->clipboard, text, value, sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y);
    system_clipboard_set(text);

    char *reference = ref_to_string(sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y);
    char message[MESSAGE_SIZE];
    snprintf(message, sizeof(message), "copied %s", reference);
    free(reference);

    set_message(manager, message);
}

bool handle_key(Editor *editor, CellManager *manager, int key) {
    // Cualquier otra tecla cancela la confirmacion de salida.
    if (key != 'q') {
        editor->quitting = false;
    }

    if (key != KEY_CTRL_L) {
        editor->reloading = false;
    }

    // Mientras se escribe el nombre de la hoja, lo que se teclea va al nombre:
    // Enter lo fija y Escape lo deja como estaba. Una tecla de mando corta la
    // escritura y se hace lo que toca, para no quedarse uno escribiendo nombres
    // sin poder cambiar de hoja.
    if (manager->renaming) {
        bool writing = key == KEY_ESC || key == KEY_ENTER || key == KEY_BACKSPACE || (key >= 32 && key < 127);

        if (!writing) {
            manager->renaming = false;
            manager->renameText[0] = '\0';
        } else {
            switch (key) {
                case KEY_ESC:
                    manager->renaming = false;
                    set_message(manager, "");
                    break;
                case KEY_ENTER: {
                    char name[SHEET_NAME_SIZE];
                    snprintf(name, sizeof(name), "%s", manager->renameText);

                    manager->renaming = false;

                    if (name[0] == '\0') {
                        set_message(manager, "the name stays");
                    } else {
                        sheet_rename(manager, manager->active, name);
                        set_message(manager, name);
                    }

                    break;
                }
                case KEY_BACKSPACE: {
                    size_t length = strlen(manager->renameText);

                    if (length > 0) {
                        manager->renameText[length - 1] = '\0';
                    }

                    break;
                }
                default: {
                    size_t length = strlen(manager->renameText);

                    if (length + 1 < sizeof(manager->renameText)) {
                        manager->renameText[length] = (char) key;
                        manager->renameText[length + 1] = '\0';
                    }
                    break;
                }
            }

            return editor->running;
        }
    }

    bool keepMessage = false;

    if (key == KEY_CTRL_Z || key == KEY_CTRL_Y) {
        // Deshacer y rehacer funcionan tambien mientras se edita.
        if (key == KEY_CTRL_Z) {
            if (can_undo(manager)) {
                undo_edit(manager);
                set_message(manager, "undone");
            } else {
                set_message(manager, "nothing to undo");
            }
        } else {
            if (can_redo(manager)) {
                redo_edit(manager);
                set_message(manager, "redone");
            } else {
                set_message(manager, "nothing to redo");
            }
        }

        keepMessage = true;
        recalculate_sheet(manager);
    } else if (key == KEY_CTRL_L) {
        // Recargar el archivo, por si se cambio por fuera.
        if (editor->path == NULL) {
            set_message(manager, "no file to load");
        } else if (sheet_any_dirty(manager) && !editor->reloading) {
            editor->reloading = true;
            set_message(manager, "unsaved changes: press Ctrl+L again to reload");
        } else {
            int ignored = 0;

            // Se olvida todo el archivo, no solo la hoja que se veia.
            workbook_clear(manager);
            load_file(editor->path, manager, &ignored);

            editor->reloading = false;
            set_message(manager, "reloaded");
        }

        keepMessage = true;
    } else if (key == KEY_CTRL_C) {
        copy(manager);
        keepMessage = true;
    } else if (key == KEY_CTRL_V) {
        if (clipboard_has_content(&manager->clipboard)) {
            bool error = paste_text(manager, clipboard_text(&manager->clipboard));

            if (!error) {
                set_message(manager, "pasted");
            }
        } else {
            set_message(manager, "the clipboard is empty");
        }

        keepMessage = true;
    } else if (key == KEY_PASTE) {
        // La terminal ha pegado un texto. El portapapeles del editor se
        // queda con ese texto, para que el Ctrl+C y el Ctrl+V funcionen
        // siempre con lo mismo.
        handle_paste(manager, get_paste_text());
        keepMessage = true;
    } else if (sheet_state(manager)->editing) {
        switch (key) {
            case KEY_ESC:
                sheet_state(manager)->editing = false;
                keepMessage = recalculate_sheet(manager);
                break;
            case KEY_ENTER:
                sheet_state(manager)->editing = false;
                if (sheet_state(manager)->cursor.y < UINT16_MAX) sheet_state(manager)->cursor.y++;
                keepMessage = recalculate_sheet(manager);
                break;
            case KEY_BACKSPACE:
                edit_backspace(manager);
                break;
            case KEY_DELETE:
                edit_delete(manager);
                break;
            case KEY_HOME:
                edit_to_start(manager);
                break;
            case KEY_TAB:
                // El tabulador completa la palabra que se esta escribiendo, y
                // va pasando por las candidatas.
                complete_tab(manager);
                break;
            case KEY_END:
                edit_to_end(manager);
                break;
            case KEY_UP:
                sheet_state(manager)->editing = false;
                if (sheet_state(manager)->cursor.y > 0) sheet_state(manager)->cursor.y--;
                keepMessage = recalculate_sheet(manager);
                break;
            case KEY_DOWN:
                sheet_state(manager)->editing = false;
                if (sheet_state(manager)->cursor.y < UINT16_MAX) sheet_state(manager)->cursor.y++;
                keepMessage = recalculate_sheet(manager);
                break;
            case KEY_LEFT:
                // Las flechas mueven el cursor de texto, no la celda.
                edit_move_cursor(manager, -1);
                break;
            case KEY_RIGHT:
                edit_move_cursor(manager, 1);
                break;
            default:
                if (key >= 32 && key < 127) {
                    char character = (char) key;
                    edit_insert(manager, &character, 1);
                }
                break;
        }
    } else {
        switch (key) {
            case 'q':
                // Si hay cambios sin guardar se pide confirmacion.
                if (sheet_any_dirty(manager) && !editor->quitting) {
                    editor->quitting = true;
                    set_message(manager, "unsaved changes: press q again to quit");
                    keepMessage = true;
                } else {
                    editor->running = false;
                }
                break;
            case 'h':
            case 'a':
            case KEY_LEFT:
                if (sheet_state(manager)->cursor.x > 0) sheet_state(manager)->cursor.x--;
                break;
            case 'l':
            case 'd':
            case KEY_RIGHT:
                if (sheet_state(manager)->cursor.x < UINT16_MAX) sheet_state(manager)->cursor.x++;
                break;
            case 'k':
            case 'w':
            case KEY_UP:
                if (sheet_state(manager)->cursor.y > 0) sheet_state(manager)->cursor.y--;
                break;
            case 'j':
            case 's':
            case KEY_DOWN:
                if (sheet_state(manager)->cursor.y < UINT16_MAX) sheet_state(manager)->cursor.y++;
                break;
            case 'i':
            case KEY_ENTER:
                // Se edita el contenido que ya tiene la celda, con el
                // cursor de texto al final.
                sheet_state(manager)->editing = true;
                edit_reset(manager);
                break;
            case 'x':
            case KEY_DELETE:
                clear_cell(manager, sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y);
                keepMessage = recalculate_sheet(manager);
                break;
            case 'v':
                manager->showValues = !manager->showValues;
                break;
            case ']':
                // El tamano de las celdas, por separado el ancho y el alto,
                // como el ancho de columna y el alto de fila de una hoja de
                // calculo. Las llaves son los corchetes con mayusculas.
                set_cell_size(cellWidth + 1, cellHeight);
                break;
            case '[':
                set_cell_size(cellWidth - 1, cellHeight);
                break;
            case '}':
                set_cell_size(cellWidth, cellHeight + 1);
                break;
            case '{':
                set_cell_size(cellWidth, cellHeight - 1);
                break;
            case KEY_CTRL_T:
                // El siguiente tema de colores. La barra dice cual esta
                // puesto, para no perderlo de vista.
                set_message(manager, theme_next());

                // Los colores no estan en el fotograma: hay que volver a
                // pintarlo entero para que se noten.
                invalidate_screen();

                keepMessage = true;
                break;
            case KEY_CTRL_N:
                // Las hojas: la siguiente y la anterior, dando la vuelta.
                sheet_select(manager, (manager->active + 1) % sheet_count(manager));
                set_message(manager, sheet_name_at(manager, manager->active));
                keepMessage = true;
                break;
            case KEY_CTRL_P:
                sheet_select(manager, (manager->active + sheet_count(manager) - 1) % sheet_count(manager));
                set_message(manager, sheet_name_at(manager, manager->active));
                keepMessage = true;
                break;
            case KEY_CTRL_A:
                // Una hoja nueva detras de esta, con el nombre de encima.
                if (sheet_add(manager, NULL) >= 0) {
                    set_message(manager, sheet_name_at(manager, manager->active));
                } else {
                    set_error_message(manager, "cannot add a sheet");
                }

                keepMessage = true;
                break;
            case KEY_CTRL_D:
                if (sheet_count(manager) <= 1) {
                    set_message(manager, "the last sheet stays");
                } else {
                    sheet_remove(manager, manager->active);
                    set_message(manager, "sheet removed");
                }

                keepMessage = true;
                break;
            case KEY_CTRL_F:
                // El nombre de la hoja que se esta viendo, escrito en la propia
                // pestana. Se empieza de cero: el nombre viejo se deja atras
                // hasta que Enter lo cambie.
                manager->renaming = true;
                manager->renameText[0] = '\0';
                set_message(manager, "new name: Enter to set, Esc to leave");
                keepMessage = true;
                break;
            case KEY_CTRL_R: {
                char message[MESSAGE_SIZE];
                int status = run_script(manager, message, MESSAGE_SIZE);
                if (status == SCRIPT_OK) {
                    manager->showValues = true;

                    if (message[0] == '\0') {
                        snprintf(message, sizeof(message), "script executed");
                    }

                    set_message(manager, message);
                } else {
                    set_error_message(manager, message);
                }

                keepMessage = true;
                break;
            }
            case KEY_CTRL_S:
                if (editor->savePath == NULL) {
                    set_message(manager, "no file to save");
                } else if (save_document(editor->savePath, manager) == FILE_LOADED) {
                    set_message(manager, "saved");
                    sheet_mark_saved(manager);
                    keepMessage = true;
                } else {
                    set_error_message(manager, "cannot write file");
                    keepMessage = true;
                }
                break;
            case KEY_CTRL_E: {
                // La hoja que se esta viendo, escrita como guion del lenguaje, al
                // lado del documento. Es lo que se puede editar a mano o correr con
                // el interprete.
                if (editor->savePath == NULL) {
                    set_error_message(manager, "nothing to export");
                    keepMessage = true;
                    break;
                }

                char script[1024];

                if (!document_change_extension(editor->savePath, ".cle", script, sizeof(script))) {
                    set_error_message(manager, "the name does not fit");
                    keepMessage = true;
                    break;
                }

                if (save_script(script, manager) == FILE_LOADED) {
                    char message[MESSAGE_SIZE];
                    snprintf(message, sizeof(message), "exported %s", script);
                    set_message(manager, message);
                } else {
                    set_error_message(manager, "cannot write the script");
                }

                keepMessage = true;
                break;
            }
            default:
                if (key >= 32 && key < 127) {
                    // Una tecla cualquiera empieza a editar la celda, y
                    // reemplaza su contenido.
                    sheet_state(manager)->editing = true;
                    clear_cell(manager, sheet_state(manager)->cursor.x, sheet_state(manager)->cursor.y);

                    char character = (char) key;
                    edit_insert(manager, &character, 1);
                }
                break;
        }
    }

    if (!keepMessage) {
        set_message(manager, "");
    }

    return editor->running;
}
