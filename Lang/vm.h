#ifndef CELEX_SCRIPT_VM
#define CELEX_SCRIPT_VM

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "chunck.h"
#include "hashmap.h"

typedef enum {
	VAR_NUM, NUM_DOUBLE, VAR_STR, VAR_NULO,
	VAR_BOOL, VAR_REF, VAR_RANGE
} VariableType;

typedef struct {
	VariableType type;
	union {
		char* str;
		int64_t number;
	} value;
} Variable;

typedef enum {
	VM_SUCCESS,
	VM_ERR,
} VM_RESULT;

VM_RESULT execute(const char* source);

// Como execute, pero conserva las celdas que ya tienen valor, para poder
// calcular las celdas de una en una.
VM_RESULT executeKeepingCells(const char* source);

// Vuelca las celdas calculadas con el formato de celex: A1 = 42
void printSheet();

// Las celdas calculadas, ordenadas por fila y columna.
typedef struct {
	char* ref;
	char* value;
} CellValue;

// Devuelve las celdas que la ejecucion dejo asignadas. El llamador libera
// el resultado con freeCellValues.
int sheetValues(CellValue** out, size_t* count);
void freeCellValues(CellValue* values, size_t count);

// Libera las tablas de variables y celdas.
void freeVM();

// Redirige la salida del programa (print).
void setOutput(FILE* stream);

#endif
