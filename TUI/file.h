#ifndef CELEX_TUI_FILE
#define CELEX_TUI_FILE
#include "cell_manager.h"

#define FILE_LOADED 1
#define FILE_NOT_FOUND 0
#define FILE_ERROR -1

int load_file(const char *path, CellManager *manager);
int save_file(const char *path, CellManager *manager);

#endif
