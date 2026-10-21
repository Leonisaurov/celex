#ifndef CELEX_SCRIPT_PARSER
#define CELEX_SCRIPT_PARSER

#include <stdbool.h>

// Analiza el source y deja el bytecode en el chunk.
// Devuelve 0 si todo fue bien, 1 si hubo un error.
int parse(const char* source);

// Mensaje del ultimo error encontrado.
const char* parseError();

#endif
