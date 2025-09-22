// Lexer.c

#include "Lexer.h"
#include <stdio.h>
#include <string.h>

static Token lexer_extract(Lexer *lexer);
static Token lexer_extract_basic(Lexer *lexer);
static Token lexer_extract_pipe(Lexer *lexer);
static Token lexer_extract_redir(Lexer *lexer);
static Token lexer_extract_control(Lexer *lexer);

void lexer_init(Lexer *lexer, const char *input){
    if(!lexer){
        perror("lexer_init: null ptr\n");
        return;
    }

    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    return;
}

void lexer_reset(Lexer *lexer, const char *input){
    if(!lexer){
        perror("lexer_reset: null ptr\n");
        return;
    }

    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    return;
}

void lexer_destroy(Lexer *lexer){
    if(!lexer){
        perror("lexer_reset: null ptr\n");
        return;
    }

    lexer->input = NULL;
    lexer->pos = 0;
    lexer->len = 0;
    return;
}

Token lexer_tokenize(Lexer *lexer){
    
}