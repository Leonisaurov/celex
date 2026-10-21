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

#endif
