//AST.h
#pragma once

#include <stdlib.h>
#include "Token.h"

typedef enum ASTNodeType {
    AST_COMMAND,        // Простая команда с аргументами
    AST_PIPELINE,       // Конвейер команд |
    AST_PIPELINE_ERR,   // Конвейер с перенаправлением stderr |&
    AST_SEQUENCE,       // Последовательность команд ;
    AST_AND,            // Логическое И &&
    AST_OR,             // Логическое ИЛИ ||
    AST_BACKGROUND,     // Фоновое выполнение &
    AST_SUBSHELL,       // Подоболочка (...)
    AST_REDIRECT        // Перенаправление ввода/вывода
} ASTNodeType;

typedef enum RedirectType {
    REDIR_IN,           // < (ввод из файла)
    REDIR_OUT,          // > (вывод в файл, перезапись)
    REDIR_OUT_APPEND,   // >> (вывод в файл, добавление)
    REDIR_ERR,          // 2> (stderr в файл, перезапись)
    REDIR_ERR_APPEND    // 2>> (stderr в файл, добавление)
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