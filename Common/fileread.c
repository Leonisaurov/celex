#include "fileread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

String asStr(const char* text) {
	String string = {
		.label = NULL,
		.length = 0,
	};

	if (text == NULL) {
		return string;
	}

	size_t length = strlen(text);
	string.label = (char*) malloc(sizeof(char) * (length + 1));
	if (string.label == NULL) {
		return string;
	}

	memcpy(string.label, text, length + 1);
	string.length = (uint32_t) length;

	return string;
}

String readF(String path) {
	String content = {
		.label = NULL,
		.length = 0,
	};

	FILE* file = fopen(path.label, "r");
	if (file == NULL) {
		return content;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size < 0) {
		fclose(file);
		return content;
	}

	content.label = (char*) malloc(sizeof(char) * (size + 1));
	if (content.label == NULL) {
		fclose(file);
		return content;
	}

	size_t read = fread(content.label, 1, size, file);
	content.label[read] = '\0';
	content.length = (uint32_t) read;

	fclose(file);

	return content;
}

void freeStr(String string) {
	if (string.label != NULL) {
		free(string.label);
	}
}
