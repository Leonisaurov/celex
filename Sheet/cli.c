#include "fileread.h"
#include "sheet.h"
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

	SheetResult result;
	char error[512];

	if (values) {
		// La salida del programa va a stderr para que stdout solo traiga las celdas.
		sheet_set_output(stderr);
	}

	if (sheet_evaluate(file.label, &result, error, sizeof(error)) != 0) {
		printf("%s\n", error);
		freeStr(file);

		return -1;
	}

	freeStr(file);

	if (values) {
		for (size_t i = 0; i < result.count; i++) {
			printf("%s = %s\n", result.values[i].ref, result.values[i].value);
		}
	}

	sheet_result_free(&result);

	return 0;
}
