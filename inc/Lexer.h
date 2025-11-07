// Lexer.h
#pragma once

#include "Token.h"
#include <stdlib.h>

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
} Lexer;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenArray;

void token_array_init(TokenArray *array);
int token_array_push(TokenArray *array, Token token);
void token_array_free(TokenArray *array);

void lexer_init(Lexer *lexer, const char *input);
void lexer_reset(Lexer *lexer, const char *input);
void lexer_destroy(Lexer *lexer);
Token lexer_tokenize(Lexer *lexer);
void lexer_free_token(Token *token);
int lexer_tokenize_all(Lexer *lexer, TokenArray *array);