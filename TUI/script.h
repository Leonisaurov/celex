#ifndef CELEX_TUI_SCRIPT
#define CELEX_TUI_SCRIPT
#include <stddef.h>
#include "cell_manager.h"

#define SCRIPT_OK 0
#define SCRIPT_ERROR -1

// Ejecuta la hoja con el interprete de celex-script y guarda en cada celda
// el valor calculado. Devuelve SCRIPT_OK o SCRIPT_ERROR, y deja el motivo en
// el buffer de mensaje.
int run_script(CellManager *manager, const char *interpreter, char *message, size_t size);

// Devuelve el interprete a usar: la variable de entorno CELEXS o "celexs".
const char* script_interpreter();

#endif
