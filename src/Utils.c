#include "Utils.h"

#include <stdio.h>

#define DEFAULT_BUF_SIZE 32

int buf_size_check(char **buf, size_t *buf_size, size_t required){
    if (!buf || !buf_size) return 0;
    
    if(required < (*buf_size)){
        return 1;
    } else{
        size_t new_cap = *buf_size ? (*buf_size * 2) : DEFAULT_BUF_SIZE;
        char *tmp = realloc(*buf, new_cap);
        if(!tmp) {
            perror("buf_size_check: realloc failed");
            return 0;
        }
        *buf = tmp;
        *buf_size = new_cap;
        return 1;
    }
}