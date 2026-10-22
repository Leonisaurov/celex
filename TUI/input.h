#ifndef CELEX_TUI_INPUT
#define CELEX_TUI_INPUT

#include <stdbool.h>
#include <stddef.h>

// Un evento de entrada: una tecla, o un pegado de la terminal.
typedef enum {
    INPUT_KEY,
    INPUT_PASTE,
} InputKind;

typedef struct {
    InputKind kind;
    int key;
    // El texto del pegado. Solo vale cuando el evento es un pegado.
    const char *text;
} InputEvent;

// Un caracter de la terminal, con el momento en que llego. El momento
// sirve para saber si varios caracteres llegaron juntos (un pegado) o si los
// tecleo la persona.
typedef struct {
    unsigned char byte;
    unsigned long long time;
} InputItem;

#define INPUT_MAX_ITEMS 4096

// Dos caracteres que llegan con menos de esta diferencia de tiempo (en
// milisegundos) se consideran parte del mismo pegado.
#define INPUT_PASTE_WINDOW 10

typedef struct {
    InputItem items[INPUT_MAX_ITEMS];
    size_t count;
    size_t pos;
    // No van a llegar mas caracteres ahora: lo que este a medias se resuelve.
    bool noMoreBytes;
    // Si se detectan los pegados por la velocidad de llegada de los
    // caracteres. Solo tiene sentido en una terminal de verdad.
    bool detectBursts;
    // Dentro de un pegado con marcadores (ESC[200~ ... ESC[201~).
    bool pasting;
    // Que hay que tirar los caracteres hasta el final de un pegado que se
    // entrego antes de tiempo.
    bool discardUntilPasteEnd;
    // El texto del pegado que se esta juntando.
    char *paste;
    size_t pasteLength;
    size_t pasteCapacity;
    // El texto del ultimo pegado entregado.
    char *pasteText;
} Input;

void input_init(Input *input);
void input_free(Input *input);

// Anade un caracter.
void input_push_byte(Input *input, unsigned char byte, unsigned long long time);

// Anade varios caracteres.
void input_push_bytes(Input *input, const char *bytes, size_t length, unsigned long long time);

// Dice que no van a llegar mas caracteres ahora, para que lo que este a
// medias (una secuencia de escape, por ejemplo) se pueda resolver.
void input_no_more_bytes(Input *input);

// Si quedan caracteres sin procesar.
bool input_pending(const Input *input);

// Si se esta juntando un pegado con marcadores.
bool input_in_paste(const Input *input);

// Devuelve el siguiente evento. Devuelve false si todavia no hay uno.
bool input_next(Input *input, InputEvent *event);

// El texto del ultimo pegado entregado.
const char *input_paste_text(Input *input);

#endif
