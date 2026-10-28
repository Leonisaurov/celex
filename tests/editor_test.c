// Pruebas de la edicion de las celdas y del documento del editor. No necesitan
// terminal: solo se llama a las funciones del gestor de celdas, sin dibujar nada.
//
//   tests/editor_test

#include "cell_manager.h"
#include "document.h"
#include "preferences.h"
#include "ref.h"
#include "sheet.h"
#include "theme.h"
#include "tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define TT_MKDIR(p) _mkdir(p)
#define TT_RMDIR(p) _rmdir(p)
#define TT_SETENV(name, value) _putenv_s((name), (value))
#else
#include <sys/stat.h>
#include <unistd.h>
#define TT_MKDIR(p) mkdir((p), 0755)
#define TT_RMDIR(p) rmdir(p)
#define TT_SETENV(name, value) setenv((name), (value), 1)
#endif

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

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return;
    }

    fputs(text, file);
    fclose(file);
}

// Los temas que define el usuario, leidos de una carpeta de pruebas en el
// temporal. Se mira el numero, el nombre, y las secuencias que salen.
static void test_custom_themes(void) {
    const char *base = getenv("TEMP");

    if (base == NULL || base[0] == '\0') {
        base = getenv("TMP");
    }

    if (base == NULL || base[0] == '\0') {
        base = getenv("TMPDIR");
    }

    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }

    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/celex_themes_test", base);
    TT_MKDIR(dir);

    char valiente[1024], oculto[1024], roto[1024], duplicado[1024];
    snprintf(valiente, sizeof(valiente), "%s/valiente.theme", dir);
    snprintf(oculto, sizeof(oculto), "%s/_oculto.theme", dir);
    snprintf(roto, sizeof(roto), "%s/roto.theme", dir);
    snprintf(duplicado, sizeof(duplicado), "%s/duplicado.theme", dir);

    write_text(valiente,
        "# un tema para las pruebas\n"
        "name valiente\n"
        "\n"
        "background #102030 on #010203   # el vacio\n"
        "cursor #fff on #00ff00 bold\n"
        "error #ff0000 reverse\n"
        "tools #123456 on #654321\n");
    write_text(oculto, "name escondido\ngrid #123456\n");
    write_text(roto, "nombre sin-acento\n");
    write_text(duplicado, "name oscuro\nbackground #000000\n");

    int base_count = theme_count();

    int added = theme_load_themes_dir(dir);
    check(added == 2, "carga los validos y salta el duplicado y el oculto");
    check(theme_count() == base_count + 2, "la lista crece solo con los temas nuevos");

    check(theme_set_by_name("valiente") != 0, "un tema del usuario se pone por su nombre");
    check(strcmp(style_sequence(STYLE_NONE), "\x1b[0;38;2;16;32;48;48;2;1;2;3m") == 0, "el fondo sale en color verdadero");
    check(strcmp(style_sequence(STYLE_CURSOR), "\x1b[0;1;38;2;255;255;255;48;2;0;255;0m") == 0, "la seleccion junta negrita, letra y fondo");
    check(strcmp(style_sequence(STYLE_ERROR), "\x1b[0;7;38;2;255;0;0m") == 0, "el error con la vuelta va sin fondo");
    check(strcmp(style_sequence(STYLE_TOOLS), "\x1b[0;38;2;18;52;86;48;2;101;67;33m") == 0,
          "la barra de las herramientas tiene su fondo");
    check(strcmp(style_sequence(STYLE_HEADER), "\x1b[0m") == 0, "lo que no se escribe queda apagado");

    check(theme_set_by_name("roto") != 0, "un archivo sin nombre toma el del archivo");
    check(strcmp(theme_name(), "roto") == 0, "y asi se llama");

    check(theme_set_by_name("escondido") == 0, "el tema con guion bajo delante no se carga");

    check(theme_set_by_name("oscuro") != 0, "un nombre repetido no pisa al integrado");
    check(strcmp(style_sequence(STYLE_GRID), "\x1b[0;90m") == 0, "sigue mandando el oscuro de siempre");

    theme_set_by_name("normal");
    theme_free_user();
    check(theme_count() == base_count, "olvidar los del usuario deja la lista como estaba");

    remove(valiente);
    remove(oculto);
    remove(roto);
    remove(duplicado);
    TT_RMDIR(dir);
}

