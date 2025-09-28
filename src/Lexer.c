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
static void skip_spaces_and_comments(Lexer *lexer);

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
    if (!lexer) {
        return make_error_token(0, "lexer_extract: null lexer");
    }

    skip_spaces_and_comments(lexer);

    if (lexer->pos >= lexer->len || !lexer->input) {
        return make_simple_token(TOKEN_EOF, lexer->pos);
    }

    char current = lexer->input[lexer->pos];
    size_t token_pos = lexer->pos;

    switch (current) {
    case '\0':
        return make_simple_token(TOKEN_EOF, token_pos);
    case '\r':
        lexer->pos++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '\n') {
            lexer->pos++;
        }
        return make_simple_token(TOKEN_NEWLINE, token_pos);
    case '\n':
        lexer->pos++;
        return make_simple_token(TOKEN_NEWLINE, token_pos);
    case '|':
        if ((lexer->pos + 1) < lexer->len && lexer->input[lexer->pos + 1] == '|') {
            return lexer_extract_control(lexer);
        }
        return lexer_extract_pipe(lexer);
    case '>':
    case '<':
        return lexer_extract_redir(lexer);
    case '&':
        if ((lexer->pos + 1) < lexer->len && lexer->input[lexer->pos + 1] == '>') {
            return lexer_extract_redir(lexer);
        }
        return lexer_extract_control(lexer);
    case ';':
        return lexer_extract_control(lexer);
    default:
        return lexer_extract_basic(lexer);
    }
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

static Token make_simple_token(TokenType type, size_t pos){
    Token token;
    token.type = type;
    token.quote = QUOTE_NONE;
    token.pos = pos;
    return token;
}

static void skip_spaces_and_comments(Lexer *lexer) {
    if (!lexer || !lexer->input) return;

    char c;

    while (lexer->pos < lexer->len) {
        c = lexer->input[lexer->pos];
        if (c == ' ' || c == '\t')
            lexer->pos++;
        else
            break;
    }

    if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '#') {
        lexer->pos++;
        while (lexer->pos < lexer->len) {
            c = lexer->input[lexer->pos];
            if (c == '\n' || c == '\r' || c == '\0') break;
            lexer->pos++;
        }
    }
}

static Token lexer_extract_basic(Lexer *lexer){
    if (!lexer || !lexer->input) 
        return make_error_token(0, "lexer_extract_basic: null lexer");

    size_t start = lexer->pos;

    char *buf =  NULL; 
    size_t len = 0, buf_size = 32;
    size_t quote_single = 0, quote_double = 0, in_quote_single = 0, in_quote_double = 0;

    while(lexer->pos < lexer->len){
        char c = lexer->input[lexer->pos];

        if(!in_quote_single && !in_quote_double){
            if(c == ' ' || c == '\n' || c == '\t' || c == '\0'|| c == '\r' || c == '&' || c == ';' || c == '|' || c == '<' || c == '>'){
                break;
            }

            if(c == '\\'){
                if(lexer->pos + 1 >= lexer->len){
                    free(buf);
                    return make_error_token(start, "lexer_extract_basic: hanging \\");
                }

                char n = lexer->input[lexer->pos + 1];

                if(n == '\\' || n == '"' || n == '$' || n == '`'){
                    if(len + 2 > buf_size){
                        buf_size *= 2;
                        buf = realloc(buf, buf_size);
                    }
                    buf[len++] = n;
                    lexer->pos += 2;
                    continue;
                }

                if(n == '\n'){
                    lexer->pos += 2;
                    continue;
                }
                if(n == '\r'){
                    if((lexer->pos + 2 < lexer->len) && (lexer->input[lexer->pos + 2] == '\n')){
                        lexer->pos += 3;
                    } else lexer->pos += 2;

                    continue;
                }

                if(len + 2 > buf_size){
                    buf_size *= 2;
                    buf = realloc(buf, buf_size);
                }
                buf[len++] = c;
                lexer->pos++;
                continue; 
            }
            if(len + 2 > buf_size){
                buf_size *= 2;
                buf = realloc(buf, buf_size);
            }
            buf[len++] = c;
            lexer->pos++;
            continue;
        }

    }

    Token token;
    token.type = TOKEN_WORD;
    token.text = buf;
    return token;
}
