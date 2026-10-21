#include "compiler.h"
#include "vm.h"
#include "fileread.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Usage:\n\t%s [fileName.cle] [--values]\n", argv[0]);
		return -1;
	}

	bool values = false;
	const char* path = NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--values") == 0) {
			values = true;
			continue;
		}

		path = argv[i];
	}

	if (path == NULL) {
		printf("Usage:\n\t%s [fileName.cle] [--values]\n", argv[0]);
		return -1;
	}

	String fileName = asStr(path);
	String file = readF(fileName);
	freeStr(fileName);

	if (file.label == NULL) {
		printf("No se pudo leer el archivo %s\n", path);
		return -1;
	}

	char *outcode;
	uint64_t len;

	if (values) {
		// La salida del programa va a stderr para que stdout solo traiga las celdas.
		setOutput(stderr);
	}

	COMPILER_RESULT result = compile(file.label, &outcode, &len);
	freeStr(file);

	if (result == COMPILER_RESULT_ERR) {
		return -1;
	}

	VM_RESULT vmResult = execute(outcode);

	if (values) {
		printSheet();
	}

	freeVM();
	free(outcode);
	freeSymTable();
	freeChunck();

	if (vmResult == VM_ERR) {
		return -1;
	}

	return 0;
}
