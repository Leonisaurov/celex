#include "sheet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunck.h"
#include "compiler.h"
#include "order.h"
#include "parser.h"
#include "symtable.h"
#include "vm.h"
static char* duplicate(const char* text) {
    if (text == NULL) {
        return NULL;
    }

    size_t length = strlen(text);
    char *copy = (char*) malloc(sizeof(char) * (length + 1));
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);

    return copy;
}

void sheet_set_output(FILE* stream) {
	setOutput(stream);
}

int sheet_evaluate(const char* source, SheetResult* result, char* error, size_t errorSize) {
    result->values = NULL;
    result->count = 0;

    if (source == NULL) {
        snprintf(error, errorSize, "no source");
        return -1;
    }

    char *program = NULL;
    uint64_t length = 0;

    // Las celdas se calculan en el orden en el que pueden depender unas de
    // otras: el lenguaje no tiene referencias adelantadas.
    char *ordered = order_source(source, error, errorSize);
    if (ordered == NULL && error != NULL && error[0] != '\0') {
        // Las celdas se dependen entre si: no se puede calcular la hoja.
        return -1;
    }

    const char *sheet = ordered != NULL ? ordered : source;

    if (compile(sheet, (byte**) &program, &length) != COMPILER_RESULT_OK) {
        free(ordered);
        snprintf(error, errorSize, "%s", parseError());
        return -1;
    }

    free(ordered);

    execute(program);

    CellValue *values = NULL;
    size_t count = 0;
    sheetValues(&values, &count);

    if (count > 0) {
        SheetValue *sheet = (SheetValue*) malloc(sizeof(SheetValue) * count);
        if (sheet == NULL) {
            freeCellValues(values, count);
            freeVM();
            free(program);
            freeSymTable();
            freeChunck();

            snprintf(error, errorSize, "out of memory");
            return -1;
        }

        size_t total = 0;
        for (size_t i = 0; i < count; i++) {
            sheet[total].ref = duplicate(values[i].ref);
            sheet[total].value = duplicate(values[i].value);
            total++;
        }

        result->values = sheet;
        result->count = total;
    }

    freeCellValues(values, count);

    freeVM();
    free(program);
    freeSymTable();
    freeChunck();

    return 0;
}

void sheet_result_free(SheetResult* result) {
    if (result == NULL || result->values == NULL) {
        return;
    }

    for (size_t i = 0; i < result->count; i++) {
        free(result->values[i].ref);
        free(result->values[i].value);
    }

    free(result->values);
    result->values = NULL;
    result->count = 0;
}

int sheet_evaluate_cells(const char* source, SheetResult* result, char* error, size_t errorSize) {
    result->values = NULL;
    result->count = 0;

    if (source == NULL) {
        snprintf(error, errorSize, "no source");
        return -1;
    }

    if (error != NULL && errorSize > 0) {
        error[0] = '\0';
    }

    size_t count = 0;
    bool hasOther = false;
    SheetAssignment *assignments = order_assignments(source, &count, &hasOther);

    if (assignments == NULL || hasOther) {
        // No se puede ordenar, o el fuente tiene otras sentencias: se
        // evalua entero, que es lo que sabe hacer el lenguaje.
        free_assignments(assignments, count);

        return sheet_evaluate(source, result, error, errorSize);
    }

    // Cada celda por separado, en el orden en que se pueden calcular.
    bool failed = false;
    char firstError[512];
    firstError[0] = '\0';

    int evaluated = 0;

    // Los programas se quedan vivos hasta el final: las celdas del lenguaje
    // guardan la referencia al nombre de la celda, que esta en el programa.
    byte **programs = (byte **) malloc(sizeof(byte *) * (count > 0 ? count : 1));
    size_t programCount = 0;

    if (programs == NULL) {
        free_assignments(assignments, count);

        snprintf(error, errorSize, "out of memory");
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        byte *program = NULL;
        uint64_t length = 0;

        if (compile(assignments[i].text, &program, &length) != COMPILER_RESULT_OK) {
            if (!failed) {
                failed = true;

                // El error del parser trae la linea del fuente de la celda,
                // que aqui no dice nada: lo que importa es que celda fallo.
                const char *message = strchr(parseError(), ':');
                if (message != NULL) {
                    message++;
                    while (*message == ' ') {
                        message++;
                    }
                } else {
                    message = parseError();
                }

                snprintf(firstError, sizeof(firstError), "%s: %s", assignments[i].ref, message);
            }

            // El compilador ya libero la tabla de simbolos y el trozo de
            // codigo cuando el fuente no compila.
            continue;
        }

        if (evaluated == 0) {
            // La primera celda empieza de cero.
            execute(program);
        } else {
            // Las demas ven el valor de las que ya se calcularon.
            executeKeepingCells(program);
        }

        evaluated++;

        programs[programCount++] = program;

        freeSymTable();
        freeChunck();
    }

    // Los valores de las celdas que se pudieron calcular.
    CellValue *values = NULL;
    size_t valueCount = 0;
    sheetValues(&values, &valueCount);

    if (valueCount > 0) {
        SheetValue *sheet = (SheetValue *) malloc(sizeof(SheetValue) * valueCount);
        if (sheet == NULL) {
            freeCellValues(values, valueCount);
            freeVM();
            freeSymTable();
            freeChunck();
            free_assignments(assignments, count);

            snprintf(error, errorSize, "out of memory");
            return -1;
        }

        size_t total = 0;
        for (size_t i = 0; i < valueCount; i++) {
            sheet[total].ref = duplicate(values[i].ref);
            sheet[total].value = duplicate(values[i].value);
            total++;
        }

        result->values = sheet;
        result->count = total;
    }

    freeCellValues(values, valueCount);

    // La tabla de simbolos y el trozo de codigo ya los libero el bucle, en
    // cada celda.
    freeVM();
    free_assignments(assignments, count);

    for (size_t i = 0; i < programCount; i++) {
        free(programs[i]);
    }

    free(programs);

    if (evaluated == 0) {
        snprintf(error, errorSize, "%s", firstError[0] != '\0' ? firstError : "no cells to evaluate");
        return -1;
    }

    if (failed) {
        // Las demas celdas si se calcularon: esto es un aviso, no un fallo.
        snprintf(error, errorSize, "%s", firstError);
    }

    return 0;
}
