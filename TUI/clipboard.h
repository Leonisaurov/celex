#ifndef CELEX_TUI_CLIPBOARD
#define CELEX_TUI_CLIPBOARD

#include <stdbool.h>

// El portapapeles del editor. Es propio, para que el Ctrl+C y el Ctrl+V
// trabajen siempre con lo mismo, sin depender del portapapeles del sistema.
//
// Guarda el texto que se pega y, cuando el texto viene de una celda, tambien
// de que celda viene. Eso es lo que permite marcar la celda copiada, como
// hacen las hojas de calculo.
typedef struct {
    // El texto que se pega.
    char *text;
    // El resultado de la celda copiada, si la hubo.
    char *value;
    // Si el texto viene de una celda de la hoja.
    bool hasCell;
    unsigned int cellX;
    unsigned int cellY;
} Clipboard;

void clipboard_init(Clipboard *clipboard);
void clipboard_free(Clipboard *clipboard);

// Copia un texto suelto.
void clipboard_set_text(Clipboard *clipboard, const char *text);

// Copia una celda: su formula, su valor y donde esta.
void clipboard_set_cell(Clipboard *clipboard, const char *text, const char *value, unsigned int x, unsigned int y);

bool clipboard_has_text(const Clipboard *clipboard);
const char *clipboard_text(const Clipboard *clipboard);
const char *clipboard_value(const Clipboard *clipboard);

// Si el portapapeles tiene algo, aunque sea un texto vacio (una celda vacia
// que se copio).
bool clipboard_has_content(const Clipboard *clipboard);

// Si el portapapeles viene de una celda, y de que celda.
bool clipboard_has_cell(const Clipboard *clipboard);
void clipboard_cell(const Clipboard *clipboard, unsigned int *x, unsigned int *y);

// Si el portapapeles tiene exactamente ese texto.
bool clipboard_is_text(const Clipboard *clipboard, const char *text);

// Copia el texto al portapapeles del sistema. En Windows la terminal usa el
// Ctrl+V para pegar, asi que el portapapeles del sistema tiene que tener lo
// mismo que el del editor. En los demas sistemas es solo un puente para
// poder pegar el contenido de la celda en otras programas.
void system_clipboard_set(const char *text);

// Las pruebas no deben tocar el portapapeles del sistema.
void system_clipboard_set_enabled(bool enabled);

#endif
