#include "keys.h"

#include "clipboard.h"
#include "document.h"
#include "file.h"
#include "popup.h"
#include "ref.h"
#include "script.h"
#include "theme.h"
#include "tools.h"
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

// Poner y quitar la barra de las herramientas. Es un ajuste de la pantalla, como
// el tamano de las celdas, y no cuenta como un cambio del archivo. El aviso va
// corto a proposito: la barra de estado dice el nombre del archivo al final, y un
// aviso largo se lo taparia, que es justo el sitio donde se clica para volver a
// traer la barra. Lo que se ve cuando esta quitada es el chip de la fila de las
// pestanas.
static void toggle_tools(CellManager *manager) {
    manager->showMenu = !manager->showMenu;

    // Un desplegable de una barra que ya no esta es una ventana sin puerta.
    popup_cancel(&manager->popup);

    // La rejilla cambia de sitio con la barra, asi que se pinta entera de nuevo.
    invalidate_screen();

    set_message(manager, manager->showMenu ? "tools shown" : "tools off");
}

// El popup mide sobre la pantalla de verdad, con los mismos margenes que la
// rejilla.
static void popup_screen(CellManager *manager, int *width, int *gridBottom) {
    int size = manager->size.y < 6 ? 6 : manager->size.y;

    *width = manager->size.x < 20 ? 20 : manager->size.x;
    *gridBottom = grid_bottom(manager, size);
}

// La lista de los temas, uno por fila.
static void open_theme_popup(CellManager *manager) {
    const char *names[POPUP_LIMIT];
    int count = theme_count();
    int current = 0;

    if (count > POPUP_LIMIT) {
        count = POPUP_LIMIT;
    }

    for (int i = 0; i < count; i++) {
        names[i] = theme_name_at(i);

        if (strcmp(names[i], theme_name()) == 0) {
            current = i;
        }
    }

    int width;
    int gridBottom;

    popup_screen(manager, &width, &gridBottom);
    popup_open_list(&manager->popup, POP_TARGET_THEME, names, count, current, width, gridBottom);
}

// El nombre del archivo, sin la ruta: es lo que se quiere cambiar con el
// guardar con otro nombre.
static const char *file_name(const char *path) {
    const char *name = path;

    for (const char *at = path; *at != '\0'; at++) {
        if (*at == '/' || *at == '\\') {
            name = at + 1;
        }
    }

    return name;
}

// La carpeta del archivo que se esta viendo, con su separador: abrir un vecino
// es lo que mas se hace despues de abrir uno.
static void folder_of(const char *path, char *into, size_t size) {
    const char *last = NULL;

    for (const char *at = path; *at != '\0'; at++) {
        if (*at == '/' || *at == '\\') {
            last = at;
        }
    }

    if (last == NULL) {
        into[0] = '\0';
        return;
    }

    size_t length = (size_t) (last - path) + 1;

    if (length >= size) {
        length = size - 1;
    }

    memcpy(into, path, length);
    into[length] = '\0';
}

static void open_text_popup(CellManager *manager, PopupTarget target, const char *initial, const char *about) {
    int width;
    int gridBottom;

    popup_screen(manager, &width, &gridBottom);

    // El aviso dice de donde se parte: el campo puede salir vacio (guardar con
    // otro nombre) o con la carpeta de delante (abrir un vecino), y escribir
    // detras de un nombre viejo nunca dio un nombre.
    char message[MESSAGE_SIZE];

    popup_open_input(&manager->popup, target, initial, width, gridBottom);

    if (target == POP_TARGET_SAVE_AS) {
        snprintf(message, sizeof(message), "save as: now %s, Enter to write, Esc to leave", about);
    } else {
        snprintf(message, sizeof(message), "open: Enter to read, Esc to leave");
    }

    set_message(manager, message);
}

// El tema puesto sigue la seleccion del popup: se prueba con solo moverse, y el
// propio popup se repinta con el tema nuevo en el mismo fotograma.
static void preview_theme(CellManager *manager, int index) {
    if (manager->popup.target != POP_TARGET_THEME) {
        return;
    }

    theme_set_by_name(theme_name_at(index));
    invalidate_screen();
}

