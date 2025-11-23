#include "Parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const Token *current_token(Parser *parser);
static const Token *previous_token(Parser *parser);
static void advance(Parser *parser);
static int match(Parser *parser, TokenType type);

static ASTNode *parse_command_line(Parser *parser);
static ASTNode *parse_and_or_list(Parser *parser);
static ASTNode *parse_pipeline(Parser *parser);
static ASTNode *parse_simple_command(Parser *parser);
static ASTNode *parse_redirects(Parser *parser, ASTNode *command);
static ASTNode *parse_primary(Parser *parser);

void parser_init(Parser *parser, TokenArray *tokens){
    assert(parser && "parser_init: null parser ptr");
    assert(tokens && "parser_init: null tokens ptr");

    parser->tokens = tokens;
    parser->pos = 0;
}

ASTNode *parser_parse(Parser *parser){
    assert(parser && "parser_parse: null parser ptr");
    assert(parser->tokens && "parser_parse: null tokens ptr");

    while (match(parser, TOKEN_NEWLINE)) {}
    
    if (match(parser, TOKEN_EOF)) {
        return NULL;
    }
    
    ASTNode *tree = parse_command_line(parser);
    
    if (!tree) {
        return NULL;
    }
    
    while (match(parser, TOKEN_NEWLINE)) {}
    
    const Token *tok = current_token(parser);
    if (tok && tok->type != TOKEN_EOF) {
        fprintf(stderr, "Parser error: unexpected token '%s' at position %zu\n",
                tok->text ? tok->text : "<operator>", tok->pos);
        ast_free(tree);
        return NULL;
    }
    
    return tree;
}

static int match(Parser *parser, TokenType type) {
    const Token *tok = current_token(parser);
    if(tok && tok->type == type){
        advance(parser);
        return 1;
    }
    return 0;
}

static void advance(Parser *parser){
    if(parser->pos < parser->tokens->count){
        parser->pos++;
    }
}

static const Token *current_token(Parser *parser){
    if(parser->pos>=parser->tokens->count) return NULL;

    return &parser->tokens->tokens[parser->pos];
}

static const Token *previous_token(Parser *parser){
    if(parser->pos == 0) return NULL;
    return &parser->tokens->tokens[parser->pos - 1];
}

static ASTNode *parse_command_line(Parser *parser){
    ASTNode *left = parse_and_or_list(parser);
    if(!left) return NULL;

    while(match(parser, TOKEN_SEMI) || match(parser, TOKEN_AMP)){
        const Token *op = previous_token(parser);
        if(!op){
            ast_free(left);
            fprintf(stderr, "Parser error: failed to get operator token\n");
            return NULL;
        }

        while(match(parser, TOKEN_NEWLINE)){}

        if(match(parser, TOKEN_EOF)){
            if(op->type == TOKEN_AMP){
                ASTNode *result = ast_create_binary(AST_BACKGROUND, left, NULL);
                if(!result){
                    ast_free(left);
                    return NULL;
                }
                return result;
            }
            return left;
        }

        ASTNode *right = parse_and_or_list(parser);
        if(!right){
            ast_free(left);
            return NULL;
        }

        if(op->type == TOKEN_SEMI){
            left = ast_create_binary(AST_SEQUENCE, left, right);
        } else {
            ASTNode *bg = ast_create_binary(AST_BACKGROUND, left, NULL);
            if(!bg){
                ast_free(left);
                ast_free(right);
                return NULL;
            }
            left = ast_create_binary(AST_SEQUENCE, bg, right);
        }
        
        if(!left){
            ast_free(right);
            return NULL;
        }
    }

    return left;
}

static ASTNode *parse_and_or_list(Parser *parser){
    ASTNode *left = parse_pipeline(parser);
    if(!left) return NULL;

    while(match(parser, TOKEN_AND) || match(parser, TOKEN_OR)){
        const Token *op = previous_token(parser);
        if(!op){
            ast_free(left);
            fprintf(stderr, "Parser error: failed to get operator token\n");
            return NULL;
        }
        TokenType op_type = op->type;
        
        ASTNode *right = parse_pipeline(parser);
        if(!right){
            ast_free(left);
            return NULL;
        }
        
        if(op_type == TOKEN_AND){
            left = ast_create_binary(AST_AND, left, right);
        } else {
            left = ast_create_binary(AST_OR, left, right);
        }
        
        if(!left){
            ast_free(right);
            return NULL;
        }
    }
    
    return left;
}

