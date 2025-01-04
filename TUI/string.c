#include "string.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void** strings;
    unsigned int length;
    unsigned int size;
} StringArena;

StringArena arena = {
    .size = 0,
    .length = 0,
};

typedef struct {
    unsigned int len;
} Options;

unsigned int glen(const char* s) {
    unsigned int i = 0;

    while (s[i] != '\0') i++;

    return i;
}

void check_arena() {
    while (arena.size <= arena.length) {
        void **strings = (void**) realloc(arena.strings, sizeof(void*) * (arena.size + 5));
        if (strings != NULL) {
            arena.strings = strings;
            arena.size += 5;
        }
    }
}

void _$free_arena() {
    if (arena.strings == NULL)
        return;

    for (unsigned int i = 0; i < arena.length; i++) {
        void* str = arena.strings[i];
        if (str == NULL) continue;

        free(str);
    }

    free(arena.strings);
    arena.size = 0;
    arena.length = 0;
}

string $S(const char* s) {
    check_arena();

    if (s == NULL)
        return NULL;
    unsigned int len = glen(s);
    char *str = (char*) malloc(sizeof(Options) + sizeof(char) * (len + 1));
    arena.strings[arena.length] = str;
    arena.length++;

    Options *opt = (Options*) str;
    opt->len = len;

    str += sizeof(Options);
    memcpy(str, s, len);

    str[len] = '\0';

    return str;
}

unsigned int get_len(string s) {
    return ((Options*)(s - sizeof(Options)))->len;
}