// Confirmar el popup de elegir: la fila marcada manda su tecla, y el boton
// sigue siendo su tecla tambien flotando.
static bool popup_choose(Editor *editor, CellManager *manager, int index) {
    Popup *popup = &manager->popup;
    bool wasMenu = popup->kind == POPUP_MENU;
    int key = popup->keys[index];

    if (wasMenu) {
        popup_close(popup);

        return handle_key(editor, manager, key);
    }

    if (popup->target == POP_TARGET_THEME) {
        preview_theme(manager, index);
        set_message(manager, theme_name());
    }

    popup_close(popup);

    return editor->running;
}

// Confirmar el campo de texto: guardar con otro nombre, o abrir lo que diga.
static void popup_commit_text(Editor *editor, CellManager *manager) {
    Popup *popup = &manager->popup;
    char message[MESSAGE_SIZE];

    if (popup->text[0] == '\0') {
        popup_close(popup);
        set_message(manager, "nothing to write");

        return;
    }

    if (popup->target == POP_TARGET_SAVE_AS) {
        bool saved = save_document(popup->text, manager) == FILE_LOADED;
        char path[1024];

        snprintf(path, sizeof(path), "%s", popup->text);
        popup_close(popup);

        if (saved) {
            snprintf(editor->savePath, sizeof(editor->savePath), "%s", path);
            sheet_mark_saved(manager);
            snprintf(message, sizeof(message), "saved %s", file_name(path));
            set_message(manager, message);
        } else {
            set_error_message(manager, "cannot write file");
        }

        return;
    }

    if (sheet_any_dirty(manager)) {
        popup_close(popup);
        set_message(manager, "unsaved changes: Ctrl+S before opening");

        return;
    }

    int ignored = 0;

    // El documento manda: lo que habia se olvida antes de leer.
    workbook_clear(manager);

    int status = load_file(popup->text, manager, &ignored);
    char path[1024];

    snprintf(path, sizeof(path), "%s", popup->text);
    popup_close(popup);

    if (status == FILE_LOADED || status == FILE_NOT_FOUND) {
        snprintf(editor->path, sizeof(editor->path), "%s", path);

        // Un guion se importa y guarda al lado, en el formato del editor.
        if (document_is_path(path)) {
            snprintf(editor->savePath, sizeof(editor->savePath), "%s", path);
        } else {
            document_change_extension(path, ".clex", editor->savePath, sizeof(editor->savePath));
        }
    }

    if (status == FILE_LOADED) {
        if (ignored > 0) {
            snprintf(message, sizeof(message), "loaded (%d lines ignored)", ignored);
        } else {
            snprintf(message, sizeof(message), "opened %s", file_name(path));
        }

        set_message(manager, message);
    } else if (status == FILE_NOT_FOUND) {
        // No existe: hoja nueva, como con el archivo que se nombra al abrir el
        // programa.
        snprintf(message, sizeof(message), "new sheet: saves as %s", file_name(editor->savePath));
        set_message(manager, message);
    } else {
        set_error_message(manager, "cannot open file");
    }
}

// La tecla, para el popup puesto. Devuelve false si la tecla no es del popup,
// que lo cierra y se hace lo que tocase.
static bool popup_key(Editor *editor, CellManager *manager, int key) {
    Popup *popup = &manager->popup;

    if (popup->kind == POPUP_INPUT) {
        if (key != KEY_ESC && key != KEY_ENTER && key != KEY_BACKSPACE && !(key >= 32 && key < 127)) {
            popup_cancel(popup);

            return false;
        }

        switch (key) {
            case KEY_ESC:
                popup_cancel(popup);
                set_message(manager, "");
                break;
            case KEY_ENTER:
                popup_commit_text(editor, manager);
                break;
            case KEY_BACKSPACE: {
                size_t length = strlen(popup->text);

                if (length > 0) {
                    popup->text[length - 1] = '\0';
                }

                break;
            }
            default: {
                size_t length = strlen(popup->text);

                if (length + 1 < POPUP_TEXT) {
                    popup->text[length] = (char) key;
                    popup->text[length + 1] = '\0';
                }

                break;
            }
        }

        return true;
    }

    bool up = key == KEY_UP || key == 'k';
    bool down = key == KEY_DOWN || key == 'j';

    if (!up && !down && key != KEY_ENTER && key != KEY_ESC) {
        popup_cancel(popup);

        return false;
    }

    if (up || down) {
        if (popup->count > 0) {
            int step = up ? -1 : 1;

            popup->selected += step;

            // La lista da la vuelta: el popup de los temas es una rueda, como
            // la de antes con el Ctrl+T solo.
            if (popup->selected < 0) {
                popup->selected = popup->count - 1;
            }

            if (popup->selected >= popup->count) {
                popup->selected = 0;
            }

            preview_theme(manager, popup->selected);
        }

        return true;
    }

    if (key == KEY_ESC) {
        popup_cancel(popup);
        set_message(manager, "");

        return true;
    }

    popup_choose(editor, manager, popup->selected);

    return true;
}

