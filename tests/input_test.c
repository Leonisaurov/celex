// Pruebas del analizador de la entrada.
//
// El analizador convierte lo que llega de la terminal en teclas y en
// pegados. Un pegado es un texto que la terminal ha pegado, no teclas que
// haya tecleado la persona.
//
//   tests/input_test

#include "input.h"
#include "tui.h"

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

int main(void) {
    Input input;
    InputEvent event;

    input_init(&input);
    input.detectBursts = true;

    // Un caracter suelto es una tecla.
    input_push_byte(&input, 'a', 100);
    check(input_next(&input, &event), "una letra da un evento");
    check(event.kind == INPUT_KEY && event.key == 'a', "una letra es una tecla");

    // El Ctrl+C llega como el codigo 3.
    input_push_byte(&input, 3, 100);
    check(input_next(&input, &event), "el Ctrl+C da un evento");
    check(event.kind == INPUT_KEY && event.key == 3, "el Ctrl+C es la tecla 3");

    // Una secuencia de escape es una tecla.
    input_push_bytes(&input, "\x1b[A", 3, 100);
    check(input_next(&input, &event), "la flecha da un evento");
    check(event.kind == INPUT_KEY && event.key == KEY_UP, "la flecha arriba es una tecla");

    input_push_bytes(&input, "\x1b[3~", 4, 100);
    check(input_next(&input, &event), "el suprimir da un evento");
    check(event.kind == INPUT_KEY && event.key == KEY_DELETE, "el suprimir es una tecla");

    // El Escape solo, sin nada detras, es el Escape.
    input_push_byte(&input, 0x1b, 100);
    check(!input_next(&input, &event), "el Escape solo todavia no esta resuelto");
    input_no_more_bytes(&input);
    check(input_next(&input, &event), "el Escape solo ya esta resuelto");
    check(event.kind == INPUT_KEY && event.key == KEY_ESC, "el Escape solo es el Escape");

    // El raton, en el formato que mandan las terminales. La columna y la fila
    // empiezan en uno al llegar, y en el editor cuentan desde cero.
    input_push_bytes(&input, "\x1b[<0;10;5M", 10, 100);
    check(input_next(&input, &event), "un clic da un evento");
    check(event.kind == INPUT_CLICK, "y es del raton, no una tecla");
    check(event.mouse.button == CLICK_LEFT && event.mouse.pressed, "el boton de la izquierda pulsado");
    check(event.mouse.column == 9 && event.mouse.row == 4, "y donde cayo");

    input_push_bytes(&input, "\x1b[<0;10;5m", 10, 100);
    check(input_next(&input, &event), "soltar el boton da su evento");
    check(event.kind == INPUT_CLICK && !event.mouse.pressed, "y dice que se solto");

    input_push_bytes(&input, "\x1b[<2;3;3M", 9, 100);
    check(input_next(&input, &event), "el boton derecho da su evento");
    check(event.kind == INPUT_CLICK && event.mouse.button == CLICK_RIGHT, "y es el derecho");
    check(event.mouse.column == 2 && event.mouse.row == 2, "con sus coordenadas");

    input_push_bytes(&input, "\x1b[<65;1;1M", 10, 100);
    check(input_next(&input, &event), "la rueda da su evento");
    check(event.kind == INPUT_CLICK && event.mouse.button == CLICK_WHEEL_DOWN, "y dice para donde giro");

    // Una coordenada de varias cifras no rompe nada.
    input_push_bytes(&input, "\x1b[<0;123;456M", 13, 100);
    check(input_next(&input, &event), "un numero de varias cifras se lee");
    check(event.mouse.column == 122 && event.mouse.row == 455, "y vale lo que dice");

    // Lo que esta a medias no se entrega: se espera al resto de la secuencia.
    input_push_bytes(&input, "\x1b[<0;10", 7, 100);
    check(!input_next(&input, &event), "un clic a medias todavia no es un evento");
    input_push_bytes(&input, ";5M", 3, 110);
    check(input_next(&input, &event), "y al llegar el resto, si");
    check(event.kind == INPUT_CLICK && event.mouse.column == 9 && event.mouse.row == 4, "con lo que faltaba");

    // Un escape que no resulta ser el raton no se queda esperando para siempre.
    // Se prueba en su propia entrada, para que lo que sobre no estorbe.
    Input odd;
    input_init(&odd);

    input_push_bytes(&odd, "\x1b[<x", 4, 100);
    check(!input_next(&odd, &event), "un escape raruno todavia no esta resuelto");
    input_no_more_bytes(&odd);
    check(input_next(&odd, &event), "y se resuelve");
    check(event.kind == INPUT_KEY && event.key == KEY_ESC, "como un Escape, que es lo que era");

    input_free(&odd);

    // Una rafaga de caracteres que han llegado juntos es un pegado.
    input_push_bytes(&input, "hola", 4, 200);
    check(input_next(&input, &event), "la rafaga da un evento");
    check(event.kind == INPUT_PASTE, "una rafaga de caracteres es un pegado");
    check(event.text != NULL && strcmp(event.text, "hola") == 0, "el pegado tiene el texto");
    check(strcmp(input_paste_text(&input), "hola") == 0, "el pegado queda guardado");

    // Una tecla que se mantiene pulsada repite el mismo caracter: eso no es
    // un pegado.
    input_push_bytes(&input, "aa", 2, 300);
    check(input_next(&input, &event), "la tecla repetida da un evento");
    check(event.kind == INPUT_KEY && event.key == 'a', "la tecla repetida es una tecla");
    check(input_next(&input, &event), "la tecla repetida da otro evento");
    check(event.kind == INPUT_KEY && event.key == 'a', "la tecla repetida es otra tecla");

    // Caracteres que llegan muy lejos uno del otro son teclas sueltas.
    input_push_byte(&input, 'x', 1000);
    input_push_byte(&input, 'y', 2000);
    check(input_next(&input, &event) && event.kind == INPUT_KEY && event.key == 'x', "un caracter es una tecla");
    check(input_next(&input, &event) && event.kind == INPUT_KEY && event.key == 'y', "otro caracter es otra tecla");

    // El pegado con marcadores: el texto va entre ESC[200~ y ESC[201~.
    input_push_bytes(&input, "\x1b[200~hola\x1b[201~", 16, 400);
    check(input_next(&input, &event), "el pegado con marcadores da un evento");
    check(event.kind == INPUT_PASTE, "el pegado con marcadores es un pegado");
    check(event.text != NULL && strcmp(event.text, "hola") == 0, "el pegado con marcadores tiene el texto");

    // El pegado puede llegar partido en varias lecturas.
    input_push_bytes(&input, "\x1b[200~", 6, 500);
    check(!input_next(&input, &event), "el pegado a medias todavia no esta");
    input_push_bytes(&input, "hola\x1b[201~", 10, 501);
    check(input_next(&input, &event), "el pegado partido da un evento");
    check(event.kind == INPUT_PASTE, "el pegado partido es un pegado");
    check(event.text != NULL && strcmp(event.text, "hola") == 0, "el pegado partido tiene el texto");

    // Un pegado con los marcadores partido tambien.
    input_push_bytes(&input, "\x1b[200~", 6, 600);
    input_push_bytes(&input, "ho", 2, 601);
    input_push_bytes(&input, "la\x1b[2", 5, 602);
    input_push_bytes(&input, "01~", 3, 603);
    check(input_next(&input, &event), "el pegado con el final partido da un evento");
    check(event.kind == INPUT_PASTE, "el pegado con el final partido es un pegado");
    check(event.text != NULL && strcmp(event.text, "hola") == 0, "el pegado con el final partido tiene el texto");

    input_free(&input);

    if (failures == 0) {
        printf("  entrada: todo correcto\n");
        return 0;
    }

    printf("  entrada: %d fallos\n", failures);

    return 1;
}
