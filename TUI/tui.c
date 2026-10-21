#include "tui.h"
#include <asm-generic/ioctls.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static struct termios raw_term;

void open_screen() {
    printf("\x1b[?1049h");
    clear_screen();
    flush();

    tcgetattr(STDIN_FILENO, &term);

    raw_term = term;
    raw_term.c_lflag &= ~(ICANON | ISIG | ECHO);
    raw_term.c_iflag &= ~(IXON | ICRNL);
    raw_term.c_cc[VMIN] = 1;
    raw_term.c_cc[VTIME] = 0;

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

static int read_byte(unsigned int timeout) {
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);

    struct termios polled = current;
    polled.c_cc[VMIN] = timeout == 0 ? 1 : 0;
    polled.c_cc[VTIME] = timeout;
    tcsetattr(STDIN_FILENO, TCSANOW, &polled);

    unsigned char byte = 0;
    ssize_t count = read(STDIN_FILENO, &byte, 1);

    tcsetattr(STDIN_FILENO, TCSANOW, &current);

    if (count < 1) {
        return -1;
    }

    return (int) byte;
}

int getkey() {
    int byte = read_byte(0);

    if (byte < 0) {
        return -1;
    }

    if (byte != KEY_ESC) {
        return byte;
    }

    int next = read_byte(2);
    if (next < 0 || (next != '[' && next != 'O')) {
        return KEY_ESC;
    }

    int code = read_byte(2);
    if (code < 0) {
        return KEY_ESC;
    }

    switch (code) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        case '3': {
            int tilde = read_byte(2);
            if (tilde == '~') {
                return KEY_DELETE;
            }
            return KEY_ESC;
        }
        default: return KEY_ESC;
    }
}
