#ifndef CELEX_TUI_TUI
#define CELEX_TUI_TUI
#include "config.h"
#include <stdint.h>

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

#ifdef DEBUG
char getch();
#endif

#endif