// Las preferencias del usuario viven en `<HOME>/.config/celex/config`, asi que
// se apunta HOME a una carpeta del temporal para no tocar las de verdad.
static void test_preferences(void) {
    const char *base = getenv("TEMP");

    if (base == NULL || base[0] == '\0') {
        base = getenv("TMP");
    }

    if (base == NULL || base[0] == '\0') {
        base = getenv("TMPDIR");
    }

    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }

    char home[1024];
    snprintf(home, sizeof(home), "%s/celex_prefs_test", base);
    TT_MKDIR(home);

    char oldHome[1024];
    const char *previous = getenv("HOME");
    snprintf(oldHome, sizeof(oldHome), "%s", previous != NULL ? previous : "");
    bool hadHome = previous != NULL;

    TT_SETENV("HOME", home);

    theme_set_by_name("normal");
    set_cell_size(4, 2);

    CellManager doc = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    check(preferences_load(&doc) == 0, "sin archivo de preferencias no hay nada que leer");

    check(theme_set_by_name("oscuro") != 0, "el tema de las preferencias existe");
    set_cell_size(6, 3);
    doc.showMenu = false;
    doc.showValues = true;

    check(preferences_save(&doc) == 1, "las preferencias se escriben en la configuracion");
    check(preferences_save(&doc) == 0, "sin cambios no se vuelve a escribir");

    // Se vuelve a lo de empezar y se leen: tiene que volver todo lo elegido.
    theme_set_by_name("normal");
    set_cell_size(4, 2);

    CellManager back = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    check(preferences_load(&back) == 1, "las preferencias se leen al arrancar");
    check(strcmp(theme_name(), "oscuro") == 0, "vuelve el tema elegido");
    check(cellWidth == 6 && cellHeight == 3, "vuelve el tamano de las celdas");
    check(!back.showMenu, "vuelve la barra de las herramientas escondida");
    check(back.showValues, "y la vista de los valores");

    // Lo que no se entiende no rompe el archivo: las lineas que si valen se leen.
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/celex/config", home);

    write_text(path, "campo-raro 7\ntheme oscuro\ncell-size 8 2\ntools on\nvalues off\n");

    CellManager edited = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    check(preferences_load(&edited) == 1, "un archivo con lineas raras se lee igual");
    check(cellWidth == 8 && cellHeight == 2, "y aplica las lineas que entiende");
    check(edited.showMenu, "la barra se puede volver a poner");
    check(!edited.showValues, "y la vista de los valores apagar");

    theme_set_by_name("normal");
    set_cell_size(4, 2);

    free_cell_manager(&edited);
    free_cell_manager(&back);
    free_cell_manager(&doc);

    if (hadHome) {
        TT_SETENV("HOME", oldHome);
    } else {
        TT_SETENV("HOME", "");
    }

    remove(path);

    char themes[1024];
    snprintf(themes, sizeof(themes), "%s/.config/celex/themes", home);
    TT_RMDIR(themes);

    char conf[1024];
    snprintf(conf, sizeof(conf), "%s/.config/celex", home);
    TT_RMDIR(conf);

    char dotConfig[1024];
    snprintf(dotConfig, sizeof(dotConfig), "%s/.config", home);
    TT_RMDIR(dotConfig);

    TT_RMDIR(home);
}

