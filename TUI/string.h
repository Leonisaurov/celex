#ifndef CELEX_STRING_LIBRARY
#define CELEX_STRING_LIBRARY

typedef char* string;

string $S(const char* s);
unsigned int get_len(string s);

void _$free_arena();

int MainStringLib(int argc, char** argv);

#define main(...) \
    main(int argc, char** argv) { \
        _$free_arena(); \
        return MainStringLib(argc, argv);\
    } \
    int MainStringLib(int argc, char** argv)
#endif
