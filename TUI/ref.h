#ifndef CELEX_TUI_REF
#define CELEX_TUI_REF

char* ref_column_name(unsigned int x);
char* ref_to_string(unsigned int x, unsigned int y);
int ref_parse(const char* text, unsigned int *x, unsigned int *y);

#endif
