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
#include "tools.h"
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

    // Por la salida de errores: las pruebas de pantalla redirigen la salida
    // normal a un archivo, y un aviso escrito ahi se colaria en lo capturado
    // en vez de llegar a quien mira.
    fprintf(stderr, "  FALLO: %s\n", what);
    failures++;
}

static void check_content(CellManager *manager, unsigned int x, unsigned int y, const char *wanted, const char *what) {
    MetaCell *meta = get_cell(manager, x, y);
    const char *content = cell_content(meta);

    if (strcmp(content, wanted) == 0) {
        return;
    }

    fprintf(stderr, "  FALLO: %s (esperado \"%s\", obtenido \"%s\")\n", what, wanted, content);
    failures++;
}

static void check_value(CellManager *manager, unsigned int x, unsigned int y, const char *wanted, const char *what) {
    MetaCell *meta = get_cell(manager, x, y);
    const char *value = cell_value(meta);

    if (strcmp(value, wanted) == 0) {
        return;
    }

    fprintf(stderr, "  FALLO: %s (esperado \"%s\", obtenido \"%s\")\n", what, wanted, value);
    failures++;
}

static void check_message(CellManager *manager, const char *wanted, const char *what) {
    if (strcmp(manager->message, wanted) == 0) {
        return;
    }

    fprintf(stderr, "  FALLO: %s (esperado \"%s\", obtenido \"%s\")\n", what, wanted, manager->message);
    failures++;
}

// Escribe un texto, como si se tecleara.
static void type_text(CellManager *manager, Editor *editor, const char *text) {
    for (const char *character = text; *character != '\0'; character++) {
        handle_key(editor, manager, (int) (unsigned char) *character);
    }
}

