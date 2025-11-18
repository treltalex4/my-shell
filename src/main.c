#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Lexer.h"
#include "Parser.h"
#include "getline.h"

/*static const char *token_type_to_str(TokenType type){
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
}*/

static int has_unclosed_quotes(const char *str){
    int single = 0;
    int double_q = 0;
    
    for(size_t i = 0; str[i] != '\0'; ++i){
        if(str[i] == '\'' && !double_q){
            single = !single;
        }
        if(str[i] == '"' && !single){
            double_q = !double_q;
        }
    }
    
    return single || double_q;
}

static char* str_concat(char *s1, const char *s2){
    if(!s2) return s1;
    if(!s1) return strdup(s2);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    char *result = realloc(s1, len1 + len2 + 1);
    if(!result){
        perror("str_concat: realloc failed");
        free(s1);
        return NULL;
    }
    
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

static char* read_command(void){
    printf("mysh> ");
    fflush(stdout);
    
    char *command = my_getline();
    if(!command){
        return NULL;
    }
    
    while(has_unclosed_quotes(command)){
        printf("> ");
        fflush(stdout);
        
        char *next_line = my_getline();
        if(!next_line){
            free(command);
            return NULL;
        }
        
        command = str_concat(command, next_line);
        free(next_line);
        
        if(!command){
            fprintf(stderr, "Error: failed to concatenate command lines\n");
            return NULL;
        }
    }
    
    return command;
}

int main(){
    Lexer lexer;
    Parser parser;

    for(;;){
        char *line = read_command();
        if(!line){
            putchar('\n');
            break;
        }

        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\n'){
            line[len - 1] = '\0';
        }

        lexer_init(&lexer, line);

        TokenArray tokens;
        if(!lexer_tokenize_all(&lexer, &tokens)){
            fprintf(stderr, "tokenize failed\n");
            lexer_destroy(&lexer);
            free(line);
            continue;
        }

        parser_init(&parser, &tokens);
        ASTNode *tree = parser_parse(&parser);
        
        if(tree){
            ast_print(tree, 0);
            ast_free(tree);
        }

        token_array_free(&tokens);
        lexer_destroy(&lexer);
        free(line);
    }

    return 0;
}