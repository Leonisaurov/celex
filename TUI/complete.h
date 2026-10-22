#ifndef CELEX_TUI_COMPLETE
#define CELEX_TUI_COMPLETE

#include <stddef.h>

// El autocompletado: cuando se esta escribiendo una palabra, se ofrecen las
// palabras del lenguaje y las que ya hay en la hoja.

// Cuantas palabras se pueden ofrecer y lo que puede pesar una.
#define COMPLETE_MAX 16
#define COMPLETE_WORD 64

// La lista de palabras que empiezan como la que se esta escribiendo.
typedef struct {
    char words[COMPLETE_MAX][COMPLETE_WORD];
    int count;
} CompleteList;

// Pone en la lista la palabra, si no estaba ya y cabe. Devuelve si la guardo.
int complete_add(CompleteList *list, const char *word);

// Las palabras del lenguaje, que siempre se pueden escribir.
int complete_keyword_count(void);
const char *complete_keyword(int index);

// Si la palabra empieza por el prefijo, sin mirar si esta en mayusculas o en
// minusculas.
int complete_starts_with(const char *word, const char *prefix);

// Lo que todas las palabras de la lista tienen al principio: lo que se puede
// escribir sin equivocarse de ninguna.
void complete_common_prefix(const CompleteList *list, char *into, size_t size);

#endif
