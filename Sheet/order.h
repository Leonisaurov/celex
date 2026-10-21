#ifndef CELEX_SHEET_ORDER
#define CELEX_SHEET_ORDER

#include <stdbool.h>
#include <stddef.h>

// Una asignacion de celda del fuente.
typedef struct {
    // La celda que asigna.
    char* ref;
    // La asignacion entera, tal cual estaba en el fuente.
    char* text;
} SheetAssignment;

// El lenguaje no tiene referencias adelantadas: una celda solo puede usar el
// valor de las celdas que ya fueron calculadas. order_source devuelve un
// fuente equivalente al original, pero con las asignaciones de celdas en el
// orden en el que se pueden calcular.
//
// Solo se reordenan las asignaciones de celdas entre si, y solo si hace
// falta: si no hay dependencias el fuente vuelve tal cual.
//
// Devuelve NULL si no hay nada que ordenar o si el fuente no se puede
// analizar; en ese caso se puede usar el original. Si las celdas se
// dependen entre si, devuelve NULL y escribe el error en error.
char* order_source(const char* source, char* error, size_t errorSize);

// Las asignaciones de celda del fuente, cada una por separado y en el orden
// en que se pueden calcular, para poder evaluarlas de una en una.
//
// Si el fuente tiene otras sentencias que no son asignaciones de celda, se
// avisa en hasOther, porque esas solo se pueden evaluar con el fuente
// entero.
//
// Devuelve NULL si no se puede analizar el fuente o si las celdas se
// dependen entre si.
SheetAssignment* order_assignments(const char* source, size_t* count, bool* hasOther);

void free_assignments(SheetAssignment* assignments, size_t count);

#endif
