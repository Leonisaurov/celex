#include "sheet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunck.h"
#include "compiler.h"
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

    if (compile(source, (byte**) &program, &length) != COMPILER_RESULT_OK) {
        snprintf(error, errorSize, "%s", parseError());
        return -1;
    }

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
