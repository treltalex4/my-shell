#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "Lexer.h"

static const char *token_type_to_str(TokenType type){
    switch(type){
    case TOKEN_WORD: return "WORD";
    case TOKEN_NEWLINE: return "NEWLINE";
    case TOKEN_EOF: return "EOF";
    case TOKEN_ERROR: return "ERROR";
    case TOKEN_PIPE: return "PIPE";
    case TOKEN_PIPE_ERR: return "PIPE_ERR";
    case TOKEN_REDIR_OUT: return "REDIR_OUT";
    case TOKEN_REDIR_OUT_APPEND: return "REDIR_OUT_APPEND";
    case TOKEN_REDIR_IN: return "REDIR_IN";
    case TOKEN_REDIR_ERR: return "REDIR_ERR";
    case TOKEN_REDIR_ERR_APPEND: return "REDIR_ERR_APPEND";
    case TOKEN_SEMI: return "SEMI";
    case TOKEN_AND: return "AND";
    case TOKEN_OR: return "OR";
    case TOKEN_AMP: return "AMP";
    case TOKEN_LPAREN: return "LPAREN";
    case TOKEN_RPAREN: return "RPAREN";
    default: return "UNKNOWN";
    }
}

static const char *quote_to_str(QuoteCount quote){
    switch(quote){
    case QUOTE_NONE: return "NONE";
    case QUOTE_SINGLE: return "SINGLE";
    case QUOTE_DOUBLE: return "DOUBLE";
    default: return "?";
    }
}

static void print_token(const Token *token){
    if(!token) return;

    const char *type = token_type_to_str(token->type);
    const char *quote = quote_to_str(token->quote);

    printf("%s @%zu", type, token->pos);

    if(token->text){
        printf(" text=\"%s\"", token->text);
    }

    if(token->quote != QUOTE_NONE){
        printf(" quote=%s", quote);
    }

    putchar('\n');
}

int main(){
    Lexer lexer;
    char *line = NULL;
    size_t cap = 0;

    for(;;){
        printf("mysh> ");
        fflush(stdout);

        ssize_t read = getline(&line, &cap, stdin);
        if(read < 0){
            putchar('\n');
            break;
        }

        if(read > 0 && line[read - 1] == '\n')
            line[read - 1] = '\0';

        lexer_init(&lexer, line);

        TokenArray tokens;
        if(!lexer_tokenize_all(&lexer, &tokens)){
            fprintf(stderr, "tokenize failed\n");
            lexer_destroy(&lexer);
            continue;
        }

        for(size_t i = 0; i < tokens.count; ++i){
            print_token(&tokens.tokens[i]);
        }

        token_array_free(&tokens);
        lexer_destroy(&lexer);
    }

    free(line);
    return 0;
}