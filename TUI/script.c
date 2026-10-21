#include "script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define CELEX_PID() ((unsigned long) _getpid())
#define CELEX_POPEN _popen
#define CELEX_PCLOSE _pclose
#define NULL_DEVICE "NUL"
#else
#include <unistd.h>
#define CELEX_PID() ((long) getpid())
#define CELEX_POPEN popen
#define CELEX_PCLOSE pclose
#define NULL_DEVICE "/dev/null"
#endif

#include "file.h"
#include "ref.h"
#include "sheet.h"

int run_script(CellManager *manager, char *message, size_t size) {
    char *source = sheet_source(manager);
    if (source == NULL) {
        snprintf(message, size, "cannot build the script");
        return SCRIPT_ERROR;
    }

    // La salida del programa no debe romper la pantalla del editor.
    FILE *sink = fopen(NULL_DEVICE, "w");
    if (sink != NULL) {
        sheet_set_output(sink);
    }

    SheetResult result;
    char error[MESSAGE_SIZE];

    if (sheet_evaluate(source, &result, error, MESSAGE_SIZE) != 0) {
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

    snprintf(message, size, "script executed");

    return SCRIPT_OK;
}

static char *temp_path(const char *suffix) {
#ifdef _WIN32
    const char *temp = getenv("TEMP");

    if (temp == NULL || temp[0] == '\0') {
        temp = getenv("TMP");
    }

    if (temp == NULL || temp[0] == '\0') {
        temp = ".";
    }

    char *path = (char *) malloc(256);
    sprintf(path, "%s\\celex-%lu%s", temp, CELEX_PID(), suffix);

    return path;
#else
    char *path = (char *) malloc(64);
    sprintf(path, "/tmp/celex-%ld%s", CELEX_PID(), suffix);

    return path;
#endif
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }

    char *content = NULL;
    size_t length = 0;
    size_t capacity = 0;
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        size_t size = strlen(buffer);
        if (length + size + 1 > capacity) {
            capacity = capacity == 0 ? 2048 : capacity * 2;
            char *res = (char *) realloc(content, capacity);
            if (res == NULL) {
                free(content);
                fclose(file);
                return NULL;
            }
            content = res;
        }

        memcpy(content + length, buffer, size + 1);
        length += size;
    }

    fclose(file);

    return content;
}

int run_external_script(CellManager *manager, const char *interpreter, char *message, size_t size) {
    if (interpreter == NULL || interpreter[0] == '\0') {
        snprintf(message, size, "no interpreter");
        return SCRIPT_ERROR;
    }

    char *script = temp_path(".cle");
    char *errors = temp_path(".err");

    if (save_file(script, manager) != FILE_LOADED) {
        snprintf(message, size, "cannot write the script");
        free(script);
        free(errors);
        return SCRIPT_ERROR;
    }

    char command[1024];
#ifdef _WIN32
    // En Windows el comando lo interpreta cmd.exe, que usa comillas dobles.
    snprintf(command, sizeof(command), "\"%s\" \"%s\" --values 2> \"%s\"", interpreter, script, errors);
#else
    snprintf(command, sizeof(command), "'%s' '%s' --values 2> '%s'", interpreter, script, errors);
#endif

    FILE *pipe = CELEX_POPEN(command, "r");
    if (pipe == NULL) {
        snprintf(message, size, "cannot run the interpreter");
        remove(script);
        free(script);
        free(errors);
        return SCRIPT_ERROR;
    }

    clear_cell_values(manager);

    char line[4096];
    while (fgets(line, sizeof(line), pipe) != NULL) {
        char *separator = strchr(line, '=');
        if (separator == NULL) {
            continue;
        }

        *separator = '\0';
        char *reference = line;
        char *value = separator + 1;

        size_t length = strlen(reference);
        while (length > 0 && (reference[length - 1] == ' ' || reference[length - 1] == '\t' || reference[length - 1] == '\r' || reference[length - 1] == '\n')) {
            reference[--length] = '\0';
        }

        length = strlen(value);
        while (length > 0 && (value[length - 1] == ' ' || value[length - 1] == '\t' || value[length - 1] == '\r' || value[length - 1] == '\n')) {
            value[--length] = '\0';
        }

        unsigned int x = 0;
        unsigned int y = 0;
        if (!ref_parse(reference, &x, &y)) {
            continue;
        }

        set_cell_value(manager, x, y, value);
    }

    int status = CELEX_PCLOSE(pipe);

    char *error = read_file(errors);
    remove(script);
    remove(errors);
    free(script);
    free(errors);

    if (status != 0) {
        if (error != NULL && error[0] != '\0') {
            char *end = strchr(error, '\n');
            if (end != NULL) {
                *end = '\0';
            }
            snprintf(message, size, "%s", error);
        } else {
            snprintf(message, size, "the script failed");
        }
        free(error);
        return SCRIPT_ERROR;
    }

    free(error);
    snprintf(message, size, "script executed");

    return SCRIPT_OK;
}
