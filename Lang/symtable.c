#include "symtable.h"
#include <stdio.h>

SymTable table;

#ifdef debug
#include <ctype.h>

void printSymsea(uint32_t start) {
	uint32_t at = start + 40;
	if (at >= table.size) at = table.size - 1;
	printf("===Symtable Sea [%d]===\n", start);
	for (uint32_t i = start; i < at; i++) {
		if (i >= table.length)
			printf("empty, ");
		else {
			if (isprint(table.data[i])) printf("'%c', ", table.data[i]);
			else printf("%u, ", table.data[i]);
		}
		if (i - start + 1 % 10 == 0) printf("\n");
	} 
	printf("\n\n");
}

int printSymbol(void *const context, struct hashmap_element_s *const element) {
	(void) context;
	printf("\t'%.*s':\n", element->key_len,(char*) element->key);
	Nav* nav = (Nav*) element->data;
	if (nav->str != -1)
		printf("\t\tstring: %lu\n", nav->str);
	if (nav->number != -1)
		printf("\t\tnumber: %lu\n", nav->number);
	return 0;
}

void printSymtable() {
	printf("===Symbolic Table===\n");
	hashmap_iterate_pairs(&table.symbols, printSymbol, NULL);
}
#endif

byte initSymTable() {
	table.data = (byte*) malloc(sizeof(byte) * UINT8_MAX);
	if (table.data == NULL) {
		printf("La symtable no pudo ser inicializada de manera correcta\n");
		return 1;
	}
	table.size = UINT8_MAX;
	if (hashmap_create(UINT8_MAX, &table.symbols) != 0) {
		printf("Los datos de la symtable no pudieron ser alocados\n");
		return 2;
	}
	table.length = 0;
	return 0;
}

Nav* SymTableGet(const char* key, uint32_t len) {
	Nav* nav = hashmap_get(&table.symbols, key, len);
	if (nav == NULL) {
		Nav* nav = (Nav*) malloc(sizeof(Nav));
		nav->number = -1;
		nav->str = -1;
		//Error no handled
		hashmap_put(&table.symbols, key, len, nav);
		return nav;
	}
	return nav;
}

void writeData(byte* data, uint32_t len) {
	byte try = 0;
	while (table.size - table.length < len) {
		byte* res = (byte*) realloc(table.data, sizeof(byte) * table.size * 2);
		if (res != NULL) {
			table.data = res;
			table.size *= 2;
			try = 0;
			continue;
		}

		try++;
		if (try > 9) {
			printf("Error al agrandar la tabla de simbolos\n");
			return;
		}
	}

	for (uint32_t i = 0; i < len; i++) {
		table.data[table.length] = data[i];
		table.length++;
	}
}
	
uint32_t writeInt64(int64_t number) {
	uint32_t i = table.length;
	writeData((byte*)&number, sizeof(int64_t));
	return i;
}

uint32_t writeString(const char* start, uint32_t len) {
	uint32_t i = table.length;
	writeData((byte*) &len, sizeof(uint32_t));
	writeData((byte*) start, len);
	return i;
}

int64_t getInt64(uint32_t pos) {
	return *(int64_t*)(table.data + pos);
}

slice getSlice(uint32_t pos) {
	slice string;
	string.length = *(uint32_t*)(table.data + pos);
	string.txt = (char*)(table.data + pos + sizeof(uint32_t));
	return string;
}

uint32_t addInt64(const char* key, uint32_t len) {
	Nav *nav = SymTableGet(key, len);
	uint64_t number = strtoll(key, NULL, 10);
	if (nav->number == -1) {
		nav->number = writeInt64(number);
	}
	return (uint32_t) nav->number;
}

uint32_t addString(const char* start, uint32_t len) {
	Nav *nav = SymTableGet(start, len);
	if (nav->str == -1) {
		nav->str = writeString(start, len);
	}
	return nav->str;
}

int freeItems(void* const context, void *const element) {
	(void) context;
	free((byte*) element);
	return -1;
}

void freeSymTable() {
	free(table.data);
	hashmap_iterate(&table.symbols, freeItems, NULL);
	hashmap_destroy(&table.symbols);
	table.size = 0;
	table.length = 0;
}
 SymTable getSymbols() {
	 return table;
 }
