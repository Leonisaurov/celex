#include "platform.h"
#include "input.h"
#include "tui.h"

#ifdef _WIN32

#include <windows.h>
#include <io.h>
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

// Los caracteres que llegan de la consola.
static Input console_input;

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

// El momento de un evento de la consola. La consola de Windows no da la
// hora de cada evento, asi que se usa la del momento en que se leen: los
// eventos de un pegado llegan todos juntos.
static unsigned long long event_time(void) {
    return (unsigned long long) GetTickCount64();
}

// Traduce una tecla de la consola a los bytes que Pondrian los caracteres
// de la terminal.
static void push_key(const INPUT_RECORD *record, unsigned long long time) {
    const KEY_EVENT_RECORD *key = &record->Event.KeyEvent;
    WORD code = key->wVirtualKeyCode;
    DWORD state = key->dwControlKeyState;
    bool control = (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

    switch (code) {
        case VK_UP:
            input_push_bytes(&console_input, "\x1b[A", 3, time);
            return;
        case VK_DOWN:
            input_push_bytes(&console_input, "\x1b[B", 3, time);
            return;
        case VK_RIGHT:
            input_push_bytes(&console_input, "\x1b[C", 3, time);
            return;
        case VK_LEFT:
            input_push_bytes(&console_input, "\x1b[D", 3, time);
            return;
        case VK_HOME:
            input_push_bytes(&console_input, "\x1b[H", 3, time);
            return;
        case VK_END:
            input_push_bytes(&console_input, "\x1b[F", 3, time);
            return;
        case VK_DELETE:
            input_push_bytes(&console_input, "\x1b[3~", 4, time);
            return;
        case VK_ESCAPE:
            input_push_byte(&console_input, 0x1b, time);
            return;
        case VK_BACK:
            input_push_byte(&console_input, 0x7f, time);
            return;
        case VK_RETURN:
            input_push_byte(&console_input, '\r', time);
            return;
        case VK_TAB:
            input_push_byte(&console_input, '\t', time);
            return;
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LWIN:
        case VK_RWIN:
            // Las teclas modificadoras no son un caracter.
            return;
        default:
            break;
    }

    char character = key->uChar.AsciiChar;

    if (character == '\0') {
        // Algunas terminales mandan la tecla sin el caracter: el codigo de
        // control se saca de la tecla virtual y de los modificadores.
        if (control && code >= 'A' && code <= 'Z') {
            input_push_byte(&console_input, (unsigned char) (code - 'A' + 1), time);
        }

        return;
    }

    if (control) {
        if (character >= 'a' && character <= 'z') {
            input_push_byte(&console_input, (unsigned char) (character - 'a' + 1), time);
            return;
        }

        if (character >= 'A' && character <= 'Z') {
            input_push_byte(&console_input, (unsigned char) (character - 'A' + 1), time);
            return;
        }

        // El Ctrl+[ y el Ctrl+@ son el Escape.
        if (character == '[' || character == '@') {
            input_push_byte(&console_input, 27, time);
            return;
        }

        // Algunos ya vienen como codigo de control.
        if (character >= 1 && character <= 26) {
            input_push_byte(&console_input, (unsigned char) character, time);
            return;
        }
    }

    input_push_byte(&console_input, (unsigned char) character, time);
}

// Cuantos eventos hay esperando en la consola.
static int count_console_events(void) {
    DWORD available = 0;

    if (!GetNumberOfConsoleInputEvents(std_input(), &available)) {
        return -1;
    }

    return (int) available;
}

// Los eventos de la consola de Windows no son caracteres: las teclas se
// traducen arriba y el raton aqui, a la secuencia que mandaria una terminal,
// para que el editor lea una sola forma de lo mismo.
static void push_mouse(const INPUT_RECORD *record, unsigned long long time) {
    const MOUSE_EVENT_RECORD *mouse = &record->Event.MouseEvent;

    int button = -1;
    bool pressed = true;

    if ((mouse->dwEventFlags & MOUSE_WHEELED) != 0) {
        // La rueda no tiene boton propio: sube con el numero arriba del todo.
        short turned = (short) LOWORD(mouse->dwButtonState);

        button = turned > 0 ? CLICK_WHEEL_UP : CLICK_WHEEL_DOWN;
    } else if ((mouse->dwEventFlags & MOUSE_MOVED) != 0) {
        // Moverse sin apretar nada no es una orden: se ignora, como en las
        // terminales que no piden los arrastres.
        return;
    } else if ((mouse->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0) {
        button = CLICK_LEFT;
    } else if ((mouse->dwButtonState & RIGHTMOST_BUTTON_PRESSED) != 0) {
        button = CLICK_RIGHT;
    } else if ((mouse->dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) != 0) {
        button = CLICK_MIDDLE;
    } else {
        button = CLICK_LEFT;
        pressed = false;
    }

    char sequence[32];
    snprintf(sequence, sizeof(sequence), "\x1b[<%d;%d;%d%c",
             button,
             mouse->dwMousePosition.X + 1,
             mouse->dwMousePosition.Y + 1,
             pressed ? 'M' : 'm');

    input_push_bytes(&console_input, sequence, strlen(sequence), time);
}

// Lee los eventos que hay en la consola, sin esperar.
static int read_console_events(void) {
    int available = count_console_events();

    if (available < 0) {
        return -1;
    }

    if (available == 0) {
        return 0;
    }

    if (available > 256) {
        available = 256;
    }

    INPUT_RECORD records[256];
    DWORD read = 0;

    if (!ReadConsoleInput(std_input(), records, (DWORD) available, &read)) {
        return -1;
    }

    unsigned long long time = event_time();

    for (DWORD i = 0; i < read; i++) {
        if (records[i].EventType == KEY_EVENT) {
            if (!records[i].Event.KeyEvent.bKeyDown) {
                continue;
            }

            push_key(&records[i], time);
            continue;
        }

        if (records[i].EventType == MOUSE_EVENT) {
            push_mouse(&records[i], time);
        }
    }

    return (int) read;
}

// Espera a que la consola tenga algo, con un tiempo maximo. Devuelve el
// resultado de la espera: WAIT_OBJECT_0 si hay algo, WAIT_TIMEOUT si se
// acabo el tiempo, o WAIT_FAILED si la consola no se puede esperar.
static DWORD wait_console_events(DWORD milliseconds) {
    HANDLE handle = std_input();

    if (handle == INVALID_HANDLE_VALUE) {
        return WAIT_FAILED;
    }

    return WaitForSingleObject(handle, milliseconds);
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

    // Las teclas llegan de en una y sin eco, como en el modo raw de POSIX. El
    // raton tambien: sin pedir sus eventos la consola no los da, y un clic se
    // queda en nada.
    if (input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &input_mode)) {
        DWORD mode = input_mode;

        mode &= ~(DWORD) (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
        mode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT;
        mode &= ~ENABLE_QUICK_EDIT_MODE;

        SetConsoleMode(input, mode);
        input_is_console = 1;
    }

    // Para que la terminal de verdad (Windows Terminal, una consola con VT)
    // mande los clics y las coordenadas en el formato de siempre.
    fputs("\x1b[?1000h\x1b[?1006h", stdout);
    fflush(stdout);

    input_init(&console_input);
    // Solo en una consola de verdad se puede saber si los caracteres llegaron
    // de golpe (un pegado) o de uno en uno (al teclear).
    console_input.detectBursts = input_is_console != 0;
}

void platform_close_screen() {
    // Se apaga el reporte del raton: lo que venga detras no tiene por que
    // recibir clics.
    fputs("\x1b[?1000l\x1b[?1006l", stdout);
    fflush(stdout);

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

    input_free(&console_input);
}

// Lee de la entrada normal, que no es una consola.
static int read_plain_bytes(Input *input) {
    unsigned char buffer[256];
    int count = 0;

    for (;;) {
        int read = _read(0, buffer, sizeof(buffer));
        if (read <= 0) {
            break;
        }

        for (int i = 0; i < read; i++) {
            input_push_byte(input, buffer[i], 0);
        }

        count += read;

        if (count > 0) {
            break;
        }
    }

    return count;
}

int platform_getkey() {
    for (;;) {
        InputEvent event;

        if (input_next(&console_input, &event)) {
            if (event.kind == INPUT_PASTE) {
                return KEY_PASTE;
            }

            if (event.kind == INPUT_CLICK) {
                return KEY_CLICK;
            }

            return event.key;
        }

        if (!input_is_console) {
            // Sin consola no hay teclas: se lee la entrada normal.
            if (read_plain_bytes(&console_input) <= 0) {
                return -1;
            }

            continue;
        }

        // Se leen los eventos que haya. Si no hay ninguno, se espera a que
        // los haya.
        int read = read_console_events();

        if (read < 0) {
            return -1;
        }

        if (read > 0) {
            continue;
        }

        // Si ya hay caracteres a medias, se espera un poco a que llegue el
        // resto de la secuencia de escape. Si no hay nada, se espera a que
        // pulse una tecla.
        DWORD wait = input_pending(&console_input) ? 30 : INFINITE;

        if (input_in_paste(&console_input)) {
            wait = 200;
        }

        DWORD result = wait_console_events(wait);

        if (result == WAIT_OBJECT_0) {
            continue;
        }

        if (result == WAIT_TIMEOUT) {
            // No llego nada: lo que este a medias se resuelve.
            input_no_more_bytes(&console_input);
            continue;
        }

        // La consola no se puede esperar: se lee directamente, que espera
        // hasta que haya algo.
        INPUT_RECORD record;
        DWORD count = 0;

        if (!ReadConsoleInput(std_input(), &record, 1, &count) || count == 0) {
            return -1;
        }

        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
            push_key(&record, event_time());
        }
    }
}

const char *platform_paste_text(void) {
    return input_paste_text(&console_input);
}

const Mouse *platform_mouse(void) {
    return input_mouse(&console_input);
}

unsigned long long platform_time(void) {
    return (unsigned long long) GetTickCount64();
}

#else

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

static struct termios term;
static struct termios raw_term;

// Los caracteres que llegan de la terminal.
static Input terminal_input;

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

// El momento actual, en milisegundos.
static unsigned long long now_ms(void) {
    struct timeval current;

    gettimeofday(&current, NULL);

    return (unsigned long long) current.tv_sec * 1000ULL + (unsigned long long) current.tv_usec / 1000ULL;
}

void platform_open_screen() {
    tcgetattr(STDIN_FILENO, &term);

    raw_term = term;
    raw_term.c_lflag &= ~(ICANON | ISIG | ECHO);
    raw_term.c_iflag &= ~(IXON | ICRNL);
    raw_term.c_cc[VMIN] = 1;
    raw_term.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

    // Los clics, en el formato de siempre: el 1000 pide los botones y el 1006
    // las coordenadas.
    fputs("\x1b[?1000h\x1b[?1006h", stdout);
    fflush(stdout);

    input_init(&terminal_input);
    // Solo en una terminal de verdad se puede saber si los caracteres
    // llegaron de golpe (un pegado) o de uno en uno (al teclear).
    terminal_input.detectBursts = isatty(STDIN_FILENO) != 0;
}

void platform_close_screen() {
    fputs("\x1b[?1000l\x1b[?1006l", stdout);
    fflush(stdout);

    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    input_free(&terminal_input);
}

// Espera a que la terminal tenga algo, con un tiempo maximo. Con un tiempo
// negativo espera sin limite.
static bool wait_input(int milliseconds) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    struct timeval current;
    struct timeval *timeout = NULL;

    if (milliseconds >= 0) {
        current.tv_sec = milliseconds / 1000;
        current.tv_usec = (milliseconds % 1000) * 1000;
        timeout = &current;
    }

    return select(STDIN_FILENO + 1, &set, NULL, NULL, timeout) > 0;
}

