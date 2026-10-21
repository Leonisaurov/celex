#include "tui.h"
#include "platform.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void flush() {
    fflush(stdout);
}

void move_cursor(uint16_t x, uint16_t y) {
    printf("\x1b[%d;%dH", y + 1, x + 1);
}

void clear_screen() {
    printf("\x1b[2J\x1b[3K");
    move_cursor(0, 0);
}

void open_screen() {
    // Primero la terminal, para que la pantalla alternativa y las secuencias
    // ANSI se apliquen bien.
    platform_open_screen();

    printf("\x1b[?1049h");
    clear_screen();
    flush();
}

void close_screen() {
    printf("\x1b[?1049l");
    flush();

    platform_close_screen();
}

void print(char* p1, ...) {
    va_list args;
    va_start(args, p1);

    char *p = p1;
    while (p != NULL) {
        printf("%s", p);
        p = va_arg(args, char*);
    }

    va_end(args);
    flush();
}

int getkey() {
    return platform_getkey();
}
