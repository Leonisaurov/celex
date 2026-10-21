#ifndef CELEX_TUI_ENTRY
#define CELEX_TUI_ENTRY

#include "string.h"

int MainStringLib(int argc, char** argv);

#define main(...) \
    main(int argc, char** argv) { \
        int _entry_status = MainStringLib(argc, argv); \
        _$free_arena(); \
        return _entry_status; \
    } \
    int MainStringLib(int argc, char** argv)

#endif
