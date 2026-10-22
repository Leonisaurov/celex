// Muestra lo que el editor pinta, convertido en una pantalla de verdad.
//
// El editor escribe secuencias ANSI, y esas secuencias son lo que se ve en
// build/*.txt cuando se corre con la salida en un archivo. Leerlas a mano no
// se puede: hace falta alguien que las interprete, mueva el cursor y pinte la
// rejilla. Esto es ese alguien.
//
// Sirve para mirar el layout con los propios ojos (si la vista se desliza de
// caracter en caracter, donde queda el resaltado, como se corta una celda en
// el borde) y para comprobar los colores, que van en las secuencias.
//
//   tests/screen <editor> <teclas> [hoja] [-ancho N] [-alto N] [-color]
//
// Las teclas son un archivo con los bytes que se teclearian, igual que en
// app_test. Con -color se marcan con un numero las celdas que llevan color,
// para ver de texto lo que de otro modo habria que ver en la terminal.
//
// Ojo con -ancho y -alto: al editor se le da la entrada y la salida por
// archivos, sin consola, y sin consola no hay tamano que leer — se dibuja para
// ochenta columnas y veinticuatro filas siempre. Los dos numeros solo mandan lo
// que se recorta al mirar, que es para lo que sirven: ver el borde de la
// pantalla. Para ver el layout de una pantalla de verdad con otro tamano hay que
// mirar en la terminal, o correr las pruebas de la consola, que si usan una
// pseudoconsola con el tamano puesto.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define PATH_SEP "\\"
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#define PATH_SEP "/"
#endif

#define MAX_COLUMNS 400
#define MAX_ROWS 200

// Un caracter de la pantalla, con lo que pesa y con el color que se pidio.
typedef struct {
    char bytes[5];
    unsigned char width;
    short fg;
    short bg;
    unsigned short attrs;
} ScreenCell;

typedef struct {
    ScreenCell cells[MAX_ROWS][MAX_COLUMNS];
    int rows;
    int columns;
    int cursorX;
    int cursorY;
    short fg;
    short bg;
    unsigned short attrs;
} Screen;

static Screen screen;

static void screen_clear(void) {
    memset(screen.cells, 0, sizeof(screen.cells));
    screen.cursorX = 0;
    screen.cursorY = 0;
}

static void screen_clamp(void) {
    if (screen.cursorX < 0) screen.cursorX = 0;
    if (screen.cursorY < 0) screen.cursorY = 0;
    if (screen.cursorX >= screen.columns) screen.cursorX = screen.columns - 1;
    if (screen.cursorY >= screen.rows) screen.cursorY = screen.rows - 1;
}

// Cuantos bytes ocupa el caracter UTF-8 que empieza aqui.
static int utf8_length(unsigned char first) {
    if (first < 0x80) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    if ((first & 0xF8) == 0xF0) return 4;

    return 1;
}

static void put_glyph(const char *bytes, int length) {
    if (screen.cursorX >= screen.columns) {
        return;
    }

    ScreenCell *cell = &screen.cells[screen.cursorY][screen.cursorX];

    memset(cell, 0, sizeof(*cell));
    memcpy(cell->bytes, bytes, (size_t) length);
    cell->bytes[length] = '\0';
    cell->width = 1;
    cell->fg = screen.fg;
    cell->bg = screen.bg;
    cell->attrs = screen.attrs;

    screen.cursorX++;
}

static void erase_area(int from, int to) {
    for (int x = from; x <= to && x < screen.columns; x++) {
        if (x < 0) continue;

        memset(&screen.cells[screen.cursorY][x], 0, sizeof(ScreenCell));
    }
}

