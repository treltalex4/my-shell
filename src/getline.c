#include "getline.h"
#include <stdio.h>
#include <stdlib.h>

#define INITIAL_BUFFER_SIZE 128

char* my_getline(void) {
    size_t size = INITIAL_BUFFER_SIZE;
    size_t pos = 0;
    
    char *line = malloc(size);
    if (!line) {
        perror("my_getline: malloc failed");
        return NULL;
    }
    
    int c;
    
    while ((c = getchar()) != EOF) {
        if (pos + 1 >= size) {
            size *= 2;
            char *new_line = realloc(line, size);
            if (!new_line) {
                perror("my_getline: realloc failed");
                free(line);
                return NULL;
            }
            line = new_line;
        }
        
        line[pos++] = (char)c;
        
        if (c == '\n') {
            break;
        }
    }
    
    if (c == EOF && pos == 0) {
        free(line);
        return NULL;
    }
    
    if (pos >= size) {
        char *new_line = realloc(line, size + 1);
        if (!new_line) {
            perror("my_getline: realloc failed");
            free(line);
            return NULL;
        }
        line = new_line;
    }
    
    line[pos] = '\0';
    
    return line;
}