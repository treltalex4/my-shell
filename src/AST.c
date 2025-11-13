#include "AST.h"
#include <stdio.h>

ASTNode *ast_create_command(char **args, size_t argc){
    ASTNode *node = malloc(sizeof(ASTNode));
    if(!node) return NULL;

    node->type = AST_COMMAND;
    node->data.command.args = args;
    node->data.command.argc = argc;
    return node;
}

ASTNode *ast_create_binary(ASTNodeType type, ASTNode *left, ASTNode *right){
    ASTNode *node = malloc(sizeof(ASTNode));
    if(!node) return NULL;

    node->type = type;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

ASTNode *ast_create_subshell(ASTNode *inner){
    ASTNode *node = malloc(sizeof(ASTNode));
    if(!node) return NULL;

    node->type = AST_SUBSHELL;
    node->data.subshell = inner;
    return node;
}

ASTNode *ast_create_redirect(ASTNode *command, RedirectType redir_type, char *filename){
    ASTNode *node = malloc(sizeof(ASTNode));
    if(!node) return NULL;

    node->type = AST_REDIRECT;
    node->data.redirect.type = redir_type;
    node->data.redirect.command = command;
    node->data.redirect.filename = filename;
    return node;
}

void ast_free(ASTNode *node){
    if(!node) return;
    
    switch(node->type){
        case AST_COMMAND:
            if(node->data.command.args){
                for(size_t i = 0; i < node->data.command.argc; i++){
                    free(node->data.command.args[i]);
                }
                free(node->data.command.args);
            }
            break;
            
        case AST_PIPELINE:
        case AST_PIPELINE_ERR:
        case AST_SEQUENCE:
        case AST_AND:
        case AST_OR:
        case AST_BACKGROUND:
            ast_free(node->data.binary.left);
            ast_free(node->data.binary.right);
            break;
            
        case AST_SUBSHELL:
            ast_free(node->data.subshell);
            break;
            
        case AST_REDIRECT:
            ast_free(node->data.redirect.command);
            free(node->data.redirect.filename);
            break;
    }
    
    free(node);
}

void ast_print(ASTNode *node, int indent){
    if(!node) return;
    
    for(int i = 0; i < indent; i++){
        printf(" ");
    }
    
    switch(node->type){
        case AST_COMMAND:
            printf("COMMAND:");
            for(size_t i = 0; i < node->data.command.argc; i++){
                printf(" %s", node->data.command.args[i]);
            }
            printf("\n");
            break;
            
        case AST_PIPELINE:
            printf("PIPELINE\n");
            ast_print(node->data.binary.left, indent + 2);
            ast_print(node->data.binary.right, indent + 2);
            break;
            
        case AST_PIPELINE_ERR:
            printf("PIPELINE_ERR\n");
            ast_print(node->data.binary.left, indent + 2);
            ast_print(node->data.binary.right, indent + 2);
            break;
            
        case AST_SEQUENCE:
            printf("SEQUENCE\n");
            ast_print(node->data.binary.left, indent + 2);
            ast_print(node->data.binary.right, indent + 2);
            break;
            
        case AST_AND:
            printf("AND\n");
            ast_print(node->data.binary.left, indent + 2);
            ast_print(node->data.binary.right, indent + 2);
            break;
            
        case AST_OR:
            printf("OR\n");
            ast_print(node->data.binary.left, indent + 2);
            ast_print(node->data.binary.right, indent + 2);
            break;
            
        case AST_BACKGROUND:
            printf("BACKGROUND\n");
            ast_print(node->data.binary.left, indent + 2);
            ast_print(node->data.binary.right, indent + 2);
            break;
            
        case AST_SUBSHELL:
            printf("SUBSHELL\n");
            ast_print(node->data.subshell, indent + 2);
            break;
            
        case AST_REDIRECT:
            printf("REDIRECT: type=%d file=%s\n",
                   node->data.redirect.type,
                   node->data.redirect.filename);
            ast_print(node->data.redirect.command, indent + 2);
            break;
    }
}