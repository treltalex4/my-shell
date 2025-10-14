// Lexer.c

#include "Lexer.h"
#include "Utils.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_BUF_SIZE 32

static Token lexer_extract(Lexer *lexer);
static Token lexer_extract_basic(Lexer *lexer);
static Token lexer_extract_pipe(Lexer *lexer);
static Token lexer_extract_redir(Lexer *lexer);
static Token lexer_extract_control(Lexer *lexer);

static Token make_error_token(size_t pos, const char *message);
static Token make_simple_token(TokenType type, size_t pos);
static void skip_spaces_and_comments(Lexer *lexer);
static int lexer_grow_buffer(char **buf, size_t *buf_size, size_t required);

static int lexer_grow_buffer(char **buf, size_t *buf_size, size_t required) {
    if (buf_size_check(buf, buf_size, required)) {
        return 1;
    }
    perror("lexer_extract_basic: alloc fail");
    return 0;
}


void lexer_init(Lexer *lexer, const char *input){
    assert(lexer && "lexer_init: null ptr");

    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    return;
}

void lexer_reset(Lexer *lexer, const char *input){
    assert(lexer && "lexer_reset: null ptr");

    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    return;
}

void lexer_destroy(Lexer *lexer){
    assert(lexer && "lexer_reset: null ptr");

    lexer->input = NULL;
    lexer->pos = 0;
    lexer->len = 0;
    return;
}

Token lexer_tokenize(Lexer *lexer)
{
    assert(lexer && "lexer_tokenize: null ptr");

    assert(lexer->input && "lexer_init: null input");

    return lexer_extract(lexer);
}

static Token lexer_extract(Lexer *lexer){
    assert(lexer && "lexer_init: null lexer");

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
    case '(':
        lexer->pos++;
        return make_simple_token(TOKEN_LPAREN, token_pos);
    case ')':
        lexer->pos++;
        return make_simple_token(TOKEN_RPAREN, token_pos);
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
    token.text = NULL;
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

    size_t len = 0, buf_size = DEFAULT_BUF_SIZE;
    char *buf = malloc(buf_size); 
    if(!buf)
        return make_error_token(start, "lexer_extract_basic: alloc fail");
    QuoteCount quote_type = QUOTE_NONE, active_quote = QUOTE_NONE;
    size_t quote_start = 0;


    while(lexer->pos < lexer->len){
        char c = lexer->input[lexer->pos];

        if(active_quote == QUOTE_NONE){
            if(c == '\''){
                quote_type = QUOTE_SINGLE;
                active_quote = QUOTE_SINGLE;
                quote_start = lexer->pos;
                lexer->pos++;
                continue;
            }

            if(c == '"'){
                quote_type = QUOTE_DOUBLE;
                active_quote = QUOTE_DOUBLE;
                quote_start = lexer->pos;
                lexer->pos++;
                continue;
            }

            if(c == ' ' || c == '\n' || c == '\t' || c == '\0'|| c == '\r' || c == '&' || c == ';' || c == '|' || c == '<' || c == '>' || c == '(' || c == ')'){
                break;
            }

            if(c == '\\'){
                if(lexer->pos + 1 >= lexer->len){
                    free(buf);
                    return make_error_token(start, "lexer_extract_basic: hanging \\");
                }

                char n = lexer->input[lexer->pos + 1];

                if(n == '\\' || n == '"' || n == '$' || n == '`'){
                    if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                        free(buf);
                        return make_error_token(start, "lexer_extract_basic: alloc fail");
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

                if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                    free(buf);
                    return make_error_token(start, "lexer_extract_basic: alloc fail");
                }
                buf[len++] = n;
                lexer->pos += 2;
                continue; 
            }
            if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                free(buf);
                return make_error_token(start, "lexer_extract_basic: alloc fail");
            }
            buf[len++] = c;
            lexer->pos++;
            continue;
        }
        if(active_quote == QUOTE_SINGLE){
            if(c == '\''){
                active_quote = QUOTE_NONE;
                lexer->pos++;
                continue;
            }
            if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                free(buf);
                return make_error_token(start, "lexer_extract_basic: alloc fail");
            }
            buf[len++] = c;
            lexer->pos++;
            continue;
        }

        if(active_quote == QUOTE_DOUBLE){
            if(c == '"'){
                active_quote = QUOTE_NONE;
                lexer->pos++;
                continue;
            }

            if(c == '\\'){
                if(lexer->pos + 1 >= lexer->len){
                    free(buf);
                    return make_error_token(quote_start, "lexer_extract_basic: hanging \\");
                }

                char n = lexer->input[lexer->pos + 1];

                if(n == '\\' || n == '"' || n == '$' || n == '`'){
                    if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                        free(buf);
                        return make_error_token(start, "lexer_extract_basic: alloc fail");
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

                if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                    free(buf);
                    return make_error_token(start, "lexer_extract_basic: alloc fail");
                }
                buf[len++] = '\\';
                lexer->pos++;
                continue;
            }

            if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
                free(buf);
                return make_error_token(start, "lexer_extract_basic: alloc fail");
            }
            buf[len++] = c;
            lexer->pos++;
            continue;
        }

    }

    if(active_quote != QUOTE_NONE){
        free(buf);
        return make_error_token(quote_start, "lexer_extract_basic: unclosed quots");
    }

    if (!lexer_grow_buffer(&buf, &buf_size, len + 1)) {
        free(buf);
        return make_error_token(start, "lexer_extract_basic: alloc fail");
    }
    buf[len] = '\0';