bool handle_mouse(Editor *editor, CellManager *manager, const Mouse *mouse) {
    int width = manager->size.x < 20 ? 20 : manager->size.x;
    int height = manager->size.y < 6 ? 6 : manager->size.y;

    // Solo manda el pulsar. Soltar el boton, o mover el raton sin apretar, no es
    // ninguna orden.
    if (!mouse->pressed) {
        return editor->running;
    }

    if (mouse->button == CLICK_WHEEL_UP || mouse->button == CLICK_WHEEL_DOWN) {
        // La rueda recorre el cursor por las filas, como las flechas.
        return handle_key(editor, manager, mouse->button == CLICK_WHEEL_UP ? KEY_UP : KEY_DOWN);
    }

    // Un clic corta la escritura del nombre de la hoja, como hace una tecla de
    // mando, y hace lo que toca despues.
    if (manager->renaming) {
        manager->renaming = false;
        manager->renameText[0] = '\0';
    }

    // Con una ventana delante, el clic es de la ventana: dentro, la fila
    // elegida manda su tecla; fuera, el desplegable se cierra y se absorbe el
    // clic, para no mover el cursor detras de la cortina.
    if (manager->popup.active) {
        int item = -1;

        if (popup_at_cell(&manager->popup, mouse->column, mouse->row, &item)) {
            if (manager->popup.kind == POPUP_INPUT) {
                return editor->running;
            }

            if (item >= 0) {
                manager->popup.selected = item;

                return popup_choose(editor, manager, item);
            }

            return editor->running;
        }

        if (manager->popup.kind == POPUP_MENU) {
            popup_cancel(&manager->popup);
        }

        return editor->running;
    }

    // La barra de las herramientas, arriba de todo: un clic en una seccion
    // cuelga su desplegable, y otro en la misma lo recoge.
    if (manager->showMenu && mouse->row == menu_row()) {
        int section = menu_at_column(width, mouse->column);

        if (section >= 0) {
            int size;
            int gridBottom;

            popup_screen(manager, &size, &gridBottom);
            popup_open_menu(&manager->popup, section, size, gridBottom);
        }

        return editor->running;
    }

    if (mouse->row == sheet_row(height)) {
        if (!manager->showMenu && mouse->column >= tools_chip_column(width)) {
            // El chip que se escribe cuando la barra esta quitada.
            toggle_tools(manager);

            return editor->running;
        }

        int sheet = sheet_at_column(manager, mouse->column);

        if (sheet < 0 || sheet == manager->active) {
            return editor->running;
        }

        sheet_select(manager, sheet);
        set_message(manager, sheet_name_at(manager, sheet));

        return editor->running;
    }

    if (mouse->row == status_row(height) && status_path_at(mouse->column)) {
        // El nombre del archivo es el tirador de la fila: escondida, el boton que
        // la ponía ya no se ve, y hay algo que poder tocar para traerla.
        toggle_tools(manager);

        return editor->running;
    }

    unsigned int x = 0;
    unsigned int y = 0;

    if (!cell_at_screen(manager, mouse->column, mouse->row, &x, &y)) {
        return editor->running;
    }

    if (sheet_state(manager)->editing) {
        // Se estaba escribiendo una celda: el clic en otra la deja como si se
        // hubiera pulsado Escape, y hace lo que toca despues.
        handle_key(editor, manager, KEY_ESC);
    }

    if (mouse->button == CLICK_RIGHT) {
        // El clic derecho copia la celda, que es lo que se quiere hacer ahi.
        sheet_state(manager)->cursor.x = (uint16_t) x;
        sheet_state(manager)->cursor.y = (uint16_t) y;

        return handle_key(editor, manager, KEY_CTRL_C);
    }

    if (mouse->button != CLICK_LEFT) {
        return editor->running;
    }

    unsigned long long now = get_time_ms();

    // Dos clics encima de la misma celda, uno detras de otro, son un doble clic.
    bool twice = x == editor->clickX && y == editor->clickY && now - editor->clickTime <= CLICK_DOUBLE_MS;

    editor->clickTime = now;
    editor->clickX = x;
    editor->clickY = y;

    sheet_state(manager)->cursor.x = (uint16_t) x;
    sheet_state(manager)->cursor.y = (uint16_t) y;

    if (!twice || sheet_state(manager)->editing) {
        return editor->running;
    }

    // Como en cualquier hoja de calculo: al doble clic se entra a editar lo que
    // tiene la celda.
    return handle_key(editor, manager, 'i');
}

