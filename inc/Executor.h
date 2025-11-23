//Executor.h
#pragma once

#include "AST.h"

// Основная функция выполнения AST
// Возвращает код возврата последней выполненной команды
int executor_execute(ASTNode *root);