Token token;
token.pos = start;
token.quote = quote_type;
token.type = TOKEN_WORD;
token.text = buf;
return token;
}

static Token lexer_extract_pipe(Lexer *lexer)
{
    if (!lexer || !lexer->input)
        return make_error_token(0, "lexer_extract_pipe: null lexer");

    size_t start = lexer->pos;

    if (lexer->pos < lexer->len)
        lexer->pos++;

    if (lexer->pos < lexer->len) {
        if (lexer->input[lexer->pos] == '&') {
            lexer->pos++;
            return make_simple_token(TOKEN_PIPE_ERR, start);
        }
    }

    return make_simple_token(TOKEN_PIPE, start);
}

static Token lexer_extract_redir(Lexer *lexer)
{
    if (!lexer || !lexer->input)
        return make_error_token(0, "lexer_extract_redir: null lexer");

    size_t start = lexer->pos;

    if (lexer->pos >= lexer->len)
        return make_error_token(start, "lexer_extract_redir: out of range");

    switch (lexer->input[lexer->pos]) {
    case '>':
        lexer->pos++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '>') {
            lexer->pos++;
            return make_simple_token(TOKEN_REDIR_OUT_APPEND, start);
        }
        return make_simple_token(TOKEN_REDIR_OUT, start);

    case '<':
        lexer->pos++;
        return make_simple_token(TOKEN_REDIR_IN, start);

    case '&':
        lexer->pos++;
        if (lexer->pos >= lexer->len || lexer->input[lexer->pos] != '>')
            return make_error_token(start, "lexer_extract_redir: expected '>' after '&'");

        lexer->pos++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '>') {
            lexer->pos++;
            return make_simple_token(TOKEN_REDIR_ERR_APPEND, start);
        }
        return make_simple_token(TOKEN_REDIR_ERR, start);

    default:
        return make_error_token(start, "lexer_extract_redir: unexpected char");
    }
}

void lexer_free_token(Token *token)
{
    if(!token)
        return;

    if(token->text){
        free(token->text);
        token->text = NULL;
    }
}

static Token lexer_extract_control(Lexer *lexer)
{
    if (!lexer || !lexer->input)
        return make_error_token(0, "lexer_extract_control: null lexer");

    size_t start = lexer->pos;

    if (lexer->pos >= lexer->len)
        return make_error_token(start, "lexer_extract_control: out of range");

    switch (lexer->input[lexer->pos]) {
    case '&':
        lexer->pos++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '&') {
            lexer->pos++;
            return make_simple_token(TOKEN_AND, start);
        }
        return make_simple_token(TOKEN_AMP, start);

    case ';':
        lexer->pos++;
        return make_simple_token(TOKEN_SEMI, start);

    case '|':
        lexer->pos++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '|') {
            lexer->pos++;
            return make_simple_token(TOKEN_OR, start);
        }
        return make_simple_token(TOKEN_PIPE, start);

    default:
        return make_error_token(start, "lexer_extract_control: unexpected char");
    }
}