int platform_getkey() {
    for (;;) {
        InputEvent event;

        if (input_next(&terminal_input, &event)) {
            if (event.kind == INPUT_PASTE) {
                return KEY_PASTE;
            }

            if (event.kind == INPUT_CLICK) {
                return KEY_CLICK;
            }

            return event.key;
        }

        // Si ya hay caracteres a medias, se espera un poco a que llegue el
        // resto de la secuencia de escape. Si no hay nada, se espera a que
        // pulse una tecla.
        int wait = input_pending(&terminal_input) ? 30 : -1;

        if (input_in_paste(&terminal_input)) {
            wait = 200;
        }

        if (!wait_input(wait)) {
            // No llego nada: lo que este a medias se resuelve.
            input_no_more_bytes(&terminal_input);
            continue;
        }

        // Se leen todos los caracteres que hay: si la terminal ha pegado un
        // texto, llega todo de golpe.
        unsigned char buffer[512];
        ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (count < 1) {
            return -1;
        }

        unsigned long long time = now_ms();

        for (ssize_t i = 0; i < count; i++) {
            input_push_byte(&terminal_input, buffer[i], time);
        }
    }
}

const char *platform_paste_text(void) {
    return input_paste_text(&terminal_input);
}

const Mouse *platform_mouse(void) {
    return input_mouse(&terminal_input);
}

unsigned long long platform_time(void) {
    return now_ms();
}

#endif
