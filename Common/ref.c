#include "ref.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* ref_column_name(unsigned int x) {
    char buffer[8];
    int length = 0;
    unsigned int index = x;

    do {
        buffer[length++] = (char) ('A' + (index % 26));
        index = index / 26;
    } while (index > 0);

    char *name = (char *) malloc((size_t) length + 1);
    for (int i = 0; i < length; i++) {
        name[i] = buffer[length - 1 - i];
    }
    name[length] = '\0';

    return name;
}

char* ref_to_string(unsigned int x, unsigned int y) {
    char *columns = ref_column_name(x);
    size_t size = strlen(columns);

    char *ref = (char *) malloc(size + 24);
    sprintf(ref, "%s%u", columns, y + 1);
    free(columns);

    return ref;
}

int ref_parse(const char *text, unsigned int *x, unsigned int *y) {
    if (text == NULL || *text == '\0') {
        return 0;
    }

    unsigned int column = 0;
    unsigned int row = 0;
    int index = 0;
    int digits = 0;

    while (text[index] >= 'A' && text[index] <= 'Z') {
        column = column * 26 + (unsigned int) (text[index] - 'A' + 1);
        index++;
    }

    if (index == 0) {
        return 0;
    }

    while (text[index] >= '0' && text[index] <= '9') {
        row = row * 10 + (unsigned int) (text[index] - '0');
        digits++;
        index++;
    }

    if (digits == 0 || row == 0 || text[index] != '\0') {
        return 0;
    }

    *x = column - 1;
    *y = row - 1;

    return 1;
}
