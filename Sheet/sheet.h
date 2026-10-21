#ifndef CELEX_SHEET_MODULE
#define CELEX_SHEET_MODULE

#include <stddef.h>
#include <stdio.h>

// La capa que une la hoja de calculo con el lenguaje de celex.
// El editor solo conoce esta interfaz: el lenguaje es un modulo que
// recibe el source de la hoja y devuelve el valor de cada celda.

typedef struct {
    char* ref;
    char* value;
} SheetValue;

typedef struct {
    SheetValue* values;
    size_t count;
} SheetResult;

// Evalua un script de celex y devuelve los valores de las celdas.
// Devuelve 0 si todo fue bien, o -1 dejando el motivo en el buffer de
// error.
int sheet_evaluate(const char* source, SheetResult* result, char* error, size_t errorSize);

// Evalua las celdas de una en una, en el orden en que se pueden calcular.
// Cada celda se compila y se ejecuta por separado, y cada una ve el valor
// de las que ya se calcularon.
//
// Si una celda no se puede calcular, las demas se calculan igual y el
// motivo queda en el buffer de error, como un aviso. Devuelve 0 si se pudo
// calcular alguna celda, y -1 si no se pudo calcular ninguna.
int sheet_evaluate_cells(const char* source, SheetResult* result, char* error, size_t errorSize);

// Redirige la salida del programa (print). Por defecto va a stdout.
void sheet_set_output(FILE* stream);

void sheet_result_free(SheetResult* result);

#endif