bool handle_key(Editor *editor, CellManager *manager, int key) {
    if (key == KEY_CLICK) {
        // El raton llega como una tecla mas, y lo que ha hecho se mira aparte.
        return handle_mouse(editor, manager, get_mouse());
    }

    // Cualquier otra tecla cancela la confirmacion de salida.
    if (key != 'q') {
        editor->quitting = false;
    }

    if (key != KEY_CTRL_L) {
        editor->reloading = false;
    }

    // Con una ventana delante, la tecla es de la ventana. Las que no son de
    // ella la cierran y se hacen lo de siempre: una ventana no puede tapar una
    // orden.
    if (manager->popup.active) {
        if (popup_key(editor, manager, key)) {
            return editor->running;
        }
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
        if (editor->path[0] == '\0') {
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
    } else if (key == KEY_CTRL_B || key == KEY_CTRL_U || key == KEY_CTRL_O) {
        // El formato de la celda. Se puede cambiar mientras se escribe, como se
        // cambia mientras se mira: es una capa encima del fuente, no dentro.
        CellFormat *format = cursor_format(manager);
        char message[MESSAGE_SIZE];

        if (format == NULL) {
            set_error_message(manager, "no room for the cell");
            keepMessage = true;
        } else {
            if (key == KEY_CTRL_B) {
                format->bold = format->bold != 0 ? 0 : 1;
                snprintf(message, sizeof(message), "bold %s", format->bold != 0 ? "on" : "off");
            } else if (key == KEY_CTRL_U) {
                format->underline = format->underline != 0 ? 0 : 1;
                snprintf(message, sizeof(message), "underline %s", format->underline != 0 ? "on" : "off");
            } else {
                // La alineacion da la vuelta: por defecto, izquierda, centro,
                // derecha. Por defecto es como en una hoja de calculo: el numero
                // a la derecha y el texto a la izquierda.
                format->align = (unsigned char) ((format->align + 1) % 4);

                const char *names[4] = { "default", "left", "center", "right" };

                snprintf(message, sizeof(message), "align %s", names[format->align]);
            }

            set_message(manager, message);
            keepMessage = true;
        }
    } else if (key == KEY_CTRL_K) {
        // La barra de las herramientas, puesta o quitada: quien trabaja a teclado se
        // queda con una fila mas de hoja, y quien trabaja a clic la quiere
        // delante.
        toggle_tools(manager);
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
                // Los temas, en una ventana flotante: cada nombre escrito con su
                // propio tema, y puesto al instante mientras se recorre la lista.
                if (manager->popup.active && manager->popup.target == POP_TARGET_THEME) {
                    popup_cancel(&manager->popup);
                    break;
                }

                open_theme_popup(manager);
                set_message(manager, theme_name());
                keepMessage = true;
                break;
            case KEY_CTRL_G: {
                // Guardar con otro nombre: el aviso dice el que hay ahora, que
                // es de donde se parte.
                const char *seed = editor->savePath;

                if (seed[0] == '\0') {
                    seed = "workbook.clex";
                }

                open_text_popup(manager, POP_TARGET_SAVE_AS, "", file_name(seed));
                keepMessage = true;
                break;
            }
            case KEY_CTRL_Q: {
                // Abrir otro archivo: la carpeta del que se esta viendo sale
                // escrita, para cambiar solo el nombre de lo de al lado.
                char folder[1024];

                folder_of(editor->path, folder, sizeof(folder));
                open_text_popup(manager, POP_TARGET_OPEN, folder, "");

                keepMessage = true;
                break;
            }
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
                if (editor->savePath[0] == '\0') {
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
                if (editor->savePath[0] == '\0') {
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
