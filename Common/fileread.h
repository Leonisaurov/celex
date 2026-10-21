#ifndef CELEX_SCRIPT_UTILS_STRING
#define CELEX_SCRIPT_UTILS_STRING

#include <stdint.h>

typedef struct {
	char* label;
	uint32_t length;
} String;

String asStr(const char* text);
String readF(String path);
void freeStr(String string);

#endif
