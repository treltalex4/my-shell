// Expander.c
// Модуль раскрытия переменных окружения
// Поддерживает: $VAR, ${VAR}, $?, $$, $!
// $? - код возврата последней команды
// $$ - PID текущего shell
// $! - PID последнего фонового процесса

#include "Expander.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#define DEFAULT_BUF_SIZE 256
#define VAR_NAME_SIZE 256

extern int g_last_exit_code;   // Код возврата последней команды
extern pid_t g_last_bg_pid;    // PID последнего фонового процесса

static char *get_variable(const char *name);
static char *expand_string(const char *str);
static int buffer_append(char **buf, size_t *len, size_t *cap, const char *str);

// Раскрытие переменных во всех токенах типа TOKEN_WORD
// Проходит по массиву токенов и заменяет $VAR на значения
int expander_expand(TokenArray *tokens){
    for(size_t i = 0; i < tokens->count; i++){
        // Раскрываем только обычные слова (не операторы, не редиректы)
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

// Получение значения переменной по имени
// Обрабатывает специальные переменные ($?, $$, $!) и обычные из окружения
static char *get_variable(const char *name){
    if(!name){
        return strdup("");
    }

    // $? - код возврата последней выполненной команды
    if(strcmp(name, "?") == 0){
        char *buf = malloc(16);
        snprintf(buf, 16, "%d", g_last_exit_code);
        return buf;
    }

    // $$ - PID текущего процесса shell
    if(strcmp(name, "$") == 0){
        char *buf = malloc(16);
        snprintf(buf, 16, "%d", getpid());
        return buf;
    }

    // $! - PID последнего фонового процесса (запущенного через &)
    if(strcmp(name, "!") == 0){
        char *buf = malloc(16);
        snprintf(buf, 16, "%d", g_last_bg_pid);
        return buf;
    }

    // Обычная переменная окружения (например, $HOME, $PATH)
    char *val = getenv(name);
    return val ? strdup(val) : strdup("");
}

// Добавление строки в динамический буфер с автоматическим расширением
// Удваивает capacity при необходимости
static int buffer_append(char **buf, size_t *len, size_t *cap, const char *str){
    size_t add_len = strlen(str);

    // Расширяем буфер если не хватает места (удваиваем размер)
    while(*len +add_len + 1 > *cap) *cap *= 2;

    char *new_buf = realloc(*buf, *cap);
    if(!new_buf) return -1;
    *buf = new_buf;

    // Копируем добавляемую строку в конец буфера
    strcpy(*buf + *len, str);
    *len += add_len;

    return 0;
}

// Раскрытие переменных в строке
// Находит $VAR, ${VAR}, $?, $$, $! и заменяет на значения
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
    
    // Проходим по строке посимвольно
    while(str[i]){
        // Копируем обычные символы до символа '$'
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

        // Нашли символ '$' - начинается переменная
        if(str[i] == '$'){
            i++;

            char var_name[VAR_NAME_SIZE];
            size_t var_len = 0;

            // Формат ${VAR} - имя переменной в фигурных скобках
            if(str[i] == '{'){
                i++;
                while(str[i] && str[i] != '}'){
                    var_name[var_len++] = str[i++];
                }
                var_name[var_len] = '\0';
                i++;  // Пропускаем закрывающую '}'
            }
            // Специальная переменная $? (код возврата)
            else if(str[i] == '?'){
                var_name[0] = '?';
                var_name[1] = '\0';
                i++;
            }
            // Специальная переменная $$ (PID shell)
            else if(str[i] == '$'){
                var_name[0] = '$';
                var_name[1] = '\0';
                i++;
            }
            // Специальная переменная $! (PID последнего фонового процесса)
            else if(str[i] == '!'){
                var_name[0] = '!';
                var_name[1] = '\0';
                i++;
            } 
            // Обычная переменная $VAR (начинается с буквы или _, состоит из букв, цифр, _)
            else if(isalpha(str[i]) || str[i] == '_'){
                while(isalnum(str[i]) || str[i] == '_'){
                    var_name[var_len++] = str[i++];
                }
                var_name[var_len] = '\0';
            }
            // Не распознанный формат - оставляем '$' как есть
            else{
                result[len++] = '$';
                continue;
            }

            // Получаем значение переменной и добавляем в результат
            char *value = get_variable(var_name);
            buffer_append(&result, &len, &cap, value);
            free(value);
        }
    }

    result[len] = '\0';
    return result;
}