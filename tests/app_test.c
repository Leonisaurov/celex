// Pruebas del editor entero.
//
// Se corre el programa de verdad, con las teclas escritas en un archivo, y
// se mira lo que deja: el archivo de la hoja que guarda y lo que dibuja en
// la pantalla. Asi se comprueba lo que ve el usuario, y no solo las funciones
// por separado.
//
//   tests/app_test <editor>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#define PATH_SEP "\\"
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#define PATH_SEP "/"
#endif

static int failures = 0;

// El color de las rayas de la rejilla en el segundo tema, que es el primero con
// color. Esta en TUI/theme.c, y si alli cambia cambia aqui.
#define GRID_COLOR "\x1b[0;90m"

static void check(bool ok, const char *what) {
    if (ok) {
        return;
    }

    printf("  FALLO: %s\n", what);
    failures++;
}

static void check_has(const char *content, const char *wanted, const char *what) {
    if (content != NULL && strstr(content, wanted) != NULL) {
        return;
    }

    printf("  FALLO: %s (no se encuentra \"%s\")\n", what, wanted);
    failures++;
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
        temp = ".";
    }

    snprintf(paths[index], sizeof(paths[index]), "%s%scelex-app-test-%ld%s", temp, PATH_SEP, (long) getpid(), suffix);

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

// Lee un archivo entero.
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

#ifdef _WIN32

