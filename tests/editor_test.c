// Pruebas de la edicion de las celdas. No necesitan terminal: solo se
// llama a las funciones del gestor de celdas, sin dibujar nada.
//
//   tests/editor_test

#include "cell_manager.h"
#include "sheet.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

int main(void) {
    CellManager manager = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    // Escribir en una celda que no tiene nada.
    manager.cursor.x = 1;
    manager.cursor.y = 1;
    edit_insert(&manager, "7", 1);
    check_content(&manager, 1, 1, "7", "escribir en una celda vacia");
    check(manager.dirty, "la hoja queda sin guardar al escribir");

    // Insertar en medio del contenido.
    manager.cursor.x = 0;
    manager.cursor.y = 0;
    set_cell_content(&manager, 0, 0, "abc");
    edit_reset(&manager);
    edit_move_cursor(&manager, -1);
    edit_move_cursor(&manager, -1);
    edit_insert(&manager, "X", 1);
    check_content(&manager, 0, 0, "aXbc", "insertar en medio");

    // Borrar lo que hay antes del cursor.
    edit_backspace(&manager);
    check_content(&manager, 0, 0, "abc", "borrar hacia atras");

    // Borrar lo que hay en el cursor.
    edit_delete(&manager);
    check_content(&manager, 0, 0, "ac", "borrar hacia adelante");

    // Insertar al principio y al final.
    edit_to_start(&manager);
    edit_insert(&manager, "1", 1);
    check_content(&manager, 0, 0, "1ac", "insertar al principio");

    edit_to_end(&manager);
    edit_insert(&manager, "2", 1);
    check_content(&manager, 0, 0, "1ac2", "insertar al final");

    // Un caracter de dos bytes ocupa un solo caracter.
    manager.cursor.x = 2;
    manager.cursor.y = 2;
    set_cell_content(&manager, 2, 2, "a\u00f1");
    edit_reset(&manager);
    check(manager.editOffset == 3, "el cursor va al final de un texto con acentos");
    edit_move_cursor(&manager, -1);
    check(manager.editOffset == 1, "el cursor salta el caracter de dos bytes entero");

    // Deshacer y rehacer.
    undo_edit(&manager);
    check_content(&manager, 0, 0, "abc", "deshacer");
    redo_edit(&manager);
    check_content(&manager, 0, 0, "1ac2", "rehacer");

    // Los cambios seguidos de la misma celda son un solo paso de deshacer.
    undo_edit(&manager);
    check_content(&manager, 0, 0, "abc", "deshacer de golpe todos los cambios de la celda");

    // Copiar y pegar.
    copy_cell_content(&manager, 1, 1);
    manager.cursor.x = 3;
    manager.cursor.y = 3;
    paste_cell_content(&manager, 3, 3);
    check_content(&manager, 3, 3, "7", "pegar en una celda vacia");

    // Pegar y deshacer deja la celda como estaba.
    undo_edit(&manager);
    check_content(&manager, 3, 3, "", "deshacer un pegado");

    free_cell_manager(&manager);

    // La hoja se evalua celda a celda: una celda que no se puede calcular no
    // deja a las demas sin valor.
    {
        SheetResult result;
        char error[512];

        int status = sheet_evaluate_cells("A1 = 1\nA2 = 2\nB1 = A1 +\nA3 = A1 + A2\n", &result, error, sizeof(error));

        check(status == 0, "una celda rota no hace fallar la hoja entera");
        check(result.count == 3, "las celdas que si se pueden calcular tienen valor");
        check(strstr(error, "B1") != NULL, "el error dice que celda fallo");

        // Y los valores son los correctos.
        bool found = false;
        for (size_t i = 0; i < result.count; i++) {
            if (strcmp(result.values[i].ref, "A3") == 0 && strcmp(result.values[i].value, "3") == 0) {
                found = true;
            }
        }

        check(found, "la celda que depende de otra tiene el valor correcto");

        sheet_result_free(&result);
    }

    if (failures == 0) {
        printf("  edicion: todo correcto\n");
        return 0;
    }

    printf("  edicion: %d fallos\n", failures);

    return 1;
}
