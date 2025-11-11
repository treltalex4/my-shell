#pragma once

#include <stdlib.h>
#include "Token.h"

typedef enum ASTNodeType {
    AST_COMMAND,
    AST_PIPELINE,
    AST_PIPELINE_ERR,
    AST_SEQUENCE,
    AST_AND,
    AST_OR,
    AST_BACKGROUND,
    AST_SUBSHELL,
    AST_REDIRECT
} ASTNodeType;

typedef enum RedirectType {
    REDIR_IN,
    REDIR_OUT,
    REDIR_OUT_APPEND,
    REDIR_ERR,
    REDIR_ERR_APPEND
} RedirectType;

typedef struct ASTNode ASTNode;

struct ASTNode {
    ASTNodeType type;
    
    union {
        struct {
            char **args;
            size_t argc;
        } command;
        
        struct {
            ASTNode *left;
            ASTNode *right;
        } binary;
        
        ASTNode *subshell;
        
        struct {
            ASTNode *command;
            RedirectType type;
            char *filename;
        } redirect;
    } data;
};

ASTNode *ast_create_command(char **args, size_t argc);

ASTNode *ast_create_binary(ASTNodeType type, ASTNode *left, ASTNode *right);

ASTNode *ast_create_subshell(ASTNode *inner);

ASTNode *ast_create_redirect(ASTNode *command, RedirectType redir_type, char *filename);

void ast_free(ASTNode *node);

void ast_print(ASTNode *node, int indent);