// Aplica una secuencia de escape. `params` son los numeros antes de la letra
// final, `count` cuantos hay, y `final` la letra.
static void apply_csi(const int *params, int count, char final, int privateMode) {
    int first = count > 0 ? params[0] : 0;

    switch (final) {
        case 'H':
        case 'f': {
            int row = count > 0 ? params[0] : 1;
            int column = count > 1 ? params[1] : 1;

            screen.cursorY = row - 1;
            screen.cursorX = column - 1;
            screen_clamp();
            break;
        }
        case 'A':
            screen.cursorY -= first > 0 ? first : 1;
            screen_clamp();
            break;
        case 'B':
            screen.cursorY += first > 0 ? first : 1;
            screen_clamp();
            break;
        case 'C':
            screen.cursorX += first > 0 ? first : 1;
            screen_clamp();
            break;
        case 'D':
            screen.cursorX -= first > 0 ? first : 1;
            screen_clamp();
            break;
        case 'J':
            if (first == 2 || first == 3) {
                screen_clear();
            } else if (first == 0) {
                erase_area(screen.cursorX, screen.columns - 1);

                for (int y = screen.cursorY + 1; y < screen.rows; y++) {
                    memset(screen.cells[y], 0, sizeof(screen.cells[y]));
                }
            }
            break;
        case 'K':
            if (first == 0) {
                erase_area(screen.cursorX, screen.columns - 1);
            } else if (first == 1) {
                erase_area(0, screen.cursorX);
            } else {
                erase_area(0, screen.columns - 1);
            }
            break;
        case 'P': {
            // Borra caracteres: lo que habia detras se corre a la izquierda.
            int n = first > 0 ? first : 1;

            for (int x = screen.cursorX; x < screen.columns; x++) {
                int source = x + n;

                screen.cells[screen.cursorY][x] = source < screen.columns
                    ? screen.cells[screen.cursorY][source]
                    : screen.cells[screen.cursorY][x];

                if (source >= screen.columns) {
                    memset(&screen.cells[screen.cursorY][x], 0, sizeof(ScreenCell));
                }
            }
            break;
        }
        case '@': {
            // Mete huecos: lo que habia se corre a la derecha.
            int n = first > 0 ? first : 1;

            for (int x = screen.columns - 1; x >= screen.cursorX; x--) {
                int source = x - n;

                screen.cells[screen.cursorY][x] = source >= screen.cursorX
                    ? screen.cells[screen.cursorY][source]
                    : screen.cells[screen.cursorY][x];

                if (source < screen.cursorX) {
                    memset(&screen.cells[screen.cursorY][x], 0, sizeof(ScreenCell));
                }
            }
            break;
        }
        case 'm':
            if (privateMode) {
                break;
            }

            if (count == 0) {
                screen.fg = -1;
                screen.bg = -1;
                screen.attrs = 0;
            }

            for (int i = 0; i < count; i++) {
                int code = params[i];

                if (code == 0) {
                    screen.fg = -1;
                    screen.bg = -1;
                    screen.attrs = 0;
                } else if (code == 1) {
                    screen.attrs |= 1;
                } else if (code == 3) {
                    screen.attrs |= 2;
                } else if (code == 4) {
                    screen.attrs |= 4;
                } else if (code == 7) {
                    screen.attrs |= 8;
                } else if (code == 9) {
                    screen.attrs |= 16;
                } else if (code >= 30 && code <= 37) {
                    screen.fg = (short) (code - 30 + 1);
                } else if (code == 39) {
                    screen.fg = -1;
                } else if (code >= 40 && code <= 47) {
                    screen.bg = (short) (code - 40 + 1);
                } else if (code == 49) {
                    screen.bg = -1;
                } else if (code >= 90 && code <= 97) {
                    screen.fg = (short) (code - 90 + 9);
                } else if (code >= 100 && code <= 107) {
                    screen.bg = (short) (code - 100 + 9);
                } else if (code == 38 || code == 48) {
                    // Color de 256 o de 24 bits: se guarda el indice, que es
                    // lo que interesa para comprobar el tema.
                    int kind = i + 1 < count ? params[i + 1] : 0;

                    if (kind == 5 && i + 2 < count) {
                        short value = (short) params[i + 2];

                        if (code == 38) screen.fg = (short) (256 + value);
                        else screen.bg = (short) (256 + value);

                        i += 2;
                    } else if (kind == 2 && i + 4 < count) {
                        // Se guarda el rojo, que es lo que distingue el color.
                        short value = (short) params[i + 2];

                        if (code == 38) screen.fg = (short) (512 + value);
                        else screen.bg = (short) (512 + value);

                        i += 4;
                    }
                }
            }
            break;
        case 'h':
        case 'l':
            if (privateMode && first == 1049 && final == 'h') {
                screen_clear();
            }
            break;
        default:
            break;
    }
}

