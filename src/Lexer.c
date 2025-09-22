// Lexer.c

#include "Lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Token lexer_extract(Lexer *lexer);
static Token lexer_extract_basic(Lexer *lexer);
static Token lexer_extract_pipe(Lexer *lexer);
static Token lexer_extract_redir(Lexer *lexer);
static Token lexer_extract_control(Lexer *lexer);

static Token make_error_token(size_t pos, const char *message);
static Token make_simple_token(TokenType type, size_t pos);
static void skip_whitespace_and_comments(Lexer *lexer);

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

Token lexer_tokenize(Lexer *lexer)
{
    if (!lexer) {
        perror("lexer_tokenize: null ptr");
        return make_error_token(0, "lexer_tokenize: null ptr");
    }

    if (!lexer->input) {
        perror("lexer_tokenize: null input");
        return make_error_token(lexer->pos, "lexer_tokenize: null input");
    }

    return lexer_extract(lexer);
}

static Token lexer_extract(Lexer *lexer){

}

static Token make_error_token(size_t pos, const char *message){
    Token token;
    token.type = TOKEN_ERROR;
    token.quote = QUOTE_NONE;
    token.pos = pos;

    if (message) {
        size_t len = strlen(message) + 1;
        char *copy = malloc(len);
        if (copy) {
            memcpy(copy, message, len);
            token.text = copy;
        } else {
            perror("lexer: failed to allocate error message");
            token.text = NULL;
        }
    } else {
        token.text = NULL;
    }

    return token;
}
