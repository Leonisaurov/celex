#include "script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file.h"
#include "ref.h"

const char* script_interpreter() {
    const char* interpreter = getenv("CELEXS");

    if (interpreter == NULL || interpreter[0] == '\0') {
        return "celexs";
    }

    return interpreter;
}

static char* temp_path(const char* suffix) {
    char *path = (char *) malloc(64);
    sprintf(path, "/tmp/celex-%ld%s", (long) getpid(), suffix);

    return path;
}

static char* read_file(const char* path) {
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

static void clear_values(CellManager *manager) {
    clear_cell_values(manager);
}

int run_script(CellManager *manager, const char *interpreter, char *message, size_t size) {
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
    snprintf(command, sizeof(command), "'%s' '%s' --values 2> '%s'", interpreter, script, errors);

    FILE *pipe = popen(command, "r");
    if (pipe == NULL) {
        snprintf(message, size, "cannot run the interpreter");
        remove(script);
        free(script);
        free(errors);
        return SCRIPT_ERROR;
    }

    clear_values(manager);

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

        MetaCell *meta = get_or_create_cell(manager, x, y);
        if (meta == NULL) {
            continue;
        }

        write_cell_value(meta, value);
    }

    int status = pclose(pipe);

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