static void feed(const char *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned char byte = (unsigned char) data[i];

        if (byte == 0x1b && i + 1 < length) {
            unsigned char next = (unsigned char) data[i + 1];

            if (next == '[') {
                size_t j = i + 2;
                int privateMode = 0;

                if (j < length && data[j] == '?') {
                    privateMode = 1;
                    j++;
                }

                int params[8];
                int count = 0;
                int current = 0;
                bool any = false;

                while (j < length && (data[j] < 0x40 || data[j] > 0x7E)) {
                    if (data[j] >= '0' && data[j] <= '9') {
                        current = current * 10 + (data[j] - '0');
                        any = true;
                    } else if (data[j] == ';') {
                        if (count < 8) params[count++] = current;
                        current = 0;
                        any = false;
                    }

                    j++;
                }

                if (j < length) {
                    if (any || current != 0 || count > 0) {
                        if (count < 8) params[count++] = current;
                    }

                    apply_csi(params, count, data[j], privateMode);
                    i = j;
                    continue;
                }

                i = j - 1;
                continue;
            }

            if (next == ']') {
                // OSC: hasta el BEL o el final de secuencia.
                size_t j = i + 2;

                while (j < length && data[j] != 0x07 && data[j] != '\\') {
                    j++;
                }

                i = j;
                continue;
            }

            if (next == '(' || next == '#') {
                i += 2;
                continue;
            }
        }

        if (byte == '\r') {
            screen.cursorX = 0;
            continue;
        }

        if (byte == '\n') {
            screen.cursorY++;
            screen_clamp();
            continue;
        }

        if (byte == 0x7f) {
            if (screen.cursorX > 0) screen.cursorX--;
            continue;
        }

        if (byte < 32) {
            continue;
        }

        int length8 = utf8_length(byte);

        if ((int) (i + (size_t) length8) > (int) length) {
            length8 = (int) (length - i);
        }

        put_glyph(data + i, length8);
        i += (size_t) length8 - 1;
    }
}

// Corre el editor con las teclas de un archivo y la salida en otro.
#ifdef _WIN32

static int run_editor(const char *editor, const char *sheet, const char *keys, const char *out) {
    SECURITY_ATTRIBUTES attributes;
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = NULL;
    attributes.bInheritHandle = TRUE;

    HANDLE input = CreateFile(keys, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, NULL);
    HANDLE output = CreateFile(out, GENERIC_WRITE, FILE_SHARE_READ, &attributes, CREATE_ALWAYS, 0, NULL);

    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE) {
        return -1;
    }

    STARTUPINFO info;
    memset(&info, 0, sizeof(info));
    info.cb = sizeof(info);
    info.dwFlags = STARTF_USESTDHANDLES;
    info.hStdInput = input;
    info.hStdOutput = output;
    info.hStdError = output;

    PROCESS_INFORMATION process;
    memset(&process, 0, sizeof(process));

    char command[1024];
    snprintf(command, sizeof(command), "\"%s\" \"%s\"", editor, sheet);

    if (!CreateProcess(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &info, &process)) {
        CloseHandle(input);
        CloseHandle(output);
        return -1;
    }

    WaitForSingleObject(process.hProcess, 30000);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(input);
    CloseHandle(output);

    return 0;
}

#else

static int run_editor(const char *editor, const char *sheet, const char *keys, const char *out) {
    pid_t pid = fork();

    if (pid == 0) {
        int input = open(keys, O_RDONLY);
        int output = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        dup2(input, STDIN_FILENO);
        dup2(output, STDOUT_FILENO);
        dup2(output, STDERR_FILENO);

        close(input);
        close(output);

        execl(editor, editor, sheet, (char *) NULL);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#endif

static char *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *) malloc((size_t) size + 1);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t) size, file);
    buffer[read] = '\0';
    fclose(file);

    if (length != NULL) {
        *length = read;
    }

    return buffer;
}

