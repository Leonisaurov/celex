#include "script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define NULL_DEVICE "NUL"
#else
#define NULL_DEVICE "/dev/null"
#endif

#include "file.h"
#include "ref.h"
#include "sheet.h"

// La salida del programa no debe romper la pantalla del editor. El archivo se
// abre una sola vez y se deja abierto, porque el lenguaje escribe en el.
static FILE *output_sink = NULL;

static FILE *program_output(void) {
    if (output_sink == NULL) {
        output_sink = fopen(NULL_DEVICE, "w");
    }

    return output_sink;
}

int run_script(CellManager *manager, char *message, size_t size) {
    char *source = sheet_source(manager);
    if (source == NULL) {
        snprintf(message, size, "cannot build the script");
        return SCRIPT_ERROR;
    }

    // La salida del programa no debe romper la pantalla del editor.
    FILE *sink = program_output();
    if (sink != NULL) {
        sheet_set_output(sink);
    }

    SheetResult result;
    char error[MESSAGE_SIZE];

    // Las celdas se evaluan de una en una: si una no se puede calcular, las
    // demas siguen y solo se avisa de la que fallo.
    if (sheet_evaluate_cells(source, &result, error, MESSAGE_SIZE) != 0) {
        snprintf(message, size, "%s", error);
        free(source);
        return SCRIPT_ERROR;
    }

    free(source);

    clear_cell_values(manager);

    for (size_t i = 0; i < result.count; i++) {
        unsigned int x = 0;
        unsigned int y = 0;

        if (!ref_parse(result.values[i].ref, &x, &y)) {
            continue;
        }

        set_cell_value(manager, x, y, result.values[i].value);
    }

    sheet_result_free(&result);

    // Si no hay nada que avisar, el mensaje se deja vacio: el que llama
    // decide que pone en su barra.
    if (error[0] != '\0') {
        snprintf(message, size, "%s", error);
    } else if (message != NULL && size > 0) {
        message[0] = '\0';
    }

    return SCRIPT_OK;
}

