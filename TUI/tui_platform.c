#include "platform.h"
#include "tui.h"

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HANDLE input_handle = INVALID_HANDLE_VALUE;
static HANDLE output_handle = INVALID_HANDLE_VALUE;

static DWORD input_mode = 0;
static DWORD output_mode = 0;

static UINT output_code_page = 0;
static UINT input_code_page = 0;

static int input_is_console = 0;
static int output_is_console = 0;

static HANDLE std_input(void) {
    if (input_handle == INVALID_HANDLE_VALUE) {
        input_handle = GetStdHandle(STD_INPUT_HANDLE);
    }

    return input_handle;
}

static HANDLE std_output(void) {
    if (output_handle == INVALID_HANDLE_VALUE) {
        output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    return output_handle;
}

TermSize getTerminalSize() {
    TermSize size = {80, 24};

    HANDLE output = std_output();

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info)) {
        size.x = (uint16_t) (info.srWindow.Right - info.srWindow.Left + 1);
        size.y = (uint16_t) (info.srWindow.Bottom - info.srWindow.Top + 1);
    }

    return size;
}

void platform_open_screen() {
    HANDLE output = std_output();

    // La pantalla se dibuja con secuencias ANSI, asi que la consola tiene que
    // interpretarlas.
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &output_mode)) {
        DWORD mode = output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(output, mode);
        output_is_console = 1;
    }

    // Los caracteres de la caja son UTF-8, y la consola los dibuja segun la
    // pagina de codigos.
    output_code_page = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    input_code_page = GetConsoleCP();
    SetConsoleCP(CP_UTF8);

    HANDLE input = std_input();

    // Las teclas llegan de una en una y sin eco, como en el modo raw de POSIX.
    if (input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &input_mode)) {
        DWORD mode = input_mode;

        mode &= ~(DWORD) (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
        mode |= ENABLE_EXTENDED_FLAGS;
        mode &= ~ENABLE_QUICK_EDIT_MODE;

        SetConsoleMode(input, mode);
        input_is_console = 1;
    }
}

void platform_close_screen() {
    if (input_is_console) {
        SetConsoleMode(std_input(), input_mode);
        input_is_console = 0;
    }

    if (output_is_console) {
        SetConsoleMode(std_output(), output_mode);
        output_is_console = 0;
    }

    if (output_code_page != 0) {
        SetConsoleOutputCP(output_code_page);
        output_code_page = 0;
    }

    if (input_code_page != 0) {
        SetConsoleCP(input_code_page);
        input_code_page = 0;
    }
}

static int map_key(KEY_EVENT_RECORD key) {
    switch (key.wVirtualKeyCode) {
        case VK_UP: return KEY_UP;
        case VK_DOWN: return KEY_DOWN;
        case VK_LEFT: return KEY_LEFT;
        case VK_RIGHT: return KEY_RIGHT;
        case VK_HOME: return KEY_HOME;
        case VK_END: return KEY_END;
        case VK_DELETE: return KEY_DELETE;
        case VK_BACK: return KEY_BACKSPACE;
        case VK_RETURN: return KEY_ENTER;
        case VK_ESCAPE: return KEY_ESC;
        case VK_TAB: return KEY_TAB;
        default: break;
    }

    char character = key.uChar.AsciiChar;

    if (character == '\0') {
        return -1;
    }

    if (key.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
        // Ctrl+letra, sin importar si la consola la entrega en mayusculas o
        // en minusculas.
        if (character >= 'a' && character <= 'z') {
            return character - 'a' + 1;
        }

        if (character >= 'A' && character <= 'Z') {
            return character - 'A' + 1;
        }
    }

    return (int) (unsigned char) character;
}

int platform_getkey() {
    if (!input_is_console) {
        // Sin consola no hay teclas: se lee la entrada normal.
        int character = getchar();

        return character == EOF ? -1 : character;
    }

    for (;;) {
        INPUT_RECORD record;
        DWORD count = 0;

        if (!ReadConsoleInput(std_input(), &record, 1, &count) || count == 0) {
            return -1;
        }

        if (record.EventType != KEY_EVENT) {
            continue;
        }

        if (!record.Event.KeyEvent.bKeyDown) {
            continue;
        }

        int key = map_key(record.Event.KeyEvent);
        if (key < 0) {
            continue;
        }

        return key;
    }
}

#else

#include <asm-generic/ioctls.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

static struct termios term;
static struct termios raw_term;

TermSize getTerminalSize() {
    struct winsize win;

    memset(&win, 0, sizeof(win));

    // Sin terminal (la entrada viene de una tuberia) se usa un tamano fijo.
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) != 0 || win.ws_col == 0 || win.ws_row == 0) {
        return (TermSize) {
            .x = 80,
            .y = 24,
        };
    }

    return (TermSize) {
        .x = win.ws_col,
        .y = win.ws_row,
    };
}

void platform_open_screen() {
    tcgetattr(STDIN_FILENO, &term);

    raw_term = term;
    raw_term.c_lflag &= ~(ICANON | ISIG | ECHO);
    raw_term.c_iflag &= ~(IXON | ICRNL);
    raw_term.c_cc[VMIN] = 1;
    raw_term.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
}

void platform_close_screen() {
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
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

int platform_getkey() {
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

#endif