// Dibuja la hoja, con el nombre de archivo que se ponga en la barra de estado, y
// devuelve lo que se dibujo.
static char *draw_path(CellManager *manager, const char *path) {
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

    // Se dibuja desde una pantalla en blanco, y dos veces: la primera pinta todo
    // y la segunda solo lo que cambio, que es lo que se ve en la pantalla de
    // verdad. Empezar de cero es lo que hace que lo que se mire sea siempre la
    // pantalla entera, y no lo que cambio desde la prueba de antes. Se dibuja la
    // pantalla igual que el programa: la rejilla, las herramientas, las pestanas y
    // la barra de estado, que es donde se ven los avisos y lo que ofrece el
    // tabulador.
    invalidate_screen();

    print_cell_manager(manager);
    print_sheets(manager);
    print_status(manager, "NORMAL", path);
    print_cell_manager(manager);
    print_sheets(manager);
    print_status(manager, "NORMAL", path);

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

// Lo mismo, sin nombre de archivo en la barra.
static char *draw(CellManager *manager) {
    return draw_path(manager, NULL);
}

int main(void) {
    // Las pruebas no deben cambiar el portapapeles del sistema.
    system_clipboard_set_enabled(false);

    CellManager manager = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor editor = {
        .path = "",
        .running = true,
        .quitting = false,
        .reloading = false,
    };

    // Escribir una formula en A1 y confirmarla.
    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->cursor.y = 0;
    type_text(&manager, &editor, "1+2");
    handle_key(&editor, &manager, KEY_ENTER);

    check_content(&manager, 0, 0, "1+2", "la celda tiene lo que se escribio");
    check_value(&manager, 0, 0, "3", "la celda esta calculada");
    check(sheet_state(&manager)->cursor.y == 1, "al confirmar, el cursor baja");

    // Antes de copiar, no hay ninguna celda marcada.
    char *screen = draw(&manager);
    check(screen != NULL && strstr(screen, DASHED_BORDER) == NULL, "sin copiar, no hay ninguna celda marcada");
    free(screen);

    // Volver a A1 y copiarla con el Ctrl+C.
    handle_key(&editor, &manager, 'k');
    check(sheet_state(&manager)->cursor.y == 0, "el cursor vuelve arriba");

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
    check(sheet_state(&manager)->cursor.x == 1, "el cursor se mueve a la derecha");

    handle_key(&editor, &manager, KEY_CTRL_V);

    check_content(&manager, 1, 0, "1+2", "la celda pegada tiene la formula");
    check_value(&manager, 1, 0, "3", "la celda pegada esta calculada");
    check(clipboard_has_cell(&manager.clipboard), "la celda copiada sigue marcada despues de pegar");

    // Pegar y deshacer deja la celda como estaba.
    handle_key(&editor, &manager, KEY_CTRL_Z);
    check_content(&manager, 1, 0, "", "deshacer un pegado");

    // Una formula que usa otra celda, pegada en otro sitio, tambien se
    // calcula.
    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->cursor.y = 1;
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
    sheet_state(&manager)->cursor.x = 2;
    sheet_state(&manager)->cursor.y = 0;
    handle_key(&editor, &manager, 'i');
    check(sheet_state(&manager)->editing, "la tecla i empieza a editar la celda");

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
    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->cursor.y = 2;
    handle_paste(&manager, "2+3");

    check_content(&manager, 0, 2, "2+3", "el pegado de la terminal pone el texto en la celda");
    check_value(&manager, 0, 2, "5", "el pegado de la terminal calcula la celda");
    check(strcmp(clipboard_text(&manager.clipboard), "2+3") == 0, "el portapapeles se queda con el texto pegado");

    // Si el texto pegado es el mismo que ya tiene el portapapeles, la celda
    // copiada sigue marcada.
    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->cursor.y = 0;
    handle_key(&editor, &manager, KEY_CTRL_C);
    handle_paste(&manager, clipboard_text(&manager.clipboard));
    check(clipboard_has_cell(&manager.clipboard), "pegar lo mismo deja la celda copiada marcada");

    // Copiar una celda vacia y pegarla: la celda de destino se vacia, como
    // en una hoja de calculo.
    set_cell_content(&manager, 4, 0, "");
    set_cell_content(&manager, 5, 0, "lo que sea");

    sheet_state(&manager)->cursor.x = 4;
    sheet_state(&manager)->cursor.y = 0;
    handle_key(&editor, &manager, KEY_CTRL_C);
    check(clipboard_has_cell(&manager.clipboard), "una celda vacia tambien se marca");

    sheet_state(&manager)->cursor.x = 5;
    sheet_state(&manager)->cursor.y = 0;
    handle_key(&editor, &manager, KEY_CTRL_V);
    check_content(&manager, 5, 0, "", "pegar una celda vacia vacia la celda");

    // Sin portapapeles, el Ctrl+V no hace nada y avisa.
    CellManager empty = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor emptyEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    handle_key(&emptyEditor, &empty, KEY_CTRL_V);
    check_message(&empty, "the clipboard is empty", "pegar sin portapapeles avisa");
    check(!sheet_state(&empty)->dirty, "pegar sin portapapeles no cambia la hoja");

    // El tamano de las celdas: los corchetes mueven el ancho y las llaves el
    // alto, y ninguna de esas teclas empieza a escribir en la celda.
    CellManager sized = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor sizedEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_size(4, 2);

    int baseWidth = cellWidth;
    int baseHeight = cellHeight;

    handle_key(&sizedEditor, &sized, ']');
    check(cellWidth == baseWidth + 1, "el corchete cierra hace la celda mas ancha");
    check(!sheet_state(&sized)->editing, "cambiar el tamano no empieza a escribir en la celda");
    check(!sheet_state(&sized)->dirty, "cambiar el tamano no cuenta como un cambio de la hoja");

    handle_key(&sizedEditor, &sized, '}');
    check(cellHeight == baseHeight + 1, "la llave cierra hace la celda mas alta");

    handle_key(&sizedEditor, &sized, '[');
    handle_key(&sizedEditor, &sized, '{');
    check(cellWidth == baseWidth && cellHeight == baseHeight, "el corchete y la llave de abrir los deshacen");

    free_cell_manager(&sized);

    // Los temas de colores: el Ctrl+T abre la ventana flotante, la flecha los
    // prueba al instante, y Enter la cierra con el puesto.
    CellManager themed = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor themedEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    const char *first = theme_name();

    handle_key(&themedEditor, &themed, KEY_CTRL_T);
    check(themed.popup.active && themed.popup.kind == POPUP_LIST && themed.popup.target == POP_TARGET_THEME,
          "el Ctrl+T abre la ventana de los temas");
    check(themed.popup.count == theme_count() && themed.popup.count > 1, "la lista trae todos los temas");
    check(strcmp(theme_name(), first) == 0, "abrir la ventana no cambia todavia el tema");

    // Cada nombre escrito con su propio tema: en lo pintado tienen que salir las
    // secuencias de temas distintos a la vez.
    screen = draw(&themed);
    check(screen != NULL && strstr(screen, "themes") != NULL, "la ventana dice que es de temas");
    check(screen != NULL && strstr(screen, "\x1b[0;37m") != NULL && strstr(screen, "\x1b[0;30m") != NULL,
          "y cada tema se escribe con su propio color");
    free(screen);

    handle_key(&themedEditor, &themed, KEY_DOWN);
    check(strcmp(theme_name(), first) != 0, "la flecha prueba el tema siguiente al instante");

    // La rueda tambien: llega como flecha.
    handle_key(&themedEditor, &themed, KEY_UP);
    check(strcmp(theme_name(), first) == 0, "y para atras vuelve a poner el de empezar");

    handle_key(&themedEditor, &themed, KEY_ENTER);
    check(!themed.popup.active, "Enter cierra la ventana con el tema puesto");
    check(strcmp(theme_name(), first) == 0, "y el tema queda");

    // Escape la cierra sin tocar lo puesto.
    handle_key(&themedEditor, &themed, KEY_CTRL_T);
    handle_key(&themedEditor, &themed, KEY_DOWN);
    handle_key(&themedEditor, &themed, KEY_ESC);
    check(!themed.popup.active, "Escape cierra la ventana");
    check(strcmp(theme_name(), first) == 0, "y cancelar deshace la vista previa");

    // Dar la vuelta a la lista, como antes daba el Ctrl+T a ciclos.
    for (int i = 0; i < theme_count(); i++) {
        handle_key(&themedEditor, &themed, KEY_DOWN);
    }

    handle_key(&themedEditor, &themed, KEY_ENTER);
    check(strcmp(theme_name(), first) == 0, "la lista de temas da la vuelta");

    // Una tecla que no es de la ventana la cierra y se hace lo de siempre.
    handle_key(&themedEditor, &themed, KEY_CTRL_T);
    handle_key(&themedEditor, &themed, KEY_CTRL_B);
    check(!themed.popup.active, "una tecla de mando cierra la ventana");

    free_cell_manager(&themed);

    // El formato de la celda, por teclado: cada tecla hace una cosa, y la
    // alineacion da la vuelta.
    CellManager styled = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor styledEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_content(&styled, 0, 0, "42");
    sheet_state(&styled)->cursor.x = 0;
    sheet_state(&styled)->cursor.y = 0;

    handle_key(&styledEditor, &styled, KEY_CTRL_B);
    check(cell_format(get_cell(&styled, 0, 0)).bold == 1, "el Ctrl+B pone la negrita");
    check_message(&styled, "bold on", "la barra dice lo que se ha hecho");

    handle_key(&styledEditor, &styled, KEY_CTRL_B);
    check(cell_format(get_cell(&styled, 0, 0)).bold == 0, "el Ctrl+B la quita");

    handle_key(&styledEditor, &styled, KEY_CTRL_U);
    check(cell_format(get_cell(&styled, 0, 0)).underline == 1, "el Ctrl+U subraya");

    handle_key(&styledEditor, &styled, KEY_CTRL_O);
    check(cell_format(get_cell(&styled, 0, 0)).align == ALIGN_LEFT, "el Ctrl+O alinea a la izquierda");
    check_message(&styled, "align left", "la barra dice la alineacion que esta puesta");

    handle_key(&styledEditor, &styled, KEY_CTRL_O);
    check(cell_format(get_cell(&styled, 0, 0)).align == ALIGN_CENTER, "otra vez, en el centro");

    handle_key(&styledEditor, &styled, KEY_CTRL_O);
    check(cell_format(get_cell(&styled, 0, 0)).align == ALIGN_RIGHT, "y a la derecha");

    handle_key(&styledEditor, &styled, KEY_CTRL_O);
    check(cell_format(get_cell(&styled, 0, 0)).align == ALIGN_DEFAULT, "y da la vuelta al sitio de empezar");

    // Dar formato a un sitio vacio hace que la celda exista, y cuenta como un
    // cambio de la hoja: si no, el archivo no se guardaria despues de formatear.
    sheet_state(&styled)->cursor.x = 5;
    sheet_state(&styled)->cursor.y = 5;
    sheet_state(&styled)->dirty = false;

    handle_key(&styledEditor, &styled, KEY_CTRL_B);
    check(get_cell(&styled, 5, 5) != NULL, "el formato cabe en una celda vacia");
    check(sheet_state(&styled)->dirty, "y formatear cuenta como un cambio de la hoja");

    free_cell_manager(&styled);

    // Como se pinta el formato: la negrita, el subrayado y el color sacan su
    // secuencia; el fuente de la celda no cambia nada.
    CellManager painted = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    MetaCell *head = get_or_create_cell(&painted, 0, 0);

    set_cell_content(&painted, 0, 0, "total");
    sheet_state(&painted)->cursor.x = 6;
    sheet_state(&painted)->cursor.y = 6;

    CellFormat bold = { ALIGN_DEFAULT, 1, 0, 0, 0 };
    set_cell_format(head, bold);

    screen = draw(&painted);
    check(screen != NULL && strstr(screen, "\x1b[1m") != NULL, "la negrita se pinta en negrita");
    check_content(&painted, 0, 0, "total", "y el fuente de la celda no se mueve");
    free(screen);

    CellFormat colored = { ALIGN_DEFAULT, 0, 1, 3, 0 };
    set_cell_format(head, colored);

    screen = draw(&painted);
    // El 3 de la paleta es el verde, que es el 32 de las secuencias.
    check(screen != NULL && strstr(screen, "\x1b[4m") != NULL, "el subrayado se pinta subrayado");
    check(screen != NULL && strstr(screen, "\x1b[32m") != NULL, "y el color de la paleta sale por su numero");
    free(screen);

    free_cell_manager(&painted);

    // La barra de las herramientas, arriba: se ven las secciones, el desplegable
    // junta lo que hay dentro, y cada boton sigue siendo la tecla de siempre. Se
    // quita con el Ctrl+K para quien viva a teclado.
    CellManager tooled = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor tooledEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_content(&tooled, 0, 0, "1");

    screen = draw(&tooled);
    check(screen != NULL && strstr(screen, "doc") != NULL && strstr(screen, "style") != NULL,
          "la barra de arriba se pinta con sus secciones");
    check(screen != NULL && strstr(screen, "bold") == NULL, "los nombres de los botones esperan dentro del desplegable");
    free(screen);

    // Colgar el desplegable de style: lo que se ve es lo que se puede elegir.
    int barStarts[MENU_LIMIT];
    menu_fit(80, barStarts);

    Mouse onStyle = { CLICK_LEFT, true, barStarts[2], menu_row() };

    handle_mouse(&tooledEditor, &tooled, &onStyle);
    check(tooled.popup.active && tooled.popup.kind == POPUP_MENU && tooled.popup.section == 2,
          "un clic en la barra cuelga el desplegable de la seccion");

    screen = draw(&tooled);
    check(screen != NULL && strstr(screen, "bold") != NULL && strstr(screen, "^B") != NULL,
          "el desplegable trae los botones con su tecla escrita");
    free(screen);

    // Un boton es su tecla, flotando tambien.
    sheet_state(&tooled)->cursor.x = 0;
    sheet_state(&tooled)->cursor.y = 0;

    Mouse onBold = { CLICK_LEFT, true, tooled.popup.x + 3, tooled.popup.y + 2 };

    handle_mouse(&tooledEditor, &tooled, &onBold);
    check(cell_format(get_cell(&tooled, 0, 0)).bold == 1, "el clic en el boton de negrita manda el Ctrl+B");
    check(!tooled.popup.active, "y el desplegable se recoge solo");

    handle_key(&tooledEditor, &tooled, KEY_CTRL_B);
    sheet_state(&tooled)->dirty = false;

    // El teclado también puede llegar: la seccion se elige con su nombre... y,
    // mientras, la barra se esconde y se vuelve a poner.
    handle_key(&tooledEditor, &tooled, KEY_CTRL_K);
    check(!tooled.showMenu, "el Ctrl+K esconde la barra");
    check_message(&tooled, "tools off", "y la barra de estado lo dice cortito, para no tapar el nombre");
    check(!sheet_state(&tooled)->dirty, "esconderla no es cambiar la hoja");

    screen = draw(&tooled);
    check(screen != NULL && strstr(screen, "doc") == NULL, "escondida, la barra no se pinta");
    free(screen);

    handle_key(&tooledEditor, &tooled, KEY_CTRL_K);
    check(tooled.showMenu, "el Ctrl+K la vuelve a poner");

    screen = draw(&tooled);
    check(screen != NULL && strstr(screen, "style") != NULL, "y se vuelve a pintar entera");
    free(screen);

    // Con el desplegable puesto, un clic fuera lo cierra sin mover el cursor de
    // la rejilla, que es lo que se espera de una cortina.
    handle_mouse(&tooledEditor, &tooled, &onStyle);

    Mouse outside = { CLICK_LEFT, true, 40, 10 };
    unsigned int beforeX = sheet_state(&tooled)->cursor.x;
    unsigned int beforeY = sheet_state(&tooled)->cursor.y;

    handle_mouse(&tooledEditor, &tooled, &outside);
    check(!tooled.popup.active, "un clic fuera cierra el desplegable");
    check(sheet_state(&tooled)->cursor.x == beforeX && sheet_state(&tooled)->cursor.y == beforeY,
          "y el clic absorbido no mueve el cursor");

    free_cell_manager(&tooled);

    // El raton. La pantalla de pruebas es de 80x24 y la celda de 4x2: arriba la
    // barra, sus letras, la rejilla hasta la fila 21, las pestanas y la barra de
    // estado.
    set_cell_size(4, 2);

    CellManager clicked = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor clickedEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_content(&clicked, 2, 2, "= 7");
    set_cell_content(&clicked, 0, 0, "1");

    Mouse click = { CLICK_LEFT, true, 16, 9 };

    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 2,
          "un clic en una celda pone el cursor ahi");
    check(!sheet_state(&clicked)->editing, "y un clic suelto no entra a editar");

    // Soltar el boton no hace nada, y caiga donde caiga.
    click.row = 2;
    click.column = 6;
    click.pressed = false;
    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 2,
          "soltar el boton no mueve el cursor");

    click.pressed = true;

    // La raya de la rejilla no es una celda, ni la de en medio ni la de abajo.
    click.column = 15;
    click.row = 2;
    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 2,
          "un clic en la raya vertical no hace nada");

    click.column = 16;
    click.row = 5;
    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 2,
          "un clic en la raya horizontal no hace nada");

    // La cabecera de las letras tampoco: con la barra puesta, las letras bajan a
    // la fila segunda.
    click.column = 11;
    click.row = 1;
    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 2,
          "un clic en las letras de arriba no hace nada");

    // Y lo de encima de las letras es la barra: si no cae en ninguna seccion,
    // tampoco.
    click.column = 40;
    click.row = menu_row();
    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 2,
          "un clic en el aire de la barra no hace nada");
    check(!clicked.popup.active, "ni siquiera cuelga un desplegable");

    // El doble clic entra a editar la celda. La primera vez que se clica encima
    // no: la memoria del clic anterior era de otra celda.
    click.column = 16;
    click.row = 12;
    handle_mouse(&clickedEditor, &clicked, &click);
    check(!sheet_state(&clicked)->editing, "el primero se queda en mover el cursor");
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 3,
          "y lleva el cursor a la celda nueva");

    handle_mouse(&clickedEditor, &clicked, &click);
    check(sheet_state(&clicked)->editing, "y el segundo, encima de la misma celda, entra a editarla");

    handle_key(&clickedEditor, &clicked, KEY_ESC);

    // El clic derecho copia la celda.
    Mouse right = { CLICK_RIGHT, true, 6, 3 };

    handle_mouse(&clickedEditor, &clicked, &right);
    check(sheet_state(&clicked)->cursor.x == 0 && sheet_state(&clicked)->cursor.y == 0,
          "el clic derecho pone el cursor en la celda");
    check_message(&clicked, "copied A1", "y copia lo que hay");

    // La rueda pasa de fila en fila, como las flechas.
    Mouse wheel = { CLICK_WHEEL_DOWN, true, 16, 8 };

    handle_mouse(&clickedEditor, &clicked, &wheel);
    check(sheet_state(&clicked)->cursor.y == 1, "la rueda hacia abajo baja una fila");

    wheel.button = CLICK_WHEEL_UP;
    handle_mouse(&clickedEditor, &clicked, &wheel);
    handle_mouse(&clickedEditor, &clicked, &wheel);
    check(sheet_state(&clicked)->cursor.y == 0, "y hacia arriba sube");

    // Un boton es su tecla, tambien colgado: el clic en la barra abre el
    // desplegable.
    int clickStarts[16];

    menu_fit(80, clickStarts);

    Mouse onBar = { CLICK_LEFT, true, clickStarts[0], menu_row() };

    handle_mouse(&clickedEditor, &clicked, &onBar);
    check(clicked.popup.active && clicked.popup.section == 0, "un clic en la barra cuelga su desplegable");

    handle_key(&clickedEditor, &clicked, KEY_ESC);
    check(!clicked.popup.active, "y el Escape lo recoge");

    // Y el clic en una pestana cambia de hoja. La segunda pestana empieza despues
    // de la primera, que mide seis, y detras de la raya.
    check(sheet_add(&clicked, "gastos") > 0, "el gestor de pruebas tiene una segunda hoja");
    check(strcmp(sheet_name_at(&clicked, 1), "gastos") == 0, "con su nombre");

    sheet_select(&clicked, 0);

    // La raya entre las dos pestanas no es de ninguna.
    Mouse onRaya = { CLICK_LEFT, true, 6, sheet_row(24) };

    handle_mouse(&clickedEditor, &clicked, &onRaya);
    check(sheet_active(&clicked) == 0, "la raya entre pestanas no cambia de hoja");

    Mouse onTab = { CLICK_LEFT, true, 7, sheet_row(24) };

    handle_mouse(&clickedEditor, &clicked, &onTab);
    check(sheet_active(&clicked) == 1, "el clic en la pestana pasa a la otra hoja");

    sheet_select(&clicked, 0);

    // Escribiendo en una celda, un clic en otra la deja escrita y lleva el cursor
    // a la nueva, como en cualquier hoja de calculo.
    sheet_state(&clicked)->cursor.x = 1;
    sheet_state(&clicked)->cursor.y = 1;
    handle_key(&clickedEditor, &clicked, 'i');
    handle_key(&clickedEditor, &clicked, '5');
    handle_key(&clickedEditor, &clicked, '5');

    Mouse elsewhere = { CLICK_LEFT, true, 16, 15 };

    handle_mouse(&clickedEditor, &clicked, &elsewhere);
    check(!sheet_state(&clicked)->editing, "el clic en otra celda termina la escritura");
    check_content(&clicked, 1, 1, "55", "y lo escrito se queda en su celda");
    check(sheet_state(&clicked)->cursor.x == 2 && sheet_state(&clicked)->cursor.y == 4,
          "y el cursor pasa a la celda clicada");

    // Escondidas las herramientas, esa fila es de la rejilla.
    handle_key(&clickedEditor, &clicked, KEY_CTRL_K);
    sheet_select(&clicked, 0);

    Mouse onGrid = { CLICK_LEFT, true, 16, 21 };

    handle_mouse(&clickedEditor, &clicked, &onGrid);
    check(sheet_state(&clicked)->cursor.y == 6,
          "sin la barra, ahi abajo tambien hay celdas");

    // Escondida la barra, la fila de las pestanas lleva el chip que la devuelve:
    // es lo que se ve, y lo que se clica.
    sheet_state(&clicked)->dirty = false;

    screen = draw_path(&clicked, "notas.clex");
    check(screen != NULL && strstr(screen, "[tools]") != NULL,
          "con la barra quitada, la de las pestanas lleva el chip");
    free(screen);

    Mouse onChip = { CLICK_LEFT, true, tools_chip_column(80) + 1, sheet_row(24) };

    handle_mouse(&clickedEditor, &clicked, &onChip);
    check(clicked.showMenu, "un clic en el chip vuelve a poner la barra");
    check(!sheet_state(&clicked)->dirty, "y traerla no cuenta como un cambio de la hoja");

    // Puesta la barra, el chip no se ve: el boton de quitarla esta en la propia
    // barra.
    screen = draw_path(&clicked, "notas.clex");
    check(screen != NULL && strstr(screen, "[tools]") == NULL, "con la barra puesta, el chip no se ve");
    check(screen != NULL && strstr(screen, "edits") != NULL, "y la barra se vuelve a pintar");
    free(screen);

    // El otro tirador es el nombre del archivo de la barra de estado.
    handle_key(&clickedEditor, &clicked, KEY_CTRL_K);

    screen = draw_path(&clicked, "notas.clex");
    check(screen != NULL && strstr(screen, "notas.clex") != NULL, "la barra lleva el nombre del archivo");
    free(screen);

    Mouse onName = { CLICK_LEFT, true, 80 - (int) strlen("notas.clex") - 1, status_row(24) };

    handle_mouse(&clickedEditor, &clicked, &onName);
    check(clicked.showMenu, "un clic en el nombre del archivo lo trae de vuelta");

    // Un clic en otra parte de la barra de estado no la mueve.
    handle_key(&clickedEditor, &clicked, KEY_CTRL_K);

    Mouse onStatus = { CLICK_LEFT, true, 2, status_row(24) };

    handle_mouse(&clickedEditor, &clicked, &onStatus);
    check(!clicked.showMenu, "un clic en otra parte de la barra de estado no la trae");

    free_cell_manager(&clicked);
    set_cell_size(4, 2);

    // Guardar con otro nombre y abrir: dos ventanas con un campo de texto, que
    // son lo unico del editor donde se escribe un nombre de archivo.
    const char *written = "hoja-de-prueba.clex";

    CellManager dialog = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor dialogEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_content(&dialog, 0, 0, "1");

    handle_key(&dialogEditor, &dialog, KEY_CTRL_G);
    check(dialog.popup.active && dialog.popup.kind == POPUP_INPUT && dialog.popup.target == POP_TARGET_SAVE_AS,
          "el Ctrl+G abre el campo de guardar con otro nombre");

    screen = draw(&dialog);
    check(screen != NULL && strstr(screen, "save as") != NULL, "la ventana dice lo que esta pidiendo");
    free(screen);

    // Un Enter sin nada escrito no guarda en ninguna parte.
    handle_key(&dialogEditor, &dialog, KEY_ENTER);
    check(!dialog.popup.active, "Enter sin nombre cierra la ventana");
    check_message(&dialog, "nothing to write", "y no guarda en ninguna parte");
    check(dialogEditor.savePath[0] == '\0', "el sitio de guardar sigue sin estar");

    // Escape deja el campo como estaba.
    handle_key(&dialogEditor, &dialog, KEY_CTRL_G);
    type_text(&dialog, &dialogEditor, "borrable");
    handle_key(&dialogEditor, &dialog, KEY_ESC);
    check(!dialog.popup.active && dialogEditor.savePath[0] == '\0', "Escape cancela el nombre escrito");

    // Y ahora el nombre de verdad, con su Extension.
    handle_key(&dialogEditor, &dialog, KEY_CTRL_G);
    type_text(&dialog, &dialogEditor, written);
    handle_key(&dialogEditor, &dialog, KEY_ENTER);

    check(!dialog.popup.active, "Enter cierra la ventana");
    check(strcmp(dialogEditor.savePath, written) == 0, "y guardar va a ese nombre desde ahora");

    {
        FILE *probe = fopen(written, "rb");

        check(probe != NULL, "el archivo sale escrito donde se dijo");

        if (probe != NULL) {
            fclose(probe);
        }
    }

    check(!sheet_any_dirty(&dialog), "lo guardado deja de estar sin guardar");

    // Abrir: el campo sale con la carpeta de delante, y Enter cambia de archivo.
    handle_key(&dialogEditor, &dialog, KEY_CTRL_Q);
    check(dialog.popup.active && dialog.popup.kind == POPUP_INPUT && dialog.popup.target == POP_TARGET_OPEN,
          "el Ctrl+Q abre el campo de abrir");

    handle_key(&dialogEditor, &dialog, KEY_BACKSPACE);
    type_text(&dialog, &dialogEditor, written);
    handle_key(&dialogEditor, &dialog, KEY_ENTER);

    check(strcmp(dialogEditor.path, written) == 0, "Enter abre el nombre escrito");
    check_content(&dialog, 0, 0, "1", "y la hoja es la del archivo");
    check(strcmp(dialogEditor.savePath, written) == 0, "y guardar sigue yendo al mismo sitio");

    // Con cambios sin guardar, abrir se niega: no se pisa el trabajo de uno.
    edit_cell_content(&dialog, 1, 0, "2");

    handle_key(&dialogEditor, &dialog, KEY_CTRL_Q);
    type_text(&dialog, &dialogEditor, written);
    handle_key(&dialogEditor, &dialog, KEY_ENTER);
    check_message(&dialog, "unsaved changes: Ctrl+S before opening", "abrir con cambios pide guardar antes");
    check(!dialog.popup.active, "y la ventana se cierra sin abrir nada");

    free_cell_manager(&dialog);
    remove(written);

    // El autocompletado: el tabulador escribe la palabra del lenguaje que falta,
    // la palabra de la hoja cuando no hay mas que una, y va pasando por las
    // candidatas cuando hay varias que empiezan igual.
    CellManager completed = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor completedEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_content(&completed, 0, 0, "123");
    set_cell_content(&completed, 0, 1, "hola");
    set_cell_content(&completed, 1, 1, "hombre");

    // Una palabra del lenguaje, que solo puede ser una.
    sheet_state(&completed)->cursor.x = 3;
    sheet_state(&completed)->cursor.y = 0;
    handle_key(&completedEditor, &completed, 'i');
    type_text(&completed, &completedEditor, "pr");
    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 3, 0, "print", "el tabulador completa una palabra del lenguaje");
    handle_key(&completedEditor, &completed, KEY_ESC);

    // Una palabra que ya esta en la hoja, y que tampoco tiene mas que una
    // candidata.
    sheet_state(&completed)->cursor.x = 3;
    sheet_state(&completed)->cursor.y = 1;
    handle_key(&completedEditor, &completed, 'i');
    type_text(&completed, &completedEditor, "12");
    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 3, 1, "123", "el tabulador completa lo que ya hay en la hoja");
    handle_key(&completedEditor, &completed, KEY_ESC);

    // Dos palabras que empiezan igual: el primer tabulador escribe lo que tienen
    // comun, y los siguientes van pasando por las candidatas.
    sheet_state(&completed)->cursor.x = 3;
    sheet_state(&completed)->cursor.y = 2;
    handle_key(&completedEditor, &completed, 'i');
    type_text(&completed, &completedEditor, "ho");
    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 3, 2, "ho", "lo comun a dos candidatas no cambia nada");

    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 3, 2, "hola", "el tabulador pone la primera candidata");

    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 3, 2, "hombre", "el tabulador pasa a la segunda");

    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 3, 2, "ho", "al dar la vuelta se vuelve a lo escrito");
    handle_key(&completedEditor, &completed, KEY_ESC);

    // Escribir despues de completar apaga la oferta.
    sheet_state(&completed)->cursor.x = 3;
    sheet_state(&completed)->cursor.y = 3;
    handle_key(&completedEditor, &completed, 'i');
    type_text(&completed, &completedEditor, "pr");
    handle_key(&completedEditor, &completed, KEY_TAB);
    type_text(&completed, &completedEditor, "!");
    check_content(&completed, 3, 3, "print!", "seguir escribiendo despues de completar deja la palabra");
    check(!completed.completion.active, "escribir apaga la oferta del tabulador");
    handle_key(&completedEditor, &completed, KEY_ESC);

    // Sin palabra escrita no hay nada que completar.
    sheet_state(&completed)->cursor.x = 4;
    sheet_state(&completed)->cursor.y = 3;
    handle_key(&completedEditor, &completed, 'i');
    handle_key(&completedEditor, &completed, KEY_TAB);
    check_content(&completed, 4, 3, "", "el tabulador no inventa nada sobre una celda vacia");
    handle_key(&completedEditor, &completed, KEY_ESC);

    // Con dos candidatas la oferta se queda delante, y se puede escribir lo que
    // hay entre ellas: es lo que ve quien esta completando.
    sheet_state(&completed)->cursor.x = 3;
    sheet_state(&completed)->cursor.y = 5;
    handle_key(&completedEditor, &completed, 'i');
    type_text(&completed, &completedEditor, "h");
    handle_key(&completedEditor, &completed, KEY_TAB);

    // Lo unico que "hola" y "hombre" tienen al principio es la o.
    check_content(&completed, 3, 5, "ho", "el tabulador escribe lo que las candidatas tienen comun");
    check(completed.completion.active, "con dos candidatas queda la oferta delante");

    char offered[MESSAGE_SIZE * 2];
    check(completion_text(&completed, offered, sizeof(offered)) == 1, "la oferta se puede escribir");
    check(strstr(offered, "hola") != NULL && strstr(offered, "hombre") != NULL, "la oferta dice cuales son las candidatas");

    // El orden de la hoja no se promete: solo que al pasar se escribe una de las
    // dos palabras enteras.
    handle_key(&completedEditor, &completed, KEY_TAB);
    const char *chosen = cell_content(get_cell(&completed, 3, 5));
    check(strcmp(chosen, "hola") == 0 || strcmp(chosen, "hombre") == 0, "el tabulador pasa a una candidata entera");

    // La oferta se ve en la barra: la palabra puesta entre llaves. Las llaves
    // solo las escribe la oferta, asi que la prueba no puede pasar por otro
    // sitio.
    char expected[COMPLETE_WORD + 4];
    snprintf(expected, sizeof(expected), "{%s}", chosen);

    char *drawing = draw(&completed);
    check(drawing != NULL && strstr(drawing, expected) != NULL, "la barra enseña la oferta del tabulador");
    free(drawing);

    handle_key(&completedEditor, &completed, KEY_ESC);

    free_cell_manager(&completed);

    // Las teclas de las hojas: anadir, cambiar, quitar.
    CellManager book = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor bookEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    set_cell_content(&book, 0, 0, "1");

    handle_key(&bookEditor, &book, KEY_CTRL_A);
    check(sheet_count(&book) == 2, "el Ctrl+A anade una hoja");
    check(sheet_active(&book) == 1, "el Ctrl+A se pone a ver la hoja nueva");
    check(get_cell(&book, 0, 0) == NULL, "la hoja nueva esta vacia");

    // La marca de la celda copiada es de una hoja: al cambiar se quita, pero lo
    // copiado se queda en el portapapeles.
    sheet_select(&book, 0);
    handle_key(&bookEditor, &book, KEY_CTRL_C);
    check(clipboard_has_cell(&book.clipboard), "la celda copiada queda marcada");

    sheet_select(&book, 1);
    check(!clipboard_has_cell(&book.clipboard), "cambiar de hoja quita la marca de la celda copiada");
    check(clipboard_has_content(&book.clipboard), "cambiar de hoja no tira lo copiado");

    // Tres hojas, para poder dar la vuelta a la lista.
    handle_key(&bookEditor, &book, KEY_CTRL_A);
    check(sheet_count(&book) == 3, "otra hoja mas");

    handle_key(&bookEditor, &book, KEY_CTRL_P);
    check(sheet_active(&book) == 1, "el Ctrl+P vuelve a la hoja de atras");

    handle_key(&bookEditor, &book, KEY_CTRL_N);
    check(sheet_active(&book) == 2, "el Ctrl+N avanza a la siguiente hoja");

    handle_key(&bookEditor, &book, KEY_CTRL_N);
    check(sheet_active(&book) == 0, "el Ctrl+N da la vuelta a la lista");

    handle_key(&bookEditor, &book, KEY_CTRL_D);
    check(sheet_count(&book) == 2, "el Ctrl+D quita la hoja que se esta viendo");

    handle_key(&bookEditor, &book, KEY_CTRL_D);
    handle_key(&bookEditor, &book, KEY_CTRL_D);
    check(sheet_count(&book) == 1, "el Ctrl+D no quita la ultima hoja");
    free_cell_manager(&book);

    // El nombre de la hoja, en una con el nombre de empezar: el Ctrl+F escribe
    // en la pestana, Enter lo fija y Escape lo deja como estaba.
    CellManager named = new_cell_manager((TermSize) { .x = 80, .y = 24 });
    Editor namedEditor = { .path = "", .running = true, .quitting = false, .reloading = false };

    handle_key(&namedEditor, &named, KEY_CTRL_F);
    check(named.renaming, "el Ctrl+F empieza a escribir el nombre");
    check(named.renameText[0] == 0, "se empieza de cero, sin el nombre viejo por delante");

    type_text(&named, &namedEditor, "hoja2");
    handle_key(&namedEditor, &named, KEY_ENTER);

    check(!named.renaming, "Enter deja de escribir el nombre");
    check(strcmp(sheet_name_at(&named, 0), "hoja2") == 0, "el nombre queda puesto");

    // Escape no cambia nada, y un nombre vacio tampoco deja a la hoja sin nombre.
    handle_key(&namedEditor, &named, KEY_CTRL_F);
    type_text(&named, &namedEditor, "x");
    handle_key(&namedEditor, &named, KEY_ESC);
    check(!named.renaming, "Escape deja de escribir el nombre");
    check(strcmp(sheet_name_at(&named, 0), "hoja2") == 0, "Escape no cambia el nombre de la hoja");

    handle_key(&namedEditor, &named, KEY_CTRL_F);
    handle_key(&namedEditor, &named, KEY_ENTER);
    check(strcmp(sheet_name_at(&named, 0), "hoja2") == 0, "un nombre vacio no deja a la hoja sin nombre");

    // Una tecla de mando corta la escritura del nombre, que era de la otra hoja.
    handle_key(&namedEditor, &named, KEY_CTRL_A);
    handle_key(&namedEditor, &named, KEY_CTRL_F);
    type_text(&named, &namedEditor, "z");
    handle_key(&namedEditor, &named, KEY_CTRL_P);
    check(!named.renaming, "una tecla de mando corta la escritura del nombre");
    check(strcmp(sheet_name_at(&named, 1), "Sheet2") == 0, "y lo escrito no se pone");

    free_cell_manager(&named);

    free_cell_manager(&empty);
    free_cell_manager(&manager);

    if (failures == 0) {
        printf("  teclas: todo correcto\n");
        return 0;
    }

    printf("  teclas: %d fallos\n", failures);

    return 1;
}