static ASTNode *parse_pipeline(Parser *parser){
    ASTNode *left = parse_primary(parser);
    if(!left) return NULL;

    while(match(parser, TOKEN_PIPE) || match(parser, TOKEN_PIPE_ERR)){
        const Token *op = previous_token(parser);
        if(!op){
            ast_free(left);
            fprintf(stderr, "Parser error: failed to get operator token\n");
            return NULL;
        }
        TokenType op_type = op->type;

        ASTNode *right = parse_primary(parser);
        if(!right){
            ast_free(left);
            return NULL;
        }

        if(op_type == TOKEN_PIPE){
            left = ast_create_binary(AST_PIPELINE, left, right);
        } else {
            left = ast_create_binary(AST_PIPELINE_ERR, left, right);
        }
        
        if(!left){
            ast_free(right);
            return NULL;
        }
    }

    return left;
}

static ASTNode *parse_simple_command(Parser *parser){
    size_t capacity = 8;
    size_t count = 0;
    char **args = malloc(capacity * sizeof(char *));
    if(!args){
        perror("parse_simple_command: malloc failed");
        return NULL;
    }

    while(1){
        const Token *tok = current_token(parser);
        
        if(!tok){
            break;
        }
        
        if(tok->type == TOKEN_ERROR){
            fprintf(stderr, "Lexer error at position %zu: %s\n",
                    tok->pos, tok->text ? tok->text : "unknown error");
            for(size_t i = 0; i < count; i++){
                free(args[i]);
            }
            free(args);
            return NULL;
        }
        
        if(tok->type != TOKEN_WORD){
            break;
        }

        if(count >= capacity){
            capacity *= 2;
            char **new_args = realloc(args, capacity * sizeof(char *));
            if(!new_args){
                perror("parse_simple_command: realloc failed");
                for(size_t i = 0; i < count; i++){
                    free(args[i]);
                }
                free(args);
                return NULL;
            }
            args = new_args;
        }

        args[count] = strdup(tok->text);
        if(!args[count]){
            perror("parse_simple_command: strdup failed");
            for(size_t i = 0; i < count; i++){
                free(args[i]);
            }
            free(args);
            return NULL;
        }
        count++;
        advance(parser);
    }

    if(count == 0){
        free(args);
        return NULL;
    }

    if(count >= capacity){
        capacity++;
        char **new_args = realloc(args, capacity * sizeof(char *));
        if(!new_args){
            perror("parse_simple_command: realloc failed");
            for(size_t i = 0; i < count; i++){
                free(args[i]);
            }
            free(args);
            return NULL;
        }
        args = new_args;
    }
    args[count] = NULL;

    ASTNode *node = ast_create_command(args, count);
    if(!node){
        fprintf(stderr, "parse_simple_command: ast_create_command failed\n");
        for(size_t i = 0; i < count; i++){
            free(args[i]);
        }
        free(args);
        return NULL;
    }

    return node;
}

static ASTNode *parse_redirects(Parser *parser, ASTNode *command){
    if(!command) return NULL;
    
    while(1){
        const Token *tok = current_token(parser);
        if(!tok) break;
        
        RedirectType redir_type;
        
        switch(tok->type){
            case TOKEN_REDIR_OUT:
                redir_type = REDIR_OUT;
                break;
            case TOKEN_REDIR_OUT_APPEND:
                redir_type = REDIR_OUT_APPEND;
                break;
            case TOKEN_REDIR_IN:
                redir_type = REDIR_IN;
                break;
            case TOKEN_REDIR_ERR:
                redir_type = REDIR_ERR;
                break;
            case TOKEN_REDIR_ERR_APPEND:
                redir_type = REDIR_ERR_APPEND;
                break;
            default:
                return command;
        }
        
        advance(parser);
        
        const Token *file_tok = current_token(parser);
        if(!file_tok || file_tok->type != TOKEN_WORD){
            fprintf(stderr, "Parser error: expected filename after redirect at position %zu\n",
                    tok->pos);
            ast_free(command);
            return NULL;
        }
        
        char *filename = strdup(file_tok->text);
        if(!filename){
            perror("parse_redirects: strdup failed");
            ast_free(command);
            return NULL;
        }
        
        advance(parser);
        
        command = ast_create_redirect(command, redir_type, filename);
        if(!command){
            return NULL;
        }
    }
    
    return command;
}

static ASTNode *parse_primary(Parser* parser){
    if(match(parser, TOKEN_LPAREN)){
        ASTNode *inner = parse_command_line(parser);
        if(!inner){
            fprintf(stderr, "Parser error: expected command after '('\n");
            return NULL;
        }
        
        if(!match(parser, TOKEN_RPAREN)){
            fprintf(stderr, "Parser error: expected ')' after subshell command\n");
            ast_free(inner);
            return NULL;
        }
        
        ASTNode *subshell = ast_create_subshell(inner);
        if(!subshell){
            fprintf(stderr, "parse_primary: ast_create_subshell failed\n");
            ast_free(inner);
            return NULL;
        }
        
        return subshell;
    }
    
    ASTNode *command = parse_simple_command(parser);
    return parse_redirects(parser, command);
}