#include "clipboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Si el portapapeles del sistema se puede tocar. Las pruebas lo apagan, para
// no cambiarle el portapapeles a quien las corre.
static bool system_enabled = true;

static char *duplicate_text(const char *text) {
    if (text == NULL) {
        text = "";
    }

    size_t length = strlen(text);
    char *copy = (char *) malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);

    return copy;
}

void clipboard_init(Clipboard *clipboard) {
    clipboard->text = NULL;
    clipboard->value = NULL;
    clipboard->hasCell = false;
    clipboard->cellX = 0;
    clipboard->cellY = 0;
}

void clipboard_free(Clipboard *clipboard) {
    free(clipboard->text);
    free(clipboard->value);

    clipboard->text = NULL;
    clipboard->value = NULL;
    clipboard->hasCell = false;
}

void clipboard_set_text(Clipboard *clipboard, const char *text) {
    char *copy = duplicate_text(text);
    if (copy == NULL) {
        return;
    }

    free(clipboard->text);
    free(clipboard->value);

    clipboard->text = copy;
    clipboard->value = NULL;
    clipboard->hasCell = false;
}

void clipboard_set_cell(Clipboard *clipboard, const char *text, const char *value, unsigned int x, unsigned int y) {
    char *copy = duplicate_text(text);
    if (copy == NULL) {
        return;
    }

    char *copy_value = duplicate_text(value);

    free(clipboard->text);
    free(clipboard->value);

    clipboard->text = copy;
    clipboard->value = copy_value;
    clipboard->hasCell = true;
    clipboard->cellX = x;
    clipboard->cellY = y;
}

bool clipboard_has_text(const Clipboard *clipboard) {
    return clipboard->text != NULL && clipboard->text[0] != '\0';
}

bool clipboard_has_content(const Clipboard *clipboard) {
    return clipboard->text != NULL;
}

const char *clipboard_text(const Clipboard *clipboard) {
    if (clipboard->text == NULL) {
        return "";
    }

    return clipboard->text;
}

const char *clipboard_value(const Clipboard *clipboard) {
    if (clipboard->value == NULL) {
        return "";
    }

    return clipboard->value;
}

bool clipboard_has_cell(const Clipboard *clipboard) {
    return clipboard->hasCell;
}

// Se quita la marca de la celda copiada, dejando lo copiado. Es lo que pasa al
// cambiar de hoja: la celda de otra hoja no se puede marcar aqui.
void clipboard_forget_cell(Clipboard *clipboard) {
    clipboard->hasCell = false;
    clipboard->cellX = 0;
    clipboard->cellY = 0;
}

void clipboard_cell(const Clipboard *clipboard, unsigned int *x, unsigned int *y) {
    if (x != NULL) {
        *x = clipboard->cellX;
    }

    if (y != NULL) {
        *y = clipboard->cellY;
    }
}

bool clipboard_is_text(const Clipboard *clipboard, const char *text) {
    if (text == NULL) {
        return false;
    }

    return strcmp(clipboard_text(clipboard), text) == 0;
}

void system_clipboard_set_enabled(bool enabled) {
    system_enabled = enabled;
}

#ifdef _WIN32

void system_clipboard_set(const char *text) {
    if (!system_enabled || text == NULL) {
        return;
    }

    int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (length <= 0) {
        return;
    }

    wchar_t *wide = (wchar_t *) malloc((size_t) length * sizeof(wchar_t));
    if (wide == NULL) {
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, length);

    if (!OpenClipboard(NULL)) {
        free(wide);
        return;
    }

    EmptyClipboard();

    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (size_t) length * sizeof(wchar_t));

    if (handle != NULL) {
        wchar_t *data = (wchar_t *) GlobalLock(handle);

        if (data != NULL) {
            memcpy(data, wide, (size_t) length * sizeof(wchar_t));
            GlobalUnlock(handle);
            SetClipboardData(CF_UNICODETEXT, handle);
        } else {
            GlobalFree(handle);
        }
    }

    CloseClipboard();

    free(wide);
}

#else

void system_clipboard_set(const char *text) {
    if (!system_enabled || text == NULL) {
        return;
    }

    // Cada terminal tiene su programa para copiar al portapapeles. Se prueban
    // en orden y se para en el primero que funciona.
    static const char *commands[] = {
        "xclip -selection clipboard 2>/dev/null",
        "wl-copy 2>/dev/null",
        "pbcopy 2>/dev/null",
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        FILE *pipe = popen(commands[i], "w");
        if (pipe == NULL) {
            continue;
        }

        fputs(text, pipe);
        fflush(pipe);

        // pclose cierra la tuberia y devuelve el codigo de salida del
        // comando.
        int status = pclose(pipe);
        if (status == 0) {
            return;
        }
    }
}

#endif
