#ifndef CELEX_SCRIPT_SYMTABLE
#define CELEX_SCRIPT_SYMTABLE
#include <stdlib.h>
#include "hashmap.h"

#define debug

typedef char byte;

typedef struct {
	char* txt;
	uint32_t length;
} slice;

typedef struct {
	int64_t number;
	int64_t str;
} Nav;

typedef struct {
	byte* data;
	struct hashmap_s symbols;
	uint32_t length;
	uint32_t size;
} SymTable;

uint32_t addInt64(const char* key, uint32_t len);
uint32_t addString(const char* start, uint32_t len);

int64_t getInt64(uint32_t pos);
slice getSlice(uint32_t pos);

byte initSymTable();
SymTable getSymbols();
void freeSymTable();

#ifdef debug
void printSymsea(uint32_t start);
void printSymtable();
#endif

#endif