static void temp_path(char *into, size_t size, const char *suffix) {
    const char *temp = getenv("TEMP");

    if (temp == NULL) {
        temp = getenv("TMP");
    }
    if (temp == NULL) {
        temp = "/tmp";
    }

    snprintf(into, size, "%s%scelex-screen-%ld%s", temp, PATH_SEP, (long)
#ifdef _WIN32
             _getpid()
#else
             getpid()
#endif
             , suffix);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <editor> <teclas> [hoja] [-hasta N] [-cuadros] [-color]\n", argv[0]);
        return 1;
    }

    const char *editor = argv[1];
    const char *keys = argv[2];
    const char *sheet = "sample.cle";
    bool showColors = false;
    bool showFrames = false;
    long until = -1;

    screen.rows = 24;
    screen.columns = 80;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-ancho") == 0 && i + 1 < argc) {
            screen.columns = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-alto") == 0 && i + 1 < argc) {
            screen.rows = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-hasta") == 0 && i + 1 < argc) {
            until = atol(argv[++i]);
        } else if (strcmp(argv[i], "-cuadros") == 0) {
            showFrames = true;
        } else if (strcmp(argv[i], "-color") == 0) {
            showColors = true;
        } else {
            sheet = argv[i];
        }
    }

    if (screen.columns > MAX_COLUMNS) screen.columns = MAX_COLUMNS;
    if (screen.rows > MAX_ROWS) screen.rows = MAX_ROWS;

    char outPath[512];
    char sheetPath[512];

    temp_path(outPath, sizeof(outPath), ".out");

    // La hoja: si no se paso ninguna, se copia la que se nombro. La copia lleva
    // la extension de la original, para que un documento siga siendo un
    // documento cuando el editor lo abre por su cuenta.
    char sheetCopy[512];
    const char *extension = strrchr(sheet, '.');

    temp_path(sheetCopy, sizeof(sheetCopy), extension != NULL ? extension : ".cle");

    size_t sheetLength = 0;
    char *sheetText = read_file(sheet, &sheetLength);

    if (sheetText != NULL) {
        FILE *file = fopen(sheetCopy, "wb");
        fwrite(sheetText, 1, sheetLength, file);
        fclose(file);
        free(sheetText);
        snprintf(sheetPath, sizeof(sheetPath), "%s", sheetCopy);
    } else {
        snprintf(sheetPath, sizeof(sheetPath), "%s", sheet);
    }

    if (run_editor(editor, sheetPath, keys, outPath) != 0) {
        fprintf(stderr, "no se pudo correr el editor\n");
        return 1;
    }

    size_t length = 0;
    char *output = read_file(outPath, &length);

    if (output == NULL) {
        fprintf(stderr, "el editor no escribio nada\n");
        return 1;
    }

    // Cuantos bytes y cuantos saltos de cursor: lo que le llega a la terminal
    // por tecla, que es lo que se nota cuando va por trozos.
    if (showFrames) {
        size_t cups = 0;

        for (size_t i = 0; i + 1 < length; i++) {
            if (output[i] == 0x1b && output[i + 1] == '[') {
                size_t j = i + 2;

                while (j < length && ((output[j] >= '0' && output[j] <= '9') || output[j] == ';')) {
                    j++;
                }

                if (j < length && (output[j] == 'H' || output[j] == 'f')) {
                    cups++;
                }
            }
        }

        fprintf(stderr, "%lu bytes, %lu saltos de cursor\n", (unsigned long) length, (unsigned long) cups);
    }

    if (until >= 0 && (size_t) until < length) {
        length = (size_t) until;
    }

    screen_clear();
    feed(output, length);

    // La pantalla, tal cual.
    for (int y = 0; y < screen.rows; y++) {
        for (int x = 0; x < screen.columns; x++) {
            ScreenCell *cell = &screen.cells[y][x];

            if (cell->bytes[0] == '\0') {
                fputc(' ', stdout);
                continue;
            }

            fputs(cell->bytes, stdout);

            if (showColors) {
                // Se marca con que color esta pintada la celda: el numero es el
                // color de delante, b el de atras, i la inversion y u el
                // subrayado.
                char mark[16];
                mark[0] = '\0';

                if (cell->fg >= 0) {
                    snprintf(mark, sizeof(mark), "<%d", (int) cell->fg);

                    if (cell->bg >= 0) {
                        snprintf(mark + strlen(mark), sizeof(mark) - strlen(mark), " b%d", (int) cell->bg);
                    }

                    if (cell->attrs & 1) {
                        snprintf(mark + strlen(mark), sizeof(mark) - strlen(mark), " n");
                    }

                    if (cell->attrs & 4) {
                        snprintf(mark + strlen(mark), sizeof(mark) - strlen(mark), " u");
                    }

                    snprintf(mark + strlen(mark), sizeof(mark) - strlen(mark), ">");
                } else if (cell->bg >= 0) {
                    snprintf(mark, sizeof(mark), "<b%d>", (int) cell->bg);
                } else if (cell->attrs & 8) {
                    snprintf(mark, sizeof(mark), "<i>");
                }

                fputs(mark, stdout);
            }
        }

        fputc('\n', stdout);
    }

    free(output);
    remove(outPath);
    remove(sheetCopy);

    return 0;
}
