//getline.h
#pragma once

#include <stdlib.h>

void terminal_init(void);
void terminal_enable_raw_mode(void);
void terminal_disable_raw_mode(void);

char *my_getline(void);

char *read_full_command(void);

int has_unclosed_syntax(const char *str);

char *str_concat(char *s1, const char *s2);