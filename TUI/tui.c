#include "tui.h"
#include <asm-generic/ioctls.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

TermSize getTerminalSize() {
    struct winsize win;

    ioctl(STDIN_FILENO, TIOCGWINSZ, &win);

    return (TermSize) {
        .x = win.ws_col,
        .y = win.ws_row,
    };
}

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

struct termios term;

void open_screen() {
    printf("\x1b[?1049h");
    clear_screen();
    flush();

    tcgetattr(STDIN_FILENO, &term);

    struct termios raw_term = term;
    raw_term.c_lflag &= ~(ICANON | ISIG | ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
}

void close_screen() {
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    printf("\x1b[?1049l");
    flush();
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

#ifdef DEBUG
char getch() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);

    struct termios raw_term = term;
    raw_term.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

    char c = 0;
    read(STDIN_FILENO, &c, 1);

    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    return c;
}
#endif
