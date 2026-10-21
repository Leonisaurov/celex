#ifndef CELEX_SHEET_ORDER
#define CELEX_SHEET_ORDER

#include <stddef.h>

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

#endif
