#ifndef CELEX_SCRIPT_COMPILER
#define CELEX_SCRIPT_COMPILER
#include "scanner.h"
#include "chunck.h"
#include "symtable.h"

typedef enum {
	COMPILER_RESULT_OK,
	COMPILER_RESULT_ERR,
} COMPILER_RESULT;

COMPILER_RESULT compile(const char* source, byte** out, uint64_t *len);

#endif
