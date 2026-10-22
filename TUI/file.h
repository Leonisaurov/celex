#ifndef CELEX_TUI_FILE
#define CELEX_TUI_FILE
#include "cell_manager.h"

#define FILE_LOADED 1
#define FILE_NOT_FOUND 0
#define FILE_ERROR -1

int load_file(const char *path, CellManager *manager, int *ignored);

// Escribe el archivo entero, con todas las hojas.
int save_file(const char *path, CellManager *manager);

// Escribe solo la hoja que se esta viendo, que es lo que evalua el interprete.
int save_sheet_file(const char *path, CellManager *manager);

// La hoja que se esta viendo como script de celex, con una linea por celda.
char *sheet_source(CellManager *manager);

// El archivo entero: una linea de nombre por hoja, y despues sus celdas.
char *workbook_source(CellManager *manager);

#endif
