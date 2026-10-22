#ifndef CELEX_TUI_PLATFORM
#define CELEX_TUI_PLATFORM

#include "tui.h"

// La terminal es la unica parte del editor que depende del sistema: en POSIX
// se habla con termios y en Windows con la consola de Win32.

// Prepara la terminal: teclas sin eco y sin espera de Enter.
void platform_open_screen();

// Restaura la terminal como estaba antes de abrirla.
void platform_close_screen();

// Espera una tecla y devuelve su codigo, o -1 si no hay ninguna.
int platform_getkey();

// El texto del ultimo pegado de la terminal.
const char *platform_paste_text();

// El ultimo suceso del raton, para cuando la tecla devuelta fue KEY_CLICK.
const Mouse *platform_mouse();

// Los milisegundos desde que empezo el programa.
unsigned long long platform_time();

#endif
