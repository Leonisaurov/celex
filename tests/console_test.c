// Pruebas del editor en una consola de verdad.
//
// El editor habla con la terminal, y hay dos caminos: cuando la entrada
// viene de una tuberia y cuando viene de una consola. El segundo es el que
// se usa todos los dias, asi que se prueba de verdad: se abre una consola
// de verdad (una pseudoconsola en Windows, un terminal en POSIX), se le
// mandan las teclas y se mira que el editor conteste y salga.
//
// En Windows las teclas con Ctrl se mandan en el modo de entrada de Win32
// (CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _), que es lo que hace el Windows
// Terminal. El Ctrl+C no llega: la consola lo convierte en una interrupcion.
//
//   tests/console_test <editor>

#ifndef _WIN32
// Para poder abrir un terminal de verdad: posix_openpt y sus amigos.
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
#include <process.h>
#define PATH_SEP "\\"
#else
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#define PATH_SEP "/"
#endif

static int failures = 0;

static void check(bool ok, const char *what) {
    if (ok) {
        return;
    }

    printf("  FALLO: %s\n", what);
    failures++;
}

// El numero del proceso, para que los archivos temporales no se mezclen.
static long current_pid(void) {
#ifdef _WIN32
    return (long) _getpid();
#else
    return (long) getpid();
#endif
}

// El camino de un archivo temporal. Cada llamada devuelve un camino
// distinto.
static const char *temp_path(int index, const char *suffix) {
    static char paths[3][512];

    const char *temp = getenv("TEMP");

    if (temp == NULL || temp[0] == '\0') {
        temp = getenv("TMP");
    }

    if (temp == NULL || temp[0] == '\0') {
        temp = "/tmp";
    }

    snprintf(paths[index], sizeof(paths[index]), "%s%scelex-console-test-%ld%s", temp, PATH_SEP, current_pid(), suffix);

    return paths[index];
}

static bool write_file(const char *path, const char *content, size_t length) {
    FILE *file = fopen(path, "wb");

    if (file == NULL) {
        return false;
    }

    size_t written = fwrite(content, 1, length, file);
    fclose(file);

    return written == length;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = (char *) malloc((size_t) size + 1);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t) size, file);
    buffer[read] = '\0';

    fclose(file);

    return buffer;
}

// Las teclas que se mandan a la consola, una por una, con una pausa entre
// ellas para que el editor las pueda ir viendo.
#ifdef _WIN32

// En Windows las teclas con Ctrl se mandan en el modo de entrada de Win32
// (CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _), que es lo que hace el Windows
// Terminal. El Ctrl+C no llega: la consola lo convierte en una interrupcion.
static const char *pieces[] = {
    // Una rafaga de caracteres: es un pegado, no son teclas tecleadas.
    "6*7",
    // Moverse a la celda de al lado.
    "l",
    // El Ctrl+V.
    "\x1b[17;29;0;1;8;1_\x1b[86;47;22;1;8;1_\x1b[86;47;22;0;8;1_\x1b[17;29;0;0;0;1_",
    // El Ctrl+E, para exportar el guion del lenguaje. VK_E es 69 y su codigo de
    // tecla 18; con Ctrl pulsado el editor saca de ahi el 5.
    "\x1b[17;29;0;1;8;1_\x1b[69;18;69;1;8;1_\x1b[69;18;0;0;8;1_\x1b[17;29;0;0;0;1_",
    // El Ctrl+S, para guardar el documento del editor.
    "\x1b[17;29;0;1;8;1_\x1b[83;31;19;1;8;1_\x1b[83;31;19;0;8;1_\x1b[17;29;0;0;0;1_",
    // Salir.
    "q",
};

#else

static const char *pieces[] = {
    // Una rafaga de caracteres: es un pegado, no son teclas tecleadas.
    "6*7",
    // Moverse a la celda de al lado.
    "l",
    // El Ctrl+V.
    "\x16",
    // El Ctrl+E, para exportar el guion del lenguaje.
    "\x05",
    // El Ctrl+S, para guardar el documento del editor.
    "\x13",
    // Salir.
    "q",
};

#endif

static const size_t pieceCount = sizeof(pieces) / sizeof(pieces[0]);

#ifdef _WIN32

