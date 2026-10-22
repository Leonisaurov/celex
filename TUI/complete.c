#include "complete.h"

#include <string.h>
#include <ctype.h>

// Las palabras del lenguaje, que son las que mas se escriben en una celda.
static const char *keywords[] = {
    "if", "else", "while", "for", "in", "print", "input", "return", "true", "false",
};

static const int keywordCount = (int) (sizeof(keywords) / sizeof(keywords[0]));

int complete_keyword_count(void) {
    return keywordCount;
}

const char *complete_keyword(int index) {
    if (index < 0 || index >= keywordCount) {
        return NULL;
    }

    return keywords[index];
}

int complete_add(CompleteList *list, const char *word) {
    if (word[0] == '\0' || strlen(word) >= COMPLETE_WORD) {
        return 0;
    }

    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->words[i], word) == 0) {
            return 0;
        }
    }

    if (list->count >= COMPLETE_MAX) {
        return 0;
    }

    strcpy(list->words[list->count++], word);

    return 1;
}

int complete_starts_with(const char *word, const char *prefix) {
    while (*prefix != '\0') {
        if (tolower((unsigned char) *word) != tolower((unsigned char) *prefix)) {
            return 0;
        }

        word++;
        prefix++;
    }

    return 1;
}

void complete_common_prefix(const CompleteList *list, char *into, size_t size) {
    into[0] = '\0';

    if (list->count == 0) {
        return;
    }

    // Se va mirando caracter a caracter, y se para en el primero que no tengan
    // todas las palabras.
    for (size_t i = 0; i < size - 1; i++) {
        char character = list->words[0][i];

        if (character == '\0') {
            break;
        }

        for (int word = 1; word < list->count; word++) {
            if (list->words[word][i] != character) {
                into[i] = '\0';
                return;
            }
        }

        into[i] = character;
        into[i + 1] = '\0';
    }
}
