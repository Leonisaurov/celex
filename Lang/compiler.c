#include "compiler.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void addContent(byte* out, byte* content, uint32_t length) {
	for (uint32_t i = 0; i < length; i++) {
		out[i] = content[i];
	}
}

COMPILER_RESULT compile(const char* source, byte** out, uint64_t *len) {
	initChunck();
	initSymTable();

	if (parse(source) != 0) {
		printf("Error de sintaxis: %s\n", parseError());
		freeSymTable();
		freeChunck();

		return COMPILER_RESULT_ERR;
	}

	Chunck inst = getChunck();
	SymTable symbols = getSymbols();

	uint64_t length = inst.len + symbols.length + sizeof(uint32_t) * 2;

	*out = (char*) malloc(sizeof(byte) * length);
	if (*out == NULL) {
		printf("Error al juntar el compilado final\n");
		freeSymTable();
		freeChunck();

		return COMPILER_RESULT_ERR;
	}
	char *outM = *out;
	*len = length;

	addContent(outM, (byte*)&symbols.length, sizeof(uint32_t));
	outM += sizeof(uint32_t);

	addContent(outM, symbols.data, symbols.length);
	outM += symbols.length;

	addContent(outM, (byte*)&inst.len, sizeof(uint32_t));
	outM += sizeof(uint32_t);

	addContent(outM, inst.op_codes, inst.len);
	outM += inst.len;

	return COMPILER_RESULT_OK;
}
