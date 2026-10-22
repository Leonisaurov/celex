#include "input.h"
#include "tui.h"

#include <stdlib.h>
#include <string.h>

// Los marcadores del pegado con marcadores (bracketed paste). Cuando la
// terminal los tiene activados, el texto pegado llega entre ellos.
#define PASTE_START "\x1b[200~"
#define PASTE_END "\x1b[201~"
#define PASTE_MARKER_LENGTH 6

// Las secuencias de escape que entiende el editor.
static const struct {
    const char *sequence;
    int key;
} sequences[] = {
    { "\x1b[A", KEY_UP },
    { "\x1b[B", KEY_DOWN },
    { "\x1b[C", KEY_RIGHT },
    { "\x1b[D", KEY_LEFT },
    { "\x1b[H", KEY_HOME },
    { "\x1b[F", KEY_END },
    { "\x1b[1~", KEY_HOME },
    { "\x1b[4~", KEY_END },
    { "\x1b[3~", KEY_DELETE },
    { "\x1bOA", KEY_HOME },
    { "\x1bOF", KEY_END },
};

// Lo que puede devolver match_sequence.
#define SEQ_UNKNOWN (-1)
#define SEQ_INCOMPLETE (-2)
#define SEQ_PASTE_START (-3)
#define SEQ_PASTE_END (-4)

void input_init(Input *input) {
    memset(input, 0, sizeof(Input));
}

void input_free(Input *input) {
    free(input->paste);
    free(input->pasteText);

    input->paste = NULL;
    input->pasteText = NULL;
    input->count = 0;
    input->pos = 0;
}

// Deja espacio para un caracter mas, sacando lo que ya se consumio.
static void input_make_room(Input *input) {
    if (input->count < INPUT_MAX_ITEMS) {
        return;
    }

    if (input->pos > 0) {
        memmove(input->items, input->items + input->pos, (input->count - input->pos) * sizeof(InputItem));
        input->count -= input->pos;
        input->pos = 0;
    }

    if (input->count < INPUT_MAX_ITEMS) {
        return;
    }

    // No hay mas sitio: se tira el caracter mas viejo.
    memmove(input->items, input->items + 1, (input->count - 1) * sizeof(InputItem));
    input->count--;
}

void input_push_byte(Input *input, unsigned char byte, unsigned long long time) {
    input_make_room(input);

    InputItem *item = &input->items[input->count++];
    item->byte = byte;
    item->time = time;

    // Ya no vale la pena esperar mas: hay caracteres nuevos.
    input->noMoreBytes = false;
}

void input_push_bytes(Input *input, const char *bytes, size_t length, unsigned long long time) {
    for (size_t i = 0; i < length; i++) {
        input_push_byte(input, (unsigned char) bytes[i], time);
    }
}

void input_no_more_bytes(Input *input) {
    input->noMoreBytes = true;
}

bool input_pending(const Input *input) {
    return input->pos < input->count;
}

bool input_in_paste(const Input *input) {
    return input->pasting;
}

// Un caracter que se puede ver en la pantalla.
static bool is_printable(unsigned char byte) {
    return byte >= 32 && byte != 127;
}

// El codigo de la tecla de un caracter suelto.
static int byte_key(unsigned char byte) {
    switch (byte) {
        case '\r':
        case '\n':
            return KEY_ENTER;
        case 0x7f:
            return KEY_BACKSPACE;
        // Algunas terminales mandan el retroceso como Ctrl+H, y el Windows
        // Terminal tambien lo usa asi según como este configurado.
        case 8:
            return KEY_BACKSPACE;
        case '\t':
            return KEY_TAB;
        default:
            return (int) byte;
    }
}

// Junta texto al pegado que se esta armando.
static void paste_append(Input *input, const char *text, size_t length) {
    if (length == 0) {
        return;
    }

    if (input->pasteLength + length + 1 > input->pasteCapacity) {
        size_t capacity = input->pasteCapacity == 0 ? 256 : input->pasteCapacity;

        while (capacity < input->pasteLength + length + 1) {
            capacity *= 2;
        }

        char *buffer = (char *) realloc(input->paste, capacity);
        if (buffer == NULL) {
            return;
        }

        input->paste = buffer;
        input->pasteCapacity = capacity;
    }

    memcpy(input->paste + input->pasteLength, text, length);
    input->pasteLength += length;
    input->paste[input->pasteLength] = '\0';
}

// Busca un marcador entre los caracteres que faltan.
static size_t find_marker(const Input *input, size_t from, const char *marker, size_t length) {
    for (size_t i = from; i + length <= input->count; i++) {
        bool same = true;

        for (size_t j = 0; j < length; j++) {
            if (input->items[i + j].byte != (unsigned char) marker[j]) {
                same = false;
                break;
            }
        }

        if (same) {
            return i;
        }
    }

    return (size_t) -1;
}

// Mira los caracteres que siguen y ve si son una secuencia conocida.
// Devuelve el codigo de la tecla, o uno de los valores SEQ_*.
static int match_sequence(const Input *input, size_t *consumed) {
    char buffer[16];
    size_t length = 0;

    for (size_t i = input->pos; i < input->count && length < sizeof(buffer) - 1; i++) {
        buffer[length++] = (char) input->items[i].byte;
    }

    buffer[length] = '\0';

    if (length >= PASTE_MARKER_LENGTH && memcmp(buffer, PASTE_START, PASTE_MARKER_LENGTH) == 0) {
        *consumed = PASTE_MARKER_LENGTH;
        return SEQ_PASTE_START;
    }

    if (length >= PASTE_MARKER_LENGTH && memcmp(buffer, PASTE_END, PASTE_MARKER_LENGTH) == 0) {
        *consumed = PASTE_MARKER_LENGTH;
        return SEQ_PASTE_END;
    }

    for (size_t i = 0; i < sizeof(sequences) / sizeof(sequences[0]); i++) {
        size_t size = strlen(sequences[i].sequence);

        if (length >= size && memcmp(buffer, sequences[i].sequence, size) == 0) {
            *consumed = size;
            return sequences[i].key;
        }
    }

    // Puede que falte parte de la secuencia: si no van a llegar mas
    // caracteres, se resuelve como una tecla suelta.
    if (!input->noMoreBytes) {
        for (size_t i = 0; i < sizeof(sequences) / sizeof(sequences[0]); i++) {
            size_t size = strlen(sequences[i].sequence);

            if (length < size && length > 0 && memcmp(buffer, sequences[i].sequence, length) == 0) {
                return SEQ_INCOMPLETE;
            }
        }
    }

    *consumed = 1;

    return SEQ_UNKNOWN;
}