// Corre el editor dentro de una pseudoconsola, que es una consola de
// verdad: el editor ve los eventos de la consola de Win32.
static int run_in_console(const char *editor, const char *sheet, char **output, size_t *outputLength) {
    SECURITY_ATTRIBUTES attributes;
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = NULL;
    attributes.bInheritHandle = TRUE;

    HANDLE inputRead = NULL;
    HANDLE inputWrite = NULL;
    HANDLE outputRead = NULL;
    HANDLE outputWrite = NULL;

    if (!CreatePipe(&inputRead, &inputWrite, &attributes, 0)) {
        return -1;
    }

    if (!CreatePipe(&outputRead, &outputWrite, &attributes, 0)) {
        CloseHandle(inputRead);
        CloseHandle(inputWrite);
        return -1;
    }

    HPCON console = NULL;
    COORD size = {80, 24};

    if (FAILED(CreatePseudoConsole(size, inputRead, outputWrite, 0, &console))) {
        CloseHandle(inputRead);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        return -1;
    }

    // La pseudoconsola ya tiene sus copias de los extremos.
    CloseHandle(inputRead);
    CloseHandle(outputWrite);

    SIZE_T attributesSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributesSize);

    LPPROC_THREAD_ATTRIBUTE_LIST attributeList = (LPPROC_THREAD_ATTRIBUTE_LIST) malloc(attributesSize);

    if (attributeList == NULL) {
        ClosePseudoConsole(console);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        return -1;
    }

    InitializeProcThreadAttributeList(attributeList, 1, 0, &attributesSize);
    UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console, sizeof(HPCON), NULL, NULL);

    STARTUPINFOEXW info;
    memset(&info, 0, sizeof(info));
    info.StartupInfo.cb = sizeof(info);
    info.lpAttributeList = attributeList;

    PROCESS_INFORMATION process;
    memset(&process, 0, sizeof(process));

    wchar_t command[1024];
    MultiByteToWideChar(CP_UTF8, 0, editor, -1, command, 1024);

    wchar_t wideSheet[512];
    MultiByteToWideChar(CP_UTF8, 0, sheet, -1, wideSheet, 512);

    wchar_t full[1200];
    _snwprintf(full, 1200, L"\"%s\" \"%s\"", command, wideSheet);

    // Los handles estandar se heredan del proceso padre, aunque no se pida
    // heredarlos. Si el padre tiene una consola, el hijo escribe en ella y
    // no en la pseudoconsola. Con los handles sin valor, el hijo usa los de
    // la pseudoconsola, que es lo que se quiere.
    HANDLE savedIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE savedOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE savedError = GetStdHandle(STD_ERROR_HANDLE);

    SetStdHandle(STD_INPUT_HANDLE, NULL);
    SetStdHandle(STD_OUTPUT_HANDLE, NULL);
    SetStdHandle(STD_ERROR_HANDLE, NULL);

    BOOL created = CreateProcessW(NULL, full, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &info.StartupInfo, &process);

    SetStdHandle(STD_INPUT_HANDLE, savedIn);
    SetStdHandle(STD_OUTPUT_HANDLE, savedOut);
    SetStdHandle(STD_ERROR_HANDLE, savedError);

    if (!created) {
        DeleteProcThreadAttributeList(attributeList);
        free(attributeList);
        ClosePseudoConsole(console);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        return -1;
    }

    DeleteProcThreadAttributeList(attributeList);
    free(attributeList);

    size_t capacity = 65536;
    size_t length = 0;
    char *buffer = (char *) malloc(capacity);

    if (buffer == NULL) {
        ClosePseudoConsole(console);
        CloseHandle(inputWrite);
        CloseHandle(outputRead);
        WaitForSingleObject(process.hProcess, 5000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return -1;
    }

    // Las teclas van a la consola, como si las tecleara la persona. La
    // entrada se deja abierta: si se cierra, la pseudoconsola manda un
    // Ctrl+C y el editor se muere.
    for (size_t i = 0; i < pieceCount; i++) {
        DWORD written = 0;
        WriteFile(inputWrite, pieces[i], (DWORD) strlen(pieces[i]), &written, NULL);

        // Se espera a que el editor vea la tecla.
        for (int tries = 0; tries < 30; tries++) {
            DWORD available = 0;

            if (PeekNamedPipe(outputRead, NULL, 0, NULL, &available, NULL) && available > 0) {
                if (length + available + 1 > capacity) {
                    capacity = (length + available + 1) * 2;
                    char *bigger = (char *) realloc(buffer, capacity);

                    if (bigger == NULL) {
                        break;
                    }

                    buffer = bigger;
                }

                DWORD read = 0;
                ReadFile(outputRead, buffer + length, available, &read, NULL);
                length += read;
                buffer[length] = '\0';
            }

            Sleep(10);
        }
    }

    // Se lee lo que quedo mientras el editor sale.
    bool running = true;

    for (int tries = 0; tries < 300 && running; tries++) {
        DWORD available = 0;

        if (PeekNamedPipe(outputRead, NULL, 0, NULL, &available, NULL) && available > 0) {
            if (length + available + 1 > capacity) {
                capacity = (length + available + 1) * 2;
                char *bigger = (char *) realloc(buffer, capacity);

                if (bigger == NULL) {
                    break;
                }

                buffer = bigger;
            }

            DWORD read = 0;
            ReadFile(outputRead, buffer + length, available, &read, NULL);
            length += read;
            buffer[length] = '\0';

            continue;
        }

        if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
            running = false;
            break;
        }

        Sleep(10);
    }

    // Al cerrar la pseudoconsola se cierra la salida: se lee lo que quedo.
    ClosePseudoConsole(console);

    for (;;) {
        DWORD available = 0;

        if (!PeekNamedPipe(outputRead, NULL, 0, NULL, &available, NULL) || available == 0) {
            DWORD read = 0;

            if (!ReadFile(outputRead, buffer + length, 4096, &read, NULL) || read == 0) {
                break;
            }

            length += read;
            continue;
        }

        if (length + available + 1 > capacity) {
            capacity = (length + available + 1) * 2;
            char *bigger = (char *) realloc(buffer, capacity);

            if (bigger == NULL) {
                break;
            }

            buffer = bigger;
        }

        DWORD read = 0;
        ReadFile(outputRead, buffer + length, available, &read, NULL);
        length += read;
    }

    buffer[length] = '\0';

    // El editor tiene que salir solo cuando le llega la q.
    DWORD waited = WaitForSingleObject(process.hProcess, 2000);

    CloseHandle(inputWrite);
    CloseHandle(outputRead);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    // Se espera a que se cierren los mangos del proceso: si no, el
    // ejecutable sigue pillado y no se puede volver a compilar.
    Sleep(300);

    if (waited != WAIT_OBJECT_0) {
        free(buffer);
        return -1;
    }

    *output = buffer;
    *outputLength = length;

    return 0;
}

