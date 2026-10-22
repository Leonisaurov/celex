// Corre el interprete sobre los casos de tests/cases y compara la salida
// con lo que hay en tests/expected.
//
// Cada caso tiene un archivo .cle y un archivo .txt con lo que se espera.
// El archivo esperado empieza con una linea con el codigo de salida que
// debe dar el interprete, y despues la salida que debe tener.
//
//   tests/runner <interprete> <directorio de pruebas>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define POPEN _popen
#define PCLOSE _pclose
#define SIN_ERRORS "2>nul"
// La entrada del interprete se deja vacia: lo que lea un `input` no depende de
// quien este por la consola.
#define SIN_ENTRADA " <nul"
// En Windows la barra se usa como separador, y cmd.exe no acepta la otra.
#define SEP "\\"
#else
#define POPEN popen
#define PCLOSE pclose
#define SIN_ERRORS "2>/dev/null"
#define SIN_ENTRADA " </dev/null"
#define SEP "/"
#endif

#define MAX_SIZE 65536

// Corre un comando y guarda su salida. Devuelve el codigo de salida.
static int run(const char* command, char* output, size_t size) {
    FILE* pipe = POPEN(command, "r");
    if (pipe == NULL) {
        snprintf(output, size, "no se pudo ejecutar el comando");
        return -1;
    }

    size_t length = 0;
    int character;

    while ((character = fgetc(pipe)) != EOF) {
        if (length + 1 < size) {
            output[length++] = (char) character;
        }
    }

    output[length] = '\0';

    return PCLOSE(pipe);
}

// Quita los espacios del final, para comparar sin el salto de linea de mas.
static void trim_end(char* text) {
    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r' ||
                          text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[--length] = '\0';
    }
}

// Quita los retornos de carro, para que los finales de linea del archivo
// esperado no importen: en Windows son CRLF y en Linux LF.
static void strip_cr(char* text) {
    size_t length = 0;

    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] != '\r') {
            text[length++] = text[i];
        }
    }

    text[length] = '\0';
}

// Quita el fin de linea de un nombre, para que los archivos con CRLF tambien
// sirvan.
static void trim_line_end(char* text) {
    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
}

// Lee un archivo entero en un buffer.
static bool read_file(const char* path, char* buffer, size_t size) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    size_t length = fread(buffer, 1, size - 1, file);
    buffer[length] = '\0';

    fclose(file);

    return true;
}

// Corre un caso y dice si salio como se esperaba.
static bool check_case(const char* interpreter, const char* directory, const char* name) {
    char source[1024];
    char expected[1024];

    snprintf(source, sizeof(source), "%s%scases%s%s.cle", directory, SEP, SEP, name);
    snprintf(expected, sizeof(expected), "%s%sexpected%s%s.txt", directory, SEP, SEP, name);

    char* wanted = (char*) malloc(MAX_SIZE);
    if (wanted == NULL || !read_file(expected, wanted, MAX_SIZE)) {
        printf("  %-16s FALTA el esperado %s\n", name, expected);
        free(wanted);
        return false;
    }

    // La primera linea del esperado es el codigo de salida. Puede llevar
    // tambien la palabra "programa", que es el modo en el que el caso se fija en
    // lo que el programa imprime, y no en las celdas que deja.
    int wantedExit = 0;
    bool programMode = false;
    char* content = wanted;
    char* newline = strchr(wanted, '\n');
    if (newline != NULL) {
        *newline = '\0';

        const char* marker = strstr(wanted, "exit:");
        if (marker != NULL) {
            wantedExit = atoi(marker + 5);
        }

        programMode = strstr(wanted, "programa") != NULL;

        content = newline + 1;
    }

    char command[2048];
#ifdef _WIN32
    // cmd.exe quita las comillas de fuera del comando, asi que se ponen dos
    // pares: uno para el comando y otro para cada programa.
    if (programMode) {
        snprintf(command, sizeof(command), "\"\"%s\" \"%s\"%s %s\"", interpreter, source, SIN_ENTRADA, SIN_ERRORS);
    } else {
        snprintf(command, sizeof(command), "\"\"%s\" \"%s\" --values%s %s\"", interpreter, source, SIN_ENTRADA, SIN_ERRORS);
    }
#else
    if (programMode) {
        snprintf(command, sizeof(command), "\"%s\" \"%s\"%s %s", interpreter, source, SIN_ENTRADA, SIN_ERRORS);
    } else {
        snprintf(command, sizeof(command), "\"%s\" \"%s\" --values%s %s", interpreter, source, SIN_ENTRADA, SIN_ERRORS);
    }
#endif

    char* output = (char*) malloc(MAX_SIZE);
    if (output == NULL) {
        free(wanted);
        return false;
    }

    int code = run(command, output, MAX_SIZE);

#ifdef _WIN32
    // En Windows el interprete devuelve -1, y el codigo de salida es el
    // mismo que en POSIX.
    if (code < 0) {
        code = 255;
    }
#else
    // En POSIX pclose devuelve el estado de la terminacion, y el codigo de
    // salida esta en los ocho bits de arriba.
    code = (code >> 8) & 0xFF;
#endif

    trim_end(output);
    trim_end(content);

    strip_cr(output);
    strip_cr(content);

    bool same = strcmp(output, content) == 0;
    bool sameCode = code == wantedExit;

    if (same && sameCode) {
        printf("  %-16s ok\n", name);
    } else {
        printf("  %-16s FALLO\n", name);

        if (!sameCode) {
            printf("      codigo de salida: %d, esperado %d\n", code, wantedExit);
        }

        if (!same) {
            printf("      esperado: %s", content);
            printf("      obtenido: %s", output);
        }
    }

    free(wanted);
    free(output);

    return same && sameCode;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Uso:\n\t%s <interprete> <directorio de pruebas>\n", argv[0]);
        return 1;
    }

    const char* interpreter = argv[1];
    const char* directory = argv[2];

    char path[1024];
    snprintf(path, sizeof(path), "%s/cases.txt", directory);

    char* manifest = (char*) malloc(MAX_SIZE);
    if (manifest == NULL || !read_file(path, manifest, MAX_SIZE)) {
        printf("No se pudo leer la lista de casos: %s\n", path);
        free(manifest);
        return 1;
    }

    int total = 0;
    int failed = 0;

    char* line = manifest;
    while (*line != '\0') {
        char* end = strchr(line, '\n');
        if (end != NULL) {
            *end = '\0';
        }

        // Se saltan las lineas vacias y los comentarios.
        if (line[0] != '\0' && line[0] != '#') {
            trim_line_end(line);

            total++;

            if (!check_case(interpreter, directory, line)) {
                failed++;
            }
        }

        if (end == NULL) {
            break;
        }

        line = end + 1;
    }

    printf("%d casos, %d fallos\n", total, failed);

    free(manifest);

    return failed == 0 ? 0 : 1;
}