// Cuantos caracteres forman un pegado: los que se pueden ver, que han
// llegado juntos y que no son todos el mismo (si son todos el mismo, es que
// una tecla se esta mantendo pulsada).
static size_t paste_run(const Input *input) {
    if (!input->detectBursts) {
        return 0;
    }

    if (input->count - input->pos < 2) {
        return 0;
    }

    unsigned long long start = input->items[input->pos].time;
    unsigned char first = input->items[input->pos].byte;
    bool same = true;
    size_t run = 0;

    for (size_t i = input->pos; i < input->count; i++) {
        unsigned char byte = input->items[i].byte;

        if (!is_printable(byte)) {
            break;
        }

        if (run > 0 && input->items[i].time - start > INPUT_PASTE_WINDOW) {
            break;
        }

        if (byte != first) {
            same = false;
        }

        run++;
    }

    if (run < 2 || same) {
        return 0;
    }

    return run;
}

// Entrega el pegado que se ha juntado.
static void deliver_paste(Input *input, InputEvent *event) {
    free(input->pasteText);

    input->pasteText = input->paste;
    input->paste = NULL;
    input->pasteLength = 0;
    input->pasteCapacity = 0;

    event->kind = INPUT_PASTE;
    event->key = 0;
    event->text = input->pasteText != NULL ? input->pasteText : "";
}

bool input_next(Input *input, InputEvent *event) {
    for (;;) {
        if (input->pos >= input->count) {
            return false;
        }

        // Un pegado que se entrego antes de tiempo: se tira lo que queda
        // hasta su final, para que no salga en la pantalla.
        if (input->discardUntilPasteEnd) {
            size_t found = find_marker(input, input->pos, PASTE_END, PASTE_MARKER_LENGTH);

            if (found != (size_t) -1) {
                input->pos = found + PASTE_MARKER_LENGTH;
                input->discardUntilPasteEnd = false;
                continue;
            }

            // Si el final no llega, no se tira todo: el editor dejaria de
            // responder.
            if (input->noMoreBytes || input->count - input->pos > 16) {
                input->discardUntilPasteEnd = false;
                continue;
            }

            input->pos = input->count;
            continue;
        }

        // Dentro de un pegado con marcadores: todo es texto hasta el final.
        if (input->pasting) {
            size_t found = find_marker(input, input->pos, PASTE_END, PASTE_MARKER_LENGTH);

            if (found != (size_t) -1) {
                for (size_t i = input->pos; i < found; i++) {
                    paste_append(input, (const char *) &input->items[i].byte, 1);
                }

                input->pos = found + PASTE_MARKER_LENGTH;
                input->pasting = false;

                deliver_paste(input, event);
                return true;
            }

            if (input->noMoreBytes) {
                // El pegado no se cerro: se entrega lo que haya.
                for (size_t i = input->pos; i < input->count; i++) {
                    paste_append(input, (const char *) &input->items[i].byte, 1);
                }

                input->pos = input->count;
                input->pasting = false;
                input->discardUntilPasteEnd = true;

                deliver_paste(input, event);
                return true;
            }

            // Se quedan los ultimos caracteres, por si el final del pegado
            // esta partido en dos.
            size_t available = input->count - input->pos;
            size_t keep = available > PASTE_MARKER_LENGTH - 1 ? available - (PASTE_MARKER_LENGTH - 1) : 0;

            for (size_t i = input->pos; i < input->pos + keep; i++) {
                paste_append(input, (const char *) &input->items[i].byte, 1);
            }

            input->pos += keep;

            return false;
        }

        unsigned char byte = input->items[input->pos].byte;

        if (byte == 0x1b) {
            size_t consumed = 0;
            int result = match_sequence(input, &consumed);

            if (result == SEQ_INCOMPLETE) {
                // Falta parte de la secuencia: se espera a que llegue.
                return false;
            }

            input->pos += consumed;

            if (result == SEQ_PASTE_START) {
                input->pasting = true;
                continue;
            }

            if (result == SEQ_PASTE_END) {
                // Un pegado que se abrio y se cerro: se entrega lo que haya.
                deliver_paste(input, event);
                return true;
            }

            event->kind = INPUT_KEY;
            event->text = NULL;
            event->key = result == SEQ_UNKNOWN ? KEY_ESC : result;

            return true;
        }

        // Una rafaga de caracteres que han llegado juntos es un pegado.
        size_t run = paste_run(input);

        if (run >= 2) {
            for (size_t i = input->pos; i < input->pos + run; i++) {
                paste_append(input, (const char *) &input->items[i].byte, 1);
            }

            input->pos += run;

            deliver_paste(input, event);
            return true;
        }

        input->pos++;

        event->kind = INPUT_KEY;
        event->text = NULL;
        event->key = byte_key(byte);

        return true;
    }
}

const char *input_paste_text(Input *input) {
    return input->pasteText != NULL ? input->pasteText : "";
}
