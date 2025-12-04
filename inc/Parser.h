//Parser.h
#pragma once

#include "AST.h"
#include "Lexer.h"

typedef struct {
    TokenArray *tokens;
    size_t pos;
} Parser;

void parser_init(Parser *parser, TokenArray *tokens);

ASTNode *parser_parse(Parser *parser);
