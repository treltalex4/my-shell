//Expander.c
#include "Expander.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern int g_last_exit_code;

static char *get_variable(const char *name);
static char *expand_string(const char *str);
static int buffer_append(char **buf, size_t *len, size_t *cap, const char *str);

int expander_expand(TokenArray *tokens){

}

static char *get_variable(const char *name){
    if(!name){
        return strdup("");
    }

    if(strcmp(name, "?") == 0){
        char *buf = malloc(16);
        snprintf(buf, 16, "%d", g_last_exit_code);
        return buf;
    }

    char *val = getenv(name);
    return val ? strdup(val) : strdup("");
}