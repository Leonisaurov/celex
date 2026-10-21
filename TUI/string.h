#ifndef CELEX_STRING_LIBRARY
#define CELEX_STRING_LIBRARY

typedef char* string;

string $S(const char* s);
unsigned int get_len(string s);

void _$free_arena();

#endif
