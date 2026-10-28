// Mide cuanto cuesta dibujar un fotograma.
//
// La fluidez del editor depende de esto: mientras se dibuja, el editor no
// mira el teclado, asi que un fotograma caro es una tecla que se contesta
// tarde. Se mide lo que tarda y lo que escribe, con el cursor moviendose a
// una celda vecina (el caso normal) y con la camara moviendose (todo el
// cuadro cambia).
//
//   tests/bench [<ancho> <alto>]
//
// La salida del editor va a un archivo temporal, para contar los bytes sin
// pagar la consola.

#include "cell_manager.h"
#include "keys.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static unsigned long long now_us(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;

    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);

    return (unsigned long long) counter.QuadPart * 1000000ULL / (unsigned long long) frequency.QuadPart;
#else
    struct timeval current;

    gettimeofday(&current, NULL);

    return (unsigned long long) current.tv_sec * 1000000ULL + (unsigned long long) current.tv_usec;
#endif
}

// Cuantos bytes escritos. Sirve para ver cuanto le llega a la terminal por
// fotograma, que es lo que se nota como salto cuando llega en trozos.
static long long bytes_written(FILE *file) {
#ifdef _WIN32
    return _ftelli64(file);
#else
    return ftello(file);
#endif
}

// Rellena una zona de la hoja, como la tendria quien trabaja.
static void fill(CellManager *manager, int columns, int rows) {
    char text[32];

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < columns; x++) {
            if ((x + y) % 3 == 0) {
                continue;
            }

            if (x % 5 == 0) {
                snprintf(text, sizeof(text), "%d", (x + 1) * (y + 1) * 137);
            } else {
                snprintf(text, sizeof(text), "A%d+B%d", x + 1, y + 1);
            }

            set_cell_content(manager, (unsigned int) x, (unsigned int) y, text);
        }
    }
}

static Editor editor_for(void) {
    Editor editor = { .running = true };

    snprintf(editor.path, sizeof(editor.path), "%s", "bench.cle");

    return editor;
}

// Cuanto tarda un fotograma, en microsegundos. Con dos teclas se alternan,
// para poder medir el caso en que la camara no se mueve (ir y volver de la
// celda vecina) y el caso en que si (siempre hacia el mismo lado).
static long long frame_us(CellManager *manager, int key, int other, int steps) {
    Editor editor = editor_for();
    unsigned long long start = now_us();

    for (int i = 0; i < steps; i++) {
        handle_key(&editor, manager, (i % 2 == 0) ? key : other);
        print_cell_manager(manager);
        print_status(manager, "NORMAL", editor.path);
    }

    return (long long) ((now_us() - start) / (unsigned long long) steps);
}

int main(int argc, char **argv) {
    int width = 80;
    int height = 24;

    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }

    CellManager manager = new_cell_manager((TermSize) { (uint16_t) width, (uint16_t) height });

    fill(&manager, 60, 60);

    char outPath[512];
    const char *temp = getenv("TEMP");

    if (temp == NULL) {
        temp = getenv("TMP");
    }
    if (temp == NULL) {
        temp = "/tmp";
    }

    snprintf(outPath, sizeof(outPath), "%s%ccelex-bench-out.txt", temp,
#ifdef _WIN32
             '\\'
#else
             '/'
#endif
            );

    if (freopen(outPath, "wb", stdout) == NULL) {
        fprintf(stderr, "no se puede abrir el archivo de salida\n");
        return 1;
    }

    // El primer fotograma pinta todo: no se cuenta.
    print_cell_manager(&manager);
    print_status(&manager, "NORMAL", "bench.cle");
    fflush(stdout);

    long long before = bytes_written(stdout);

    // Ir y volver a la celda de al lado: la camara no se mueve.
    long long neighbor = frame_us(&manager, 'l', 'h', 200);

    // La camara se mueve: todo el cuadro cambia de contenido.
    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->originX = 0;
    print_cell_manager(&manager);
    fflush(stdout);

    long long camera = frame_us(&manager, 'd', -1, 200);

    sheet_state(&manager)->cursor.x = 0;
    sheet_state(&manager)->cursor.y = 0;
    sheet_state(&manager)->originX = 0;
    sheet_state(&manager)->originY = 0;
    print_cell_manager(&manager);
    fflush(stdout);

    // Escribir dentro de una celda: el fotograma mas repetido.
    Editor insert = editor_for();
    handle_key(&insert, &manager, 'i');

    long long typing = frame_us(&manager, 'a', -1, 200);

    fflush(stdout);
    long long bytes = bytes_written(stdout) - before;

    fclose(stdout);
    remove(outPath);

    // El informe va a stderr: stdout es el papel donde se dibujaron los
    // fotogramas.
    fprintf(stderr, "pantalla %dx%d\n", width, height);
    fprintf(stderr, "  celda vecina   %6lld us   %6lld bytes\n", neighbor, bytes / 600);
    fprintf(stderr, "  con la camara  %6lld us\n", camera);
    fprintf(stderr, "  escribiendo    %6lld us\n", typing);

    free_cell_manager(&manager);

    return 0;
}
