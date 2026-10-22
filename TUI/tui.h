#ifndef CELEX_TUI_TUI
#define CELEX_TUI_TUI
#include "config.h"
#include <stdint.h>

#define KEY_ESC 27
#define KEY_ENTER 10
#define KEY_TAB 9
#define KEY_BACKSPACE 127
#define KEY_UP 256
#define KEY_DOWN 257
#define KEY_LEFT 258
#define KEY_RIGHT 259
#define KEY_HOME 260
#define KEY_END 261
#define KEY_DELETE 262

// La terminal ha pegado un texto. El texto se pide con get_paste_text().
#define KEY_PASTE 300

typedef struct {
    uint16_t x;
    uint16_t y;
} TermSize;

TermSize getTerminalSize();

void flush();
void move_cursor(uint16_t x, uint16_t y);
void clear_screen();

void open_screen();
void close_screen();

void print(char* p1, ...);

int getkey();

// El texto del ultimo pegado de la terminal. Solo vale cuando la tecla
// devuelta fue KEY_PASTE.
const char *get_paste_text();

#endif
