// Token.h
#pragma once

#include <stdlib.h>

typedef enum TokenType {
    TOKEN_WORD, 
    TOKEN_NEWLINE,
    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_PIPE, // |
    TOKEN_PIPE_ERR, // |&

    TOKEN_REDIR_OUT, // >
    TOKEN_REDIR_OUT_APPEND, // >>
    TOKEN_REDIR_IN, // <
    TOKEN_REDIR_ERR, // &>
    TOKEN_REDIR_ERR_APPEND, // &>>

    TOKEN_SEMI, // ;
    TOKEN_AND, // &&
    TOKEN_OR, // ||
    TOKEN_AMP, // &

    TOKEN_LPAREN,
    TOKEN_RPAREN
} TokenType;

typedef enum QuoteCount {
    QUOTE_NONE,
    QUOTE_SINGLE,
    QUOTE_DOUBLE
} QuoteCount;

typedef struct Token {
    TokenType type;
    char *text;
    QuoteCount quote;
    size_t pos;
} Token;