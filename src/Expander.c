//Expander.c
#include "Expander.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define DEFAULT_BUF_SIZE 256
#define VAR_NAME_SIZE 256

extern int g_last_exit_code;

static char *get_variable(const char *name);
static char *expand_string(const char *str);
static int buffer_append(char **buf, size_t *len, size_t *cap, const char *str);

int expander_expand(TokenArray *tokens){
    for(size_t i = 0; i < tokens->count; i++){
        if(tokens->tokens[i].type == TOKEN_WORD){
            char *expanded = expand_string(tokens->tokens[i].text);
            if(expanded){
                free(tokens->tokens[i].text);
                tokens->tokens[i].text = expanded;
            }
        }
    }
    return 0;
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

static int buffer_append(char **buf, size_t *len, size_t *cap, const char *str){
    size_t add_len = strlen(str);

    while(*len +add_len + 1 > *cap) *cap *= 2;

    char *new_buf = realloc(*buf, *cap);
    if(!new_buf) return -1;
    *buf = new_buf;

    strcpy(*buf + *len, str);
    *len += add_len;

    return 0;
}

static char *expand_string(const char *str){
    if(!str){
        return NULL;
    }

    size_t cap = DEFAULT_BUF_SIZE;
    size_t len = 0, i = 0;
    char *result = malloc(cap);
    if(!result){
        return NULL;
    }
    while(str[i]){
        while(str[i] && str[i] != '$'){
            if(len + 2 >= cap){
                cap *= 2;
                char *buf = realloc(result, cap);
                if(!buf){
                    free(result);
                    return NULL;
                }
                result = buf;
            }
            result[len++] = str[i++];
        }

        if(str[i] == '$'){
            i++;

            char var_name[VAR_NAME_SIZE];
            size_t var_len = 0;

            if(str[i] == '{'){
                i++;
                while(str[i] && str[i] != '}'){
                    var_name[var_len++] = str[i++];
                }
                var_name[var_len] = '\0';
                i++;
            }
            else if(str[i] == '?'){
                var_name[0] = '?';
                var_name[1] = '\0';
                i++;
            } 
            else if(isalpha(str[i]) || str[i] == '_'){
                while(isalnum(str[i]) || str[i] == '_'){
                    var_name[var_len++] = str[i++];
                }
                var_name[var_len] = '\0';
            }
            else{
                result[len++] = '$';
                continue;
            }

            char *value = get_variable(var_name);
            buffer_append(&result, &len, &cap, value);
            free(value);
        }
    }

    result[len] = '\0';
    return result;
}