// Corre el editor con la entrada de un archivo y la salida en otro.
static int run_editor(const char *editor, const char *sheet, const char *keys, const char *out) {
    SECURITY_ATTRIBUTES attributes;
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = NULL;
    attributes.bInheritHandle = TRUE;

    HANDLE input = CreateFile(keys, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, NULL);
    HANDLE output = CreateFile(out, GENERIC_WRITE, FILE_SHARE_READ, &attributes, CREATE_ALWAYS, 0, NULL);

    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE) {
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
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

    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(input);
    CloseHandle(output);

    return (int) code;
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

// El portapapeles del sistema, que es donde el Windows Terminal busca lo
// que pega con el Ctrl+V.
#ifdef _WIN32

static char *read_system_clipboard(void) {
    if (!OpenClipboard(NULL)) {
        return NULL;
    }

    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    char *result = NULL;

    if (handle != NULL) {
        wchar_t *data = (wchar_t *) GlobalLock(handle);

        if (data != NULL) {
            int length = WideCharToMultiByte(CP_UTF8, 0, data, -1, NULL, 0, NULL, NULL);

            if (length > 0) {
                result = (char *) malloc((size_t) length);

                if (result != NULL) {
                    WideCharToMultiByte(CP_UTF8, 0, data, -1, result, length, NULL, NULL);
                }
            }

            GlobalUnlock(handle);
        }
    }

    CloseClipboard();

    return result;
}

#else

static char *read_system_clipboard(void) {
    // En los demas sistemas depende de como se copia al portapapeles, asi
    // que no se comprueba.
    return NULL;
}

#endif

// El camino de al lado, con otra extension: el editor abre un .cle y guarda un
// .clex, y al reves con el guion que exporta.
static void other_path(const char *path, const char *extension, char *into, size_t size) {
    const char *dot = strrchr(path, '.');
    size_t stem = dot != NULL ? (size_t) (dot - path) : strlen(path);

    snprintf(into, size, "%.*s%s", (int) stem, path, extension);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <editor>\n", argv[0]);
        return 1;
    }

    const char *editor = argv[1];

    const char *sheetPath = temp_path(0, ".cle");
    const char *keysPath = temp_path(1, ".keys");
    const char *outPath = temp_path(2, ".out");

    // La hoja empieza con una celda.
    if (!write_file(sheetPath, "A1 = 1\n", 7)) {
        printf("  FALLO: no se puede escribir el archivo de la hoja\n");
        return 1;
    }

    // Las teclas: escribir una formula, confirmar, volver a la celda,
    // copiarla con el Ctrl+C, moverse a la derecha, pegar con el Ctrl+V,
    // guardar con el Ctrl+S y salir.
    char keys[64];
    size_t length = 0;

    const char *written = "6*7";
    for (size_t i = 0; i < strlen(written); i++) {
        keys[length++] = written[i];
    }

    keys[length++] = '\r';    // Enter
    keys[length++] = 'k';     // volver a la celda de arriba
    keys[length++] = 0x03;    // Ctrl+C
    keys[length++] = 'l';     // mover a la derecha
    keys[length++] = 0x16;    // Ctrl+V
    keys[length++] = 0x02;    // Ctrl+B: negrita en la celda pegada
    keys[length++] = 0x0F;    // Ctrl+O: y alineada a la izquierda
    keys[length++] = 0x05;    // Ctrl+E: exportar el guion .cle
    keys[length++] = 0x13;    // Ctrl+S: guardar el documento .clex
    keys[length++] = 'q';     // salir
    keys[length++] = 'q';

    if (!write_file(keysPath, keys, length)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    int status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    // Lo que se guardo en el guion exportado: la celda de la que se copio y la
    // celda donde se pego. El .cle queda como un programa de celex, sin nada del
    // editor dentro.
    char *sheet = read_file(sheetPath);
    check(sheet != NULL, "se puede leer el guion exportado");

    if (sheet != NULL) {
        check_has(sheet, "A1 = 6*7", "la celda de la que se copio se guardo");
        check_has(sheet, "B1 = 6*7", "la celda donde se pego se guardo");
        check(strstr(sheet, "theme") == NULL && strstr(sheet, "cell ") == NULL,
              "el guion exportado no lleva campos del editor");
        check(strstr(sheet, "format") == NULL, "ni lleva los formatos");
    }

    free(sheet);

    // Y lo que guardo el Ctrl+S, que es el documento del editor, al lado.
    char documentPath[512];
    other_path(sheetPath, ".clex", documentPath, sizeof(documentPath));

    char *document = read_file(documentPath);
    check(document != NULL, "el editor guarda un documento al lado del guion");

    if (document != NULL) {
        check_has(document, "celex-document 1", "el documento empieza con su cabecera");
        check_has(document, "cell A1 \"6*7\"", "el documento guarda las celdas con su estructura");
        check_has(document, "format B1 align=left bold", "y guarda el formato de la celda");
    }

    free(document);
    remove(documentPath);

    // Lo que se dibujo en la pantalla: la formula pegada y el borde
    // punteado de la celda copiada.
    char *screen = read_file(outPath);
    check(screen != NULL, "se puede leer lo que dibujo el editor");

    if (screen != NULL) {
        check_has(screen, "6*7", "la formula pegada sale en la pantalla");
        check_has(screen, "\xE2\x94\x84", "la celda copiada sale con el borde punteado");
    }

    free(screen);

    // El portapapeles del sistema tiene lo mismo que el del editor: asi el
    // Windows Terminal, que usa el Ctrl+V para pegar, pega la celda copiada.
    char *system = read_system_clipboard();
    if (system != NULL) {
        check_has(system, "6*7", "el portapapeles del sistema tiene la celda copiada");
        free(system);
    }

    remove(sheetPath);
    remove(keysPath);
    remove(outPath);

    // Y ahora un pegado de la terminal: el texto llega entre los marcadores
    // que usa el pegado con marcadores.
    if (!write_file(sheetPath, "A1 = 1\n", 7)) {
        printf("  FALLO: no se puede escribir el archivo de la hoja\n");
        return 1;
    }

    char pasted[64];
    size_t pastedLength = 0;

    const char *start = "\x1b[200~";
    const char *text = "9*9";
    const char *end = "\x1b[201~";

    for (size_t i = 0; i < strlen(start); i++) {
        pasted[pastedLength++] = start[i];
    }

    for (size_t i = 0; i < strlen(text); i++) {
        pasted[pastedLength++] = text[i];
    }

    for (size_t i = 0; i < strlen(end); i++) {
        pasted[pastedLength++] = end[i];
    }

    pasted[pastedLength++] = 0x05;    // Ctrl+E: exportar el guion
    pasted[pastedLength++] = 0x13;    // Ctrl+S
    pasted[pastedLength++] = 'q';     // salir

    if (!write_file(keysPath, pasted, pastedLength)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    sheet = read_file(sheetPath);
    check(sheet != NULL, "se puede leer la hoja guardada");

    if (sheet != NULL) {
        check_has(sheet, "A1 = 9*9", "el pegado de la terminal pone el texto en la celda");
    }

    free(sheet);

    remove(sheetPath);
    remove(keysPath);
    remove(outPath);

    // Y ahora la vista: cuando el cursor se va al borde, la rejilla se
    // desplaza y tiene que ir a buscar la celda que hace falta, sin perder de
    // vista donde esta el cursor.
    const char *farSheet = "A1 = 1\nU1 = 777\n";
    size_t farLength = strlen(farSheet);

    if (!write_file(sheetPath, farSheet, farLength)) {
        printf("  FALLO: no se puede escribir el archivo de la hoja\n");
        return 1;
    }

    // Sin moverse: la celda U1 esta fuera de la pantalla, y no se dibuja.
    if (!write_file(keysPath, "q", 1)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    screen = read_file(outPath);
    check(screen != NULL, "se puede leer lo que dibujo el editor");

    if (screen != NULL) {
        check(strstr(screen, "777") == NULL, "lo que esta fuera de la vista no se dibuja");
    }

    free(screen);

    remove(outPath);

    // Veinte celdas a la derecha, y una de vuelta: el cursor se para en T1, y
    // el 777 de U1 solo puede salir si la vista se desplazo hasta ensenarla.
    char moving[64];
    size_t movingLength = 0;

    for (int i = 0; i < 20; i++) {
        moving[movingLength++] = 'l';
    }

    moving[movingLength++] = 'h';
    moving[movingLength++] = 'q';

    if (!write_file(keysPath, moving, movingLength)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    screen = read_file(outPath);
    check(screen != NULL, "se puede leer lo que dibujo el editor");

    if (screen != NULL) {
        check_has(screen, "777", "la vista se desplaza hasta la celda del cursor");
        check_has(screen, "T1", "la barra dice donde esta el cursor despues del desplazamiento");
    }

    free(screen);

    remove(sheetPath);
    remove(keysPath);
    remove(outPath);

    // Y los colores. El tema de empezar no pinta nada con color, y el Ctrl+T
    // pasa a uno que si: lo que se comprueba es que la secuencia del color de
    // las rayas llegue a la pantalla.
    if (!write_file(sheetPath, "A1 = 1\n", 7)) {
        printf("  FALLO: no se puede escribir el archivo de la hoja\n");
        return 1;
    }

    if (!write_file(keysPath, "q", 1)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    screen = read_file(outPath);

    if (screen != NULL) {
        check(strstr(screen, GRID_COLOR) == NULL, "el tema de empezar no pinta con color");
    }

    free(screen);
    remove(outPath);

    if (!write_file(keysPath, "\x14q", 2)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    screen = read_file(outPath);

    if (screen != NULL) {
        check_has(screen, GRID_COLOR, "el Ctrl+T pinta la rejilla con el color del tema");
    }

    free(screen);

    remove(sheetPath);
    remove(keysPath);
    remove(outPath);

    // Y las hojas: una hoja nueva con su celda, guardada, y el archivo abierto
    // otra vez. Las dos hojas tienen que seguir separadas, cada una con su
    // nombre en la barra de las pestañas.
    if (!write_file(sheetPath, "A1 = 1\n", 7)) {
        printf("  FALLO: no se puede escribir el archivo de la hoja\n");
        return 1;
    }

    char sheets[16];
    size_t sheetsLength = 0;

    sheets[sheetsLength++] = 0x01;              // Ctrl+A: una hoja nueva
    sheets[sheetsLength++] = '7';
    sheets[sheetsLength++] = '7';
    sheets[sheetsLength++] = 0x1b;              // fuera de la celda
    sheets[sheetsLength++] = 0x13;              // Ctrl+S: el documento
    sheets[sheetsLength++] = 'q';

    if (!write_file(keysPath, sheets, sheetsLength)) {
        printf("  FALLO: no se puede escribir el archivo de las teclas\n");
        return 1;
    }

    status = run_editor(editor, sheetPath, keysPath, outPath);
    check(status == 0, "el editor sale sin errores");

    other_path(sheetPath, ".clex", documentPath, sizeof(documentPath));
    document = read_file(documentPath);

    if (document != NULL) {
        // La hoja que se importa de un guion se llama como el archivo, para
        // reconocerla en las pestanas.
        const char *base = strrchr(sheetPath, '/');

        if (base == NULL) {
            base = strrchr(sheetPath, '\\');
        }

        base = base != NULL ? base + 1 : sheetPath;

        const char *dot = strrchr(base, '.');
        char expected[64];
        snprintf(expected, sizeof(expected), "sheet \"%.*s\"", (int) (dot - base), base);

        check_has(document, expected, "la hoja importada coge el nombre del archivo");
        check_has(document, "sheet \"Sheet2\"", "la hoja nueva sale con su nombre");
        check_has(document, "cell A1 \"77\"", "la celda de la hoja nueva se guarda");

        // La celda de la primera hoja sigue siendo la de antes: las hojas no se
        // pisan entre ellas.
        check(strstr(document, "cell A1 \"1\"") != NULL, "la primera hoja conserva su celda");
    } else {
        check(false, "el documento con dos hojas se guardo");
    }

    free(document);
    remove(documentPath);

    // Abriendo el archivo guardado otra vez, las dos pestañas se ven.
    screen = read_file(outPath);
    check(screen != NULL, "se puede leer lo que dibujo el editor");

    if (screen != NULL) {
        // La raya que separa las pestañas y la hoja que se esta viendo, tal y
        // como las pinta el tema de empezar: la raya sin color y la hoja al
        // reves. Entre medias van las secuencias, asi que se busca la linea
        // completa de bytes.
        check_has(screen, "\x1b[0m|\x1b[0;7mSheet2", "las pestañas de las hojas se dibujan");
    }

    free(screen);

    remove(sheetPath);
    remove(keysPath);
    remove(outPath);

    if (failures == 0) {
        printf("  editor: todo correcto\n");
        return 0;
    }

    printf("  editor: %d fallos\n", failures);

    return 1;
}