#else

// Corre el editor dentro de un terminal de verdad.
static int run_in_console(const char *editor, const char *sheet, char **output, size_t *outputLength) {
    int master = posix_openpt(O_RDWR | O_NOCTTY);

    if (master < 0) {
        return -1;
    }

    if (grantpt(master) != 0 || unlockpt(master) != 0) {
        close(master);
        return -1;
    }

    char name[256];

    if (ptsname_r(master, name, sizeof(name)) != 0) {
        close(master);
        return -1;
    }

    pid_t pid = fork();

    if (pid == 0) {
        close(master);

        setsid();

        int slave = open(name, O_RDWR);

        if (slave < 0) {
            _exit(127);
        }

        ioctl(slave, TIOCSCTTY, 0);

        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);

        if (slave > STDERR_FILENO) {
            close(slave);
        }

        execl(editor, editor, sheet, (char *) NULL);
        _exit(127);
    }

    size_t capacity = 65536;
    size_t length = 0;
    char *buffer = (char *) malloc(capacity);

    if (buffer == NULL) {
        close(master);
        waitpid(pid, NULL, 0);
        return -1;
    }

    // La lectura no bloquea: si el editor se queda parado, la prueba tiene
    // que fallar, no quedarse esperando.
    int flags = fcntl(master, F_GETFL);
    fcntl(master, F_SETFL, flags | O_NONBLOCK);

    int status = 0;
    bool running = true;

    // Se espera a que el editor ponga la terminal en su modo.
    usleep(300000);

    for (size_t i = 0; i < pieceCount && running; i++) {
        if (write(master, pieces[i], strlen(pieces[i])) < 0) {
            break;
        }

        for (int tries = 0; tries < 30; tries++) {
            ssize_t got = read(master, buffer + length, capacity - length - 1);

            if (got > 0) {
                length += (size_t) got;
                buffer[length] = '\0';
            }

            pid_t done = waitpid(pid, &status, WNOHANG);

            if (done == pid) {
                running = false;
                break;
            }

            usleep(10000);
        }
    }

    for (int tries = 0; tries < 300 && running; tries++) {
        ssize_t got = read(master, buffer + length, capacity - length - 1);

        if (got > 0) {
            length += (size_t) got;
            buffer[length] = '\0';
            continue;
        }

        pid_t done = waitpid(pid, &status, WNOHANG);

        if (done == pid) {
            running = false;
            break;
        }

        usleep(10000);
    }

    // Se lee lo que quedo.
    for (;;) {
        if (length + 4096 > capacity) {
            capacity *= 2;
            char *bigger = (char *) realloc(buffer, capacity);

            if (bigger == NULL) {
                break;
            }

            buffer = bigger;
        }

        ssize_t got = read(master, buffer + length, capacity - length - 1);

        if (got <= 0) {
            break;
        }

        length += (size_t) got;
    }

    buffer[length] = '\0';

    close(master);

    // Se espera a que se cierren los mangos del proceso: si no, el
    // ejecutable sigue pillado y no se puede volver a compilar.
    usleep(300000);

    if (running) {
        waitpid(pid, &status, 0);
        free(buffer);
        return -1;
    }

    *output = buffer;
    *outputLength = length;

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#endif

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <editor>\n", argv[0]);
        return 1;
    }

    const char *editor = argv[1];

    const char *sheetPath = temp_path(0, ".cle");

    if (!write_file(sheetPath, "A1 = 1\n", 7)) {
        printf("  FALLO: no se puede escribir el archivo de la hoja\n");
        return 1;
    }

    char *output = NULL;
    size_t outputLength = 0;

    int status = run_in_console(editor, sheetPath, &output, &outputLength);

    check(status == 0, "el editor contesta y sale con la q");
    check(output != NULL && outputLength > 0, "el editor dibuja algo");

    // Lo que se guardo en la hoja: la celda donde se pego.
    char *sheet = read_file(sheetPath);

    if (sheet == NULL) {
        check(false, "se puede leer el guion exportado");
    } else {
        if (strstr(sheet, "A1 = 6*7") == NULL) {
            printf("  FALLO: la celda del pegado no se guardo\n");
            failures++;
        }

        if (strstr(sheet, "B1 = 6*7") == NULL) {
            printf("  FALLO: la celda donde se pego con el Ctrl+V no se guardo\n");
            failures++;
        }
    }

    free(sheet);

    // El Ctrl+S escribe el documento del editor al lado, con su estructura.
    char documentPath[512];
    const char *dot = strrchr(sheetPath, '.');

    snprintf(documentPath, sizeof(documentPath), "%.*s%s", (int) (dot - sheetPath), sheetPath, ".clex");

    char *document = read_file(documentPath);

    if (document == NULL) {
        check(false, "el editor guarda su documento en una consola de verdad");
    } else {
        if (strstr(document, "celex-document 1") == NULL || strstr(document, "cell A1 \"6*7\"") == NULL) {
            printf("  FALLO: el documento no trae la cabecera y las celdas\n");
            failures++;
        }
    }

    free(document);
    remove(documentPath);

    if (output != NULL && strstr(output, "6*7") == NULL) {
        printf("  FALLO: la formula pegada no sale en la pantalla\n");
        failures++;
    }

    // El pegado no es escribir: el editor se queda en el modo normal.
    if (output != NULL && strstr(output, "EDIT") != NULL) {
        printf("  FALLO: el pegado se tomo como si se estuviera escribiendo\n");
        failures++;
    }

    if (failures > 0 && output != NULL) {
        // Se enseña lo que dibujo el editor, para ver donde se quedo.
        printf("  --- pantalla ---\n%s\n  --------------\n", output);
    }

    free(output);

    remove(sheetPath);

    if (failures == 0) {
        printf("  consola: todo correcto\n");
        return 0;
    }

    printf("  consola: %d fallos\n", failures);

    return 1;
}
