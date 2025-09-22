// Lexer.h
#pragma once

#include "Token.h"
#include <stdlib.h>

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
} Lexer;

void lexer_init(Lexer *lexer, const char *input);

void lexer_reset(Lexer *lexer, const char *input);

void lexer_destroy(Lexer *lexer);

Token lexer_next_token(Lexer *lexer);

void lexer_free_token(Token *token);
