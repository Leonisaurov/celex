// Pruebas de las teclas del editor.
//
// Las teclas se manejan con la misma funcion que usa el programa, para
// comprobar lo que hace el usuario de verdad: escribir una celda, copiarla
// con el Ctrl+C, mover el cursor y pegarla con el Ctrl+V.
//
//   tests/keys_test

#include "cell_manager.h"
#include "clipboard.h"
#include "keys.h"
#include "theme.h"
#include "tui.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define close _close
#else
#include <unistd.h>
#endif

// El borde punteado de la celda copiada.
#define DASHED_BORDER "\xE2\x94\x84"

static int failures = 0;

static void check(bool ok, const char *what) {
    if (ok) {
        return;
    }

    printf("  FALLO: %s\n", what);
    failures++;
}

static void check_content(CellManager *manager, unsigned int x, unsigned int y, const char *wanted, const char *what) {
    MetaCell *meta = get_cell(manager, x, y);
    const char *content = cell_content(meta);

    if (strcmp(content, wanted) == 0) {
        return;
    }

    printf("  FALLO: %s (esperado \"%s\", obtenido \"%s\")\n", what, wanted, content);
    failures++;
}

static void check_value(CellManager *manager, unsigned int x, unsigned int y, const char *wanted, const char *what) {
    MetaCell *meta = get_cell(manager, x, y);
    const char *value = cell_value(meta);

    if (strcmp(value, wanted) == 0) {
        return;
    }

    printf("  FALLO: %s (esperado \"%s\", obtenido \"%s\")\n", what, wanted, value);
    failures++;
}

static void check_message(CellManager *manager, const char *wanted, const char *what) {
    if (strcmp(manager->message, wanted) == 0) {
        return;
    }

    printf("  FALLO: %s (esperado \"%s\", obtenido \"%s\")\n", what, wanted, manager->message);
    failures++;
}

// Escribe un texto, como si se tecleara.
static void type_text(CellManager *manager, Editor *editor, const char *text) {
    for (const char *character = text; *character != '\0'; character++) {
        handle_key(editor, manager, (int) (unsigned char) *character);
    }
}

// Dibuja la hoja y devuelve lo que se dibujo en la pantalla.
static char *draw(CellManager *manager) {
    FILE *file = tmpfile();

    if (file == NULL) {
        return NULL;
    }

    int saved = dup(fileno(stdout));

    if (saved < 0) {
        fclose(file);
        return NULL;
    }

    dup2(fileno(file), fileno(stdout));

    // Se dibuja dos veces: la primera dibuja todo y la segunda solo lo que
    // cambio, que es lo que se ve en la pantalla de verdad.
    print_cell_manager(manager);
    print_cell_manager(manager);

    fflush(stdout);

    dup2(saved, fileno(stdout));
    close(saved);

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = (char *) malloc((size_t) size + 1);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t) size, file);
    buffer[read] = '\0';

    fclose(file);

    return buffer;
}

int main(void) {
    // Las pruebas no deben cambiar el portapapeles del sistema.
    system_clipboard_set_enabled(false);

    CellManager manager = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor editor = {
        .path = NULL,
        .running = true,
        .quitting = false,
        .reloading = false,
    };

    // Escribir una formula en A1 y confirmarla.
    manager.cursor.x = 0;
    manager.cursor.y = 0;
    type_text(&manager, &editor, "1+2");
    handle_key(&editor, &manager, KEY_ENTER);

    check_content(&manager, 0, 0, "1+2", "la celda tiene lo que se escribio");
    check_value(&manager, 0, 0, "3", "la celda esta calculada");
    check(manager.cursor.y == 1, "al confirmar, el cursor baja");

    // Antes de copiar, no hay ninguna celda marcada.
    char *screen = draw(&manager);
    check(screen != NULL && strstr(screen, DASHED_BORDER) == NULL, "sin copiar, no hay ninguna celda marcada");
    free(screen);

    // Volver a A1 y copiarla con el Ctrl+C.
    handle_key(&editor, &manager, 'k');
    check(manager.cursor.y == 0, "el cursor vuelve arriba");

    handle_key(&editor, &manager, KEY_CTRL_C);

    check(clipboard_has_text(&manager.clipboard), "el portapapeles tiene el texto copiado");
    check(strcmp(clipboard_text(&manager.clipboard), "1+2") == 0, "el portapapeles tiene la formula de la celda");
    check(clipboard_has_cell(&manager.clipboard), "la celda copiada queda marcada");

    unsigned int copiedX = 99;
    unsigned int copiedY = 99;
    clipboard_cell(&manager.clipboard, &copiedX, &copiedY);
    check(copiedX == 0 && copiedY == 0, "la celda marcada es la que se copio");

    // La celda copiada se ve con un borde punteado.
    screen = draw(&manager);
    check(screen != NULL && strstr(screen, DASHED_BORDER) != NULL, "la celda copiada sale con el borde punteado");
    free(screen);

    // Moverse a B1 y pegar con el Ctrl+V.
    handle_key(&editor, &manager, 'l');
    check(manager.cursor.x == 1, "el cursor se mueve a la derecha");

    handle_key(&editor, &manager, KEY_CTRL_V);

    check_content(&manager, 1, 0, "1+2", "la celda pegada tiene la formula");
    check_value(&manager, 1, 0, "3", "la celda pegada esta calculada");
    check(clipboard_has_cell(&manager.clipboard), "la celda copiada sigue marcada despues de pegar");

    // Pegar y deshacer deja la celda como estaba.
    handle_key(&editor, &manager, KEY_CTRL_Z);
    check_content(&manager, 1, 0, "", "deshacer un pegado");

    // Una formula que usa otra celda, pegada en otro sitio, tambien se
    // calcula.
    manager.cursor.x = 0;
    manager.cursor.y = 1;
    type_text(&manager, &editor, "A1*10");
    handle_key(&editor, &manager, KEY_ENTER);
    check_value(&manager, 0, 1, "30", "la celda que usa otra celda esta calculada");

    handle_key(&editor, &manager, 'k');
    handle_key(&editor, &manager, KEY_CTRL_C);
    handle_key(&editor, &manager, 'l');
    handle_key(&editor, &manager, KEY_CTRL_V);

    check_content(&manager, 1, 1, "A1*10", "la formula pegada es la misma");
    check_value(&manager, 1, 1, "30", "la formula pegada se vuelve a calcular");

    // Dentro de una celda, el Ctrl+C copia el texto y no la celda.
    manager.cursor.x = 2;
    manager.cursor.y = 0;
    handle_key(&editor, &manager, 'i');
    check(manager.editing, "la tecla i empieza a editar la celda");

    type_text(&manager, &editor, "9");
    handle_key(&editor, &manager, KEY_CTRL_C);

    check(!clipboard_has_cell(&manager.clipboard), "copiar el texto quita la marca de la celda");
    check(strcmp(clipboard_text(&manager.clipboard), "9") == 0, "el portapapeles tiene el texto de la celda");

    // El Ctrl+V dentro de una celda mete el texto donde esta el cursor.
    handle_key(&editor, &manager, KEY_HOME);
    handle_key(&editor, &manager, KEY_CTRL_V);
    check_content(&manager, 2, 0, "99", "el texto pegado entra donde esta el cursor");

    handle_key(&editor, &manager, KEY_END);
    handle_key(&editor, &manager, KEY_CTRL_V);
    check_content(&manager, 2, 0, "999", "el texto pegado entra al final de la celda");

    handle_key(&editor, &manager, KEY_ESC);

    // Un pegado de la terminal: el texto entra en la celda del cursor y la
    // hoja se vuelve a calcular.
    manager.cursor.x = 0;
    manager.cursor.y = 2;
    handle_paste(&manager, "2+3");

    check_content(&manager, 0, 2, "2+3", "el pegado de la terminal pone el texto en la celda");
    check_value(&manager, 0, 2, "5", "el pegado de la terminal calcula la celda");
    check(strcmp(clipboard_text(&manager.clipboard), "2+3") == 0, "el portapapeles se queda con el texto pegado");

    // Si el texto pegado es el mismo que ya tiene el portapapeles, la celda
    // copiada sigue marcada.
    manager.cursor.x = 0;
    manager.cursor.y = 0;
    handle_key(&editor, &manager, KEY_CTRL_C);
    handle_paste(&manager, clipboard_text(&manager.clipboard));
    check(clipboard_has_cell(&manager.clipboard), "pegar lo mismo deja la celda copiada marcada");

    // Copiar una celda vacia y pegarla: la celda de destino se vacia, como
    // en una hoja de calculo.
    set_cell_content(&manager, 4, 0, "");
    set_cell_content(&manager, 5, 0, "lo que sea");

    manager.cursor.x = 4;
    manager.cursor.y = 0;
    handle_key(&editor, &manager, KEY_CTRL_C);
    check(clipboard_has_cell(&manager.clipboard), "una celda vacia tambien se marca");

    manager.cursor.x = 5;
    manager.cursor.y = 0;
    handle_key(&editor, &manager, KEY_CTRL_V);
    check_content(&manager, 5, 0, "", "pegar una celda vacia vacia la celda");

    // Sin portapapeles, el Ctrl+V no hace nada y avisa.
    CellManager empty = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor emptyEditor = { .path = NULL, .running = true, .quitting = false, .reloading = false };

    handle_key(&emptyEditor, &empty, KEY_CTRL_V);
    check_message(&empty, "the clipboard is empty", "pegar sin portapapeles avisa");
    check(!empty.dirty, "pegar sin portapapeles no cambia la hoja");

    // El tamano de las celdas: los corchetes mueven el ancho y las llaves el
    // alto, y ninguna de esas teclas empieza a escribir en la celda.
    CellManager sized = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor sizedEditor = { .path = NULL, .running = true, .quitting = false, .reloading = false };

    set_cell_size(4, 2);

    int baseWidth = cellWidth;
    int baseHeight = cellHeight;

    handle_key(&sizedEditor, &sized, ']');
    check(cellWidth == baseWidth + 1, "el corchete cierra hace la celda mas ancha");
    check(!sized.editing, "cambiar el tamano no empieza a escribir en la celda");
    check(!sized.dirty, "cambiar el tamano no cuenta como un cambio de la hoja");

    handle_key(&sizedEditor, &sized, '}');
    check(cellHeight == baseHeight + 1, "la llave cierra hace la celda mas alta");

    handle_key(&sizedEditor, &sized, '[');
    handle_key(&sizedEditor, &sized, '{');
    check(cellWidth == baseWidth && cellHeight == baseHeight, "el corchete y la llave de abrir los deshacen");

    free_cell_manager(&sized);

    // Los temas de colores: el Ctrl+T pasa al siguiente, y dando la vuelta
    // entera se vuelve al de empezar.
    CellManager themed = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor themedEditor = { .path = NULL, .running = true, .quitting = false, .reloading = false };

    const char *first = theme_name();

    handle_key(&themedEditor, &themed, KEY_CTRL_T);
    check(strcmp(theme_name(), first) != 0, "el Ctrl+T cambia el tema");
    check_message(&themed, theme_name(), "la barra dice el tema que esta puesto");

    // Se da la vuelta a la lista menos una: falta un tema para volver al
    // principio.
    for (int i = 0; i < theme_count() - 2; i++) {
        handle_key(&themedEditor, &themed, KEY_CTRL_T);
    }

    handle_key(&themedEditor, &themed, KEY_CTRL_T);
    check(strcmp(theme_name(), first) == 0, "los temas dan la vuelta y vuelven al primero");

    free_cell_manager(&themed);

    free_cell_manager(&empty);
    free_cell_manager(&manager);

    if (failures == 0) {
        printf("  teclas: todo correcto\n");
        return 0;
    }

    printf("  teclas: %d fallos\n", failures);

    return 1;
}
