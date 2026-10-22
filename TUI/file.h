#ifndef CELEX_TUI_FILE
#define CELEX_TUI_FILE
#include "cell_manager.h"

#define FILE_LOADED 1
#define FILE_NOT_FOUND 0
#define FILE_ERROR -1

// Abre un archivo. Si es un documento del editor (.clex) se leen las hojas, el
// tema y el tamano de las celdas; si no, se importa el guion del lenguaje como
// una sola hoja. `ignored` cuenta las lineas que no se entendieron.
int load_file(const char *path, CellManager *manager, int *ignored);

// Escribe el documento entero, en el formato .clex del editor.
int save_document(const char *path, CellManager *manager);

// Escribe la hoja que se esta viendo como guion de celex script, en el formato
// .cle, limpio de todo lo que sea del editor.
int save_script(const char *path, CellManager *manager);

// La hoja que se esta viendo como script de celex, con una linea por celda.
char *sheet_source(CellManager *manager);

#endif