int main(void) {
    CellManager manager = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    // Escribir en una celda que no tiene nada.
    sheet_state(&manager)->cursor.x = 1;
    sheet_state(&manager)->cursor.y = 1;
    edit_insert(&manager, "7", 1);
    check_content(&manager, 1, 1, "7", "escribir en una celda vacia");
    check(sheet_state(&manager)->dirty, "la hoja queda sin guardar al escribir");

    // Insertar en medio del contenido.
    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->cursor.y = 0;
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
    sheet_state(&manager)->cursor.x = 2;
    sheet_state(&manager)->cursor.y = 2;
    set_cell_content(&manager, 2, 2, "a\u00f1");
    edit_reset(&manager);
    check(sheet_state(&manager)->editOffset == 3, "el cursor va al final de un texto con acentos");
    edit_move_cursor(&manager, -1);
    check(sheet_state(&manager)->editOffset == 1, "el cursor salta el caracter de dos bytes entero");

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
    paste_cell_content(&manager, 3, 3, clipboard_text(&manager.clipboard));
    check_content(&manager, 3, 3, "7", "pegar en una celda vacia");

    // Pegar y deshacer deja la celda como estaba.
    undo_edit(&manager);
    check_content(&manager, 3, 3, "", "deshacer un pegado");

    // El valor de una celda se invalida cuando cambia su contenido: si no, se
    // seguiria enseñando un valor que ya no corresponde.
    set_cell_value(&manager, 5, 5, "42");
    check_value(&manager, 5, 5, "42", "la celda tiene valor");

    copy_cell_content(&manager, 1, 1);
    paste_cell_content(&manager, 5, 5, clipboard_text(&manager.clipboard));
    check_content(&manager, 5, 5, "7", "pegar encima de una celda con valor");
    check_value(&manager, 5, 5, "", "el valor viejo se invalida al pegar");

    // Y al deshacer tambien.
    set_cell_value(&manager, 5, 5, "42");
    undo_edit(&manager);
    check_value(&manager, 5, 5, "", "el valor se invalida al deshacer");

    // Pegar dentro de una celda pega el texto del portapapeles, donde esta
    // el cursor.
    set_cell_content(&manager, 6, 6, "A1 + 2");
    set_cell_value(&manager, 6, 6, "3");
    copy_cell_content(&manager, 6, 6);

    sheet_state(&manager)->cursor.x = 7;
    sheet_state(&manager)->cursor.y = 7;
    sheet_state(&manager)->editOffset = 0;
    paste_cell_text(&manager, 7, 7, clipboard_text(&manager.clipboard));
    check_content(&manager, 7, 7, "A1 + 2", "pegar dentro de una celda pega la formula");

    // Si la celda copiada no tiene valor, se pega su contenido.
    set_cell_content(&manager, 8, 8, "sin valor");
    copy_cell_content(&manager, 8, 8);

    sheet_state(&manager)->cursor.x = 9;
    sheet_state(&manager)->cursor.y = 9;
    sheet_state(&manager)->editOffset = 0;
    paste_cell_text(&manager, 9, 9, clipboard_text(&manager.clipboard));
    check_content(&manager, 9, 9, "sin valor", "pegar una celda sin valor pega su contenido");

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

    // El deslizamiento de la vista: un salto del ancho de una celda se pinta en
    // fotogramas de en medio, para que se vean las celdas cortadas por el
    // borde. Un salto de una sola columna no necesita ninguno, y uno muy grande
    // se pinta de golpe para no quedarse detras de las teclas.
    check(pan_frames(0, 0) == 0, "no hay deslizamiento cuando la vista no se mueve");
    check(pan_frames(1, 0) == 0, "una columna no necesita fotogramas en medio");
    check(pan_frames(CELL_SIZE_X + 1, 0) == 2, "el ancho de una celda se desliza en dos fotogramas");
    check(pan_frames(-(CELL_SIZE_X + 1), 0) == 2, "volver tambien se desliza");
    check(pan_frames(0, CELL_SIZE_Y + 1) == 2, "bajar una fila se desliza");
    check(pan_frames(2, 0) == 1, "dos columnas se deslizan en un fotograma");
    check(pan_frames(20, 0) == 0, "un salto grande se pinta de golpe");

    // El tamano de las celdas tiene limites: por encima y por debajo de ellos no
    // se sale, y se puede volver al de siempre.
    set_cell_size(CELL_MAX_WIDTH + 10, CELL_MAX_HEIGHT + 10);
    check(cellWidth == CELL_MAX_WIDTH, "el ancho de celda no pasa del maximo");
    check(cellHeight == CELL_MAX_HEIGHT, "el alto de celda no pasa del maximo");

    set_cell_size(0, 0);
    check(cellWidth == CELL_MIN_WIDTH, "el ancho de celda no baja del minimo");
    check(cellHeight == CELL_MIN_HEIGHT, "el alto de celda no baja del minimo");

    set_cell_size(4, 2);
    check(cellWidth == 4 && cellHeight == 2, "el tamano de celda se puede poner el de siempre");
    check(pan_frames(CELL_SIZE_X + 1, 0) == 2, "con el tamano de siempre, el deslizamiento es de dos fotogramas");

    // Las hojas del archivo: cada una con sus celdas, su cursor, su vista y su
    // historial de deshacer, que no se mezclan con las de las demas.
    CellManager book = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    check(sheet_count(&book) == 1, "el archivo empieza con una hoja");
    check(strcmp(sheet_name_at(&book, 0), "Sheet1") == 0, "la primera hoja se llama Sheet1");
    check(sheet_state(&book)->originX == 0 && sheet_state(&book)->originY == 0, "la primera hoja empieza sin deslizar");

    set_cell_content(&book, 0, 0, "1");

    int second = sheet_add(&book, "datos");
    check(second == 1, "anadir una hoja la pone detras de la que se esta viendo");
    check(sheet_count(&book) == 2, "ya hay dos hojas");
    check(sheet_active(&book) == 1, "la hoja nueva es la que se esta viendo");
    check(strcmp(sheet_name_at(&book, 1), "datos") == 0, "la hoja nueva tiene el nombre pedido");
    check(get_cell(&book, 0, 0) == NULL, "la hoja nueva esta vacia");

    set_cell_content(&book, 0, 0, "77");

    sheet_select(&book, 0);
    check_content(&book, 0, 0, "1", "volver a la primera hoja deja sus celdas");

    sheet_select(&book, 1);
    check_content(&book, 0, 0, "77", "la segunda hoja tiene las suyas");

    // El cursor y la vista son de cada hoja.
    sheet_state(&book)->cursor.x = 4;
    sheet_state(&book)->originX = 30;
    sheet_select(&book, 0);
    check(sheet_state(&book)->cursor.x == 0, "el cursor es de cada hoja");
    check(sheet_state(&book)->originX == 0, "lo deslizado es de cada hoja");

    // Y deshacer tambien: lo que se edito en una hoja no se deshace en otra.
    sheet_select(&book, 1);
    edit_cell_content(&book, 5, 5, "nueva");
    check(can_undo(&book), "la hoja editada tiene algo que deshacer");
    check(sheet_any_dirty(&book), "la hoja editada esta sin guardar");

    sheet_select(&book, 0);
    check(!can_undo(&book), "la otra hoja no tiene nada que deshacer");

    // Guardar deja limpias todas las hojas, no solo la que se veia.
    sheet_mark_saved(&book);
    check(!sheet_any_dirty(&book), "despues de guardar no queda ninguna hoja sin guardar");

    sheet_rename(&book, 1, "cambios");
    check(strcmp(sheet_name_at(&book, 1), "cambios") == 0, "el nombre de una hoja se puede cambiar");

    // Dos hojas no pueden llamarse igual: en las pestanas no se distinguirian.
    // Aqui la primera se queda con el nombre de la otra, y no se cambia.
    sheet_rename(&book, 0, "datos");
    check(sheet_count(&book) == 2, "siguen habiendo dos hojas");

    sheet_rename(&book, 1, "datos");
    check(strcmp(sheet_name_at(&book, 1), "cambios") == 0, "no se pone a una hoja el nombre que ya tiene otra");

    int added = sheet_add(&book, NULL);
    check(added >= 0, "se anadio una hoja para la prueba de nombres");
    check(strcmp(sheet_name_at(&book, added), "Sheet3") == 0, "la hoja anadida coge un nombre que no este usado");

    sheet_rename(&book, 0, "Sheet3");
    check(strcmp(sheet_name_at(&book, 0), "datos") == 0, "y la que se renombra tampoco repite nombre");

    // Se quita la hoja de prueba, para seguir con las dos de antes.
    sheet_remove(&book, added);
    check(sheet_count(&book) == 2, "la hoja de prueba se quito");

    // Quitar hojas: la que se esta viendo cambia de numero, y la ultima no se va.
    sheet_remove(&book, 1);
    check(sheet_count(&book) == 1, "quitar una hoja deja una menos");
    check(sheet_active(&book) == 0, "se sigue viendo la que queda");
    check_content(&book, 0, 0, "1", "la que queda es la primera");

    sheet_remove(&book, 0);
    check(sheet_count(&book) == 1, "la ultima hoja no se quita");

    // Las pestañas de la barra: caben todas, o se recorta por los lados, pero la
    // hoja que se esta viendo entra siempre.
    int six[20];

    for (int i = 0; i < 20; i++) {
        six[i] = 6;
    }

    int first = -1;
    int last = -1;

    sheet_bar_range(six, 3, 0, 80, &first, &last);
    check(first == 0 && last == 3, "con sitio de sobra se ven todas las pestañas");

    sheet_bar_range(six, 20, 19, 20, &first, &last);
    check(first <= 19 && last > 19, "la ultima hoja se ve aunque no quepan todas");
    check(last - first < 20, "y se dejan fuera las que no caben");

    sheet_bar_range(six, 20, 10, 20, &first, &last);
    check(first <= 10 && last > 10, "la hoja del medio tambien queda visible");

    int wide[1] = { 40 };
    sheet_bar_range(wide, 1, 0, 10, &first, &last);
    check(first == 0 && last == 1, "una hoja con nombre largo se pinta igualmente");

    sheet_bar_range(six, 0, 0, 80, &first, &last);
    check(first == 0 && last == 0, "sin hojas no hay pestañas");

    // El documento del editor, que es lo que guarda el .clex: lo que se escribe
    // tiene que volver igual, con las hojas, el tamano de las celdas, el tema y
    // lo que habia en cada celda.
    CellManager doc = new_cell_manager((TermSize) { .x = 80, .y = 24 });

    set_cell_content(&doc, 0, 0, "\"hola\"");
    set_cell_content(&doc, 1, 3, "A1 + 2");
    sheet_state(&doc)->cursor.x = 4;
    sheet_state(&doc)->cursor.y = 2;
    sheet_state(&doc)->originX = 15;

    // El formato de una celda con fuente, y el de una celda vacia: los dos son
    // una capa encima del contenido, y los dos caben en el documento.
    MetaCell *styled = get_or_create_cell(&doc, 1, 3);
    CellFormat rightBold = { ALIGN_RIGHT, 1, 0, 3, 0 };

    if (styled != NULL) {
        set_cell_format(styled, rightBold);
    }

    MetaCell *painted = get_or_create_cell(&doc, 7, 1);
    CellFormat filled = { ALIGN_DEFAULT, 0, 1, 0, 4 };

    if (painted != NULL) {
        set_cell_format(painted, filled);
    }

    check(sheet_add(&doc, "gastos") > 0, "el documento tiene otra hoja");
    set_cell_content(&doc, 0, 0, "30");

    char *text = document_source(&doc);
    check(text != NULL, "el documento se puede escribir");

    if (text != NULL) {
        check(strstr(text, "celex-document 1") != NULL, "empieza con la cabecera y la version");
        check(strstr(text, "theme") == NULL, "el tema no es de la hoja y no va en el documento");
        check(strstr(text, "cell-size") == NULL, "ni el tamano de las celdas");
        check(strstr(text, "tools") == NULL, "ni la barra de las herramientas");
        check(strstr(text, "sheet \"gastos\"") != NULL, "cada hoja abre su bloque");
        check(strstr(text, "cell A1 \"30\"") != NULL, "y dentro van sus celdas");
        check(strstr(text, "cell A1 \"\\\"hola\\\"\"") != NULL, "las comillas del fuente salen escapadas");
        check(strstr(text, "  select E3\n") != NULL, "guarda donde estaba el cursor");
        check(strstr(text, "  scroll 15 0\n") != NULL, "y lo deslizado que llevaba la vista");
        check(strstr(text, "format B4 align=right bold fg=3\n") != NULL, "el formato sale en su linea");
        check(strstr(text, "format H2 underline bg=4\n") != NULL, "y una celda vacia lleva solo su formato");

        CellManager back = new_cell_manager((TermSize) { .x = 80, .y = 24 });
        int ignored = -1;

        check(document_load(text, &back, &ignored) == 1, "el documento se reconoce por su cabecera");
        check(ignored == 0, "y no sobra ninguna linea");
        check(sheet_count(&back) == 2, "vuelven las dos hojas");
        check(strcmp(sheet_name_at(&back, 0), "Sheet1") == 0, "la primera hoja trae su nombre");
        check(strcmp(sheet_name_at(&back, 1), "gastos") == 0, "y la segunda el suyo");
        check(cellWidth == 4 && cellHeight == 2, "el documento no toca el tamano de las celdas");
        check(strcmp(theme_name(), "normal") == 0, "ni el tema");
        check(back.showMenu, "ni la barra de las herramientas");

        // Al abrir, la que se ve es la primera, como en el editor.
        check_content(&back, 0, 0, "\"hola\"", "el fuente con comillas vuelve desenredado");
        check_content(&back, 1, 3, "A1 + 2", "y el resto de las celdas");
        check(sheet_state(&back)->cursor.x == 4 && sheet_state(&back)->cursor.y == 2, "vuelve donde estaba el cursor");
        check(sheet_state(&back)->originX == 15, "y vuelve lo deslizado de la vista");

        sheet_select(&back, 1);
        check_content(&back, 0, 0, "30", "la otra hoja trae lo suyo");

        sheet_select(&back, 0);

        CellFormat backRight = cell_format(get_cell(&back, 1, 3));
        CellFormat backFilled = cell_format(get_cell(&back, 7, 1));

        check(backRight.align == ALIGN_RIGHT && backRight.bold == 1 && backRight.fg == 3,
              "vuelve el formato de la celda");
        check(backRight.underline == 0 && backRight.bg == 0, "y lo que no se puso sigue del tema");
        check(backFilled.underline == 1 && backFilled.bg == 4 && backFilled.align == ALIGN_DEFAULT,
              "vuelve el formato de la celda vacia");
        check_content(&back, 7, 1, "", "que sigue sin fuente");

        // Un documento que empieza con un campo que no se conoce se lee igual:
        // asi un archivo de una version futura no deja de abrirse.
        char *future = NULL;
        size_t futureLength = strlen(text) + 64;

        future = (char *) malloc(futureLength);
        snprintf(future, futureLength, "%s  campo-nuevo 7\n", text);

        CellManager newer = new_cell_manager((TermSize) { .x = 80, .y = 24 });
        int newerIgnored = 0;

        check(document_load(future, &newer, &newerIgnored) == 1, "un documento con un campo nuevo se abre");
        check(newerIgnored == 1, "y se cuenta lo que no se entendo");
        check(sheet_count(&newer) == 2, "con las hojas que si se entendieron");

        free(future);
        free_cell_manager(&newer);
        free_cell_manager(&back);
    }

    free(text);

    // Un formato roto no se medio cuela: la linea entera se ignora y se cuenta,
    // y la celda se queda con lo que si se entendo.
    const char *broken =
        "celex-document 1\n"
        "theme normal\n"
        "cell-size 4 2\n"
        "sheet \"rota\"\n"
        "  cell A1 \"5\"\n"
        "  format A1 align=sideways\n"
        "  format A1 fg=99\n"
        "  format A1 bold underline\n"
        "end-sheet\n";

    CellManager rota = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    int rotaIgnored = 0;

    check(document_load(broken, &rota, &rotaIgnored) == 1, "un formato roto no cierra el documento");
    check(rotaIgnored == 2, "y se cuenta cada linea que no vale");

    CellFormat rotaFormat = cell_format(get_cell(&rota, 0, 0));

    check(rotaFormat.bold == 1 && rotaFormat.underline == 1, "lo que si valia se pone");
    check(rotaFormat.align == ALIGN_DEFAULT && rotaFormat.fg == 0, "lo que no, no");
    check(rota.showMenu, "un documento sin la linea de las herramientas las muestra");

    free_cell_manager(&rota);

    // Donde cae el texto dentro de la celda. Con celdas de ocho:
    set_cell_size(8, 2);

    check(align_pad(ALIGN_LEFT, "7", 1) == 0, "a la izquierda no se corre nada");
    check(align_pad(ALIGN_RIGHT, "7", 1) == 7, "a la derecha se corre todo lo que sobra");
    check(align_pad(ALIGN_CENTER, "siete", 5) == 1, "en el centro el hueco se parte, y el resto se queda detras");
    check(align_pad(ALIGN_RIGHT, "siete y medio", 8) == 0, "lo que no cabe no se corre");

    // La alineacion de empezar es la de una hoja de calculo.
    check(align_pad(ALIGN_DEFAULT, "7", 1) == 7, "un numero suelto se va a la derecha");
    check(align_pad(ALIGN_DEFAULT, "12.500", 6) == 2, "un numero con puntos tambien");
    check(align_pad(ALIGN_DEFAULT, "-7", 2) == 6, "y uno con el signo delante");
    check(align_pad(ALIGN_DEFAULT, "siete", 5) == 0, "un texto se queda a la izquierda");
    check(align_pad(ALIGN_DEFAULT, "=7+1", 4) == 0, "un fuente no es un numero");

    set_cell_size(4, 2);

    // La barra de las herramientas: lo que se pinta es lo que se clica, y lo que
    // no cabe no se pinta ni se clica.
    int starts[MENU_LIMIT];
    int fitted = menu_fit(80, starts);

    check(fitted > 0 && fitted <= menu_count(), "en ochenta columnas caben algunas secciones");
    check(starts[0] == MENU_MARGIN, "la primera deja el aire de la izquierda");

    for (int i = 0; i < fitted; i++) {
        MenuSection section = menu_at(i);
        int size = (int) strlen(section.label);

        check(size >= 3, " cada seccion se puede leer");
        check(starts[i] + size <= 80, "y cada nombre cabe entero en la pantalla");
        check(menu_at_column(80, starts[i]) == i, "una seccion se clica donde empieza");
        check(menu_at_column(80, starts[i] + size - 1) == i, "y en su ultima letra");

        if (i > 0) {
            check(starts[i] > starts[i - 1], "y van una detras de otra");
        }
    }

    check(menu_at_column(80, 0) == -1, "el aire de la izquierda no es de ninguna seccion");
    check(menu_at_column(80, 80 - 1) == -1 || fitted < menu_count(), "el aire del final no es de ninguna");
    check(menu_fit(2, starts) == 0, "en una pantalla muy corta no cabe ninguna");
    check(menu_fit(200, starts) == menu_count(), "con sitio de sobra caben todas");

    set_cell_size(4, 2);
    theme_set_by_name("normal");
    free_cell_manager(&doc);

    // Los caminos de los dos formatos.
    check(document_is_path("hojas.clex"), "un .clex es el documento del editor");
    check(!document_is_path("hojas.cle"), "un .cle es el guion del lenguaje");

    char other[64];
    check(document_change_extension("notas.cle", ".clex", other, sizeof(other)) && strcmp(other, "notas.clex") == 0,
          "del guion se pasa al documento del lado");
    check(document_change_extension("notas.clex", ".cle", other, sizeof(other)) && strcmp(other, "notas.cle") == 0,
          "y del documento al guion");
    check(document_change_extension("sinextension", ".clex", other, sizeof(other)) && strcmp(other, "sinextension.clex") == 0,
          "un nombre sin extension la coge detras");
    check(document_change_extension("carpeta.d/me", ".clex", other, sizeof(other)) && strcmp(other, "carpeta.d/me.clex") == 0,
          "un punto del camino no es una extension");

    // El ida y vuelta de las referencias, sobre todo a partir de la Z, donde la
    // columna es base 26 biyectiva: A vale 1, AA 27, AB 28.
    {
        static const struct { unsigned int x; const char *name; } column_names[] = {
            { 0, "A" }, { 25, "Z" }, { 26, "AA" }, { 27, "AB" },
            { 51, "AZ" }, { 52, "BA" }, { 701, "ZZ" }, { 702, "AAA" },
        };

        for (size_t i = 0; i < sizeof(column_names) / sizeof(column_names[0]); i++) {
            char *got = ref_column_name(column_names[i].x);
            check(got != NULL && strcmp(got, column_names[i].name) == 0, "el nombre de la columna es el que toca");
            free(got);
        }

        int roundtrip_ok = 1;

        for (unsigned int x = 0; x < 720; x++) {
            char *name = ref_column_name(x);
            char ref[16];
            unsigned int px = 0;
            unsigned int py = 0;

            snprintf(ref, sizeof(ref), "%s1", name);

            if (!ref_parse(ref, &px, &py) || px != x || py != 0) {
                roundtrip_ok = 0;
            }

            free(name);
        }

        check(roundtrip_ok, "toda columna se escribe y se lee como la misma coordenada");
    }

    test_custom_themes();
    test_preferences();

    free_cell_manager(&book);

    if (failures == 0) {
        printf("  edicion: todo correcto\n");
        return 0;
    }

    printf("  edicion: %d fallos\n", failures);

    return 1;
}
