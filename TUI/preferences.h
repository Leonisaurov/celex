#ifndef CELEX_TUI_PREFERENCES
#define CELEX_TUI_PREFERENCES

#include "cell_manager.h"

// Las preferencias del usuario: lo que se elige una vez y vale para todos los
// archivos. Viven en `$HOME/.config/celex/config`, no en el `.clex`, porque no
// son parte de la hoja sino de como se mira:
//
//   celex-preferences 1
//   theme oscuro
//   cell-size 6 3
//   tools off
//   values on
//
// - `theme`: el tema que se dejo puesto.
// - `cell-size`: el ancho y el alto de una celda, en columnas y filas.
// - `tools`: si la barra de herramientas esta puesta (`on`) o quitada (`off`).
// - `values`: si la rejilla ensena los valores (`on`) o el fuente (`off`).
//
// Una linea que no se entiende se deja estar: el archivo se puede escribir a
// mano, y un campo nuevo de una version futura no rompe los que si se conocen.

// Lee el archivo y lo aplica al gestor. Si no existe, deja lo que hubiera y
// toma eso como lo guardado, para que el primer cambio se escriba. Devuelve 1
// si lo leyo.
int preferences_load(CellManager *manager);

// Escribe lo que el gestor tiene ahora. Solo toca el archivo si algo cambio
// respecto de lo ultimo leido o escrito. Devuelve 1 si escribio.
int preferences_save(const CellManager *manager);

// Lo que llama el bucle: guarda los cambios de la pantalla, salvo mientras la
// ventana de los temas esta abierta. Ahi el tema que suena es una prueba, y se
// fija al confirmar (Enter o un clic), no al pasar por encima.
int preferences_sync(const CellManager *manager);

#endif
