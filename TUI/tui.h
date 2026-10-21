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

#endif
