// Executor.c

#include "Executor.h"
#include "Builtins.h"
#include "JobControl.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

// Флаг, указывающий что процесс выполняется в фоне
// Используется чтобы избежать вызова tcsetpgrp в дочерних процессах фоновых задач
static int g_in_background = 0;

static int execute_command(ASTNode *root);
static int execute_pipeline(ASTNode *root);
static int execute_redirect(ASTNode *root);
static int execute_and_or(ASTNode *root);
static int execute_subshell(ASTNode *root);
static int execute_background(ASTNode *root);
// Вспомогательная функция для преобразования AST в строку команды
// Используется для отображения команды в job list
static char* ast_to_string(ASTNode *node);

static char* ast_to_string(ASTNode *node) {
    if (!node) return strdup("");
    
    char *result = NULL;
    size_t len = 0;
    
    switch (node->type) {
    case AST_COMMAND: {
        // Вычисляем длину итоговой строки (все аргументы + пробелы)
        for (size_t i = 0; node->data.command.args[i]; i++) {
            len += strlen(node->data.command.args[i]) + 1;        }
        result = malloc(len + 1);
        if (!result) return strdup("???");
        result[0] = '\0';
        
        // Собираем команду из аргументов через пробел
        for (size_t i = 0; node->data.command.args[i]; i++) {
            if (i > 0) strcat(result, " ");
            strcat(result, node->data.command.args[i]);
        }
        return result;
    }
    case AST_PIPELINE:
    case AST_PIPELINE_ERR: {
        // Рекурсивно преобразуем левую и правую части pipeline
        char *left = ast_to_string(node->data.binary.left);
        char *right = ast_to_string(node->data.binary.right);
        len = strlen(left) + strlen(right) + 4;        result = malloc(len + 1);
        if (!result) { free(left); free(right); return strdup("???"); }
        sprintf(result, "%s | %s", left, right);
        free(left); free(right);
        return result;
    }
    case AST_SUBSHELL: {
        char *inner = ast_to_string(node->data.subshell);
        len = strlen(inner) + 3;        result = malloc(len + 1);
        if (!result) { free(inner); return strdup("???"); }
        sprintf(result, "(%s)", inner);
        free(inner);
        return result;
    }
    case AST_REDIRECT: {
        char *cmd = ast_to_string(node->data.redirect.command);
        const char *redir = ">";
        switch (node->data.redirect.type) {
            case REDIR_IN: redir = "<"; break;
            case REDIR_OUT: redir = ">"; break;
            case REDIR_OUT_APPEND: redir = ">>"; break;
            case REDIR_ERR: redir = "&>"; break;
            case REDIR_ERR_APPEND: redir = "&>>"; break;
        }
        len = strlen(cmd) + strlen(redir) + strlen(node->data.redirect.filename) + 3;
        result = malloc(len + 1);
        if (!result) { free(cmd); return strdup("???"); }
        sprintf(result, "%s %s %s", cmd, redir, node->data.redirect.filename);
        free(cmd);
        return result;
    }
    case AST_SEQUENCE: {
        char *left = ast_to_string(node->data.binary.left);
        char *right = ast_to_string(node->data.binary.right);
        len = strlen(left) + strlen(right) + 3;        result = malloc(len + 1);
        if (!result) { free(left); free(right); return strdup("???"); }
        sprintf(result, "%s; %s", left, right);
        free(left); free(right);
        return result;
    }
    case AST_AND: {
        char *left = ast_to_string(node->data.binary.left);
        char *right = ast_to_string(node->data.binary.right);
        len = strlen(left) + strlen(right) + 5;        result = malloc(len + 1);
        if (!result) { free(left); free(right); return strdup("???"); }
        sprintf(result, "%s && %s", left, right);
        free(left); free(right);
        return result;
    }
    case AST_OR: {
        char *left = ast_to_string(node->data.binary.left);
        char *right = ast_to_string(node->data.binary.right);
        len = strlen(left) + strlen(right) + 5;        result = malloc(len + 1);
        if (!result) { free(left); free(right); return strdup("???"); }
        sprintf(result, "%s || %s", left, right);
        free(left); free(right);
        return result;
    }
    default:
        return strdup("???");
    }
}

// Главная функция выполнения AST
// Диспетчеризует выполнение в зависимости от типа узла
int executor_execute(ASTNode *root){
    if(!root){
        return 0;
    }

    switch (root->type) {
    case AST_COMMAND:
        return execute_command(root);

    case AST_PIPELINE:
    case AST_PIPELINE_ERR:
        return execute_pipeline(root);  

    case AST_REDIRECT:
        return execute_redirect(root);

    case AST_AND:
    case AST_OR:
        return execute_and_or(root);

    case AST_SUBSHELL:
        return execute_subshell(root);

    case AST_SEQUENCE:
        executor_execute(root->data.binary.left);
        return executor_execute(root->data.binary.right);

    case AST_BACKGROUND:
        return execute_background(root);

    default:
        fprintf(stderr, "executor_execute: unknown node type\n");
        return 1;
    }
}

// Выполнение команды в фоне (cmd &)
// Создаёт дочерний процесс, не ждёт завершения, добавляет в job list
static int execute_background(ASTNode *root){
    pid_t pid = fork();
    
    if(pid < 0){
        perror("fork");
        return 1;
    }
    
    if(pid == 0){
        // Дочерний процесс: создаём новую группу процессов
        setpgid(0, 0);
        // Устанавливаем флаг, чтобы вложенные команды не вызывали tcsetpgrp
        g_in_background = 1;
        
        // Восстанавливаем обработчики сигналов
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        // Игнорируем попытки чтения/записи в терминал
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_DFL);
        
        // Оптимизация: для простой команды сразу делаем exec
        if(root->data.subshell && root->data.subshell->type == AST_COMMAND){
            char **args = root->data.subshell->data.command.args;
            if(args && args[0]){
                if(is_builtin(args[0])){
                    exit(execute_builtin(args));
                }
                execvp(args[0], args);
                perror(args[0]);
                exit(127);
            }
        }
        
        int code = executor_execute(root->data.subshell);
        exit(code);
    }
    
    // Родительский процесс: гарантируем что дочерний в своей группе
    setpgid(pid, pid);
    
    // Сохраняем PID для $! (последний фоновый процесс)
    extern pid_t g_last_bg_pid;
    g_last_bg_pid = pid;
    
    // Создаём строковое представление команды для job list
    char *cmd_str = ast_to_string(root->data.subshell);
    
    // Добавляем задачу в список фоновых задач
    Job *job = job_create(pid, cmd_str, JOB_BACKGROUND);
    if(job){
        job_add_process(job, pid, cmd_str);
        job_list_add(job_list_get(), job);
        printf("[%d] %d\n", job->job_id, pid);
    } else {
        printf("[bg] %d\n", pid);
    }
    
    free(cmd_str);
    
    return 0;
}

// Выполнение простой команды
// Встроенные команды выполняются в текущем процессе, внешние через fork/exec
static int execute_command(ASTNode *root){
    char **args = root->data.command.args;

    if(!args || !args[0]){
        fprintf(stderr, "execute_command: no command\n");
        return 1;
    }

    // Встроенные команды выполняются без fork
    if(is_builtin(args[0])){
        return execute_builtin(args);
    }

    pid_t pid = fork();

    if(pid < 0){
        perror("fork");
        return 1;
    }

    if(pid == 0){
        // Дочерний процесс: создаём новую группу если не в фоне
        if(!g_in_background){
            setpgid(0, 0);  // Делаем себя лидером новой группы
        }
        // Восстанавливаем обработчики сигналов по умолчанию
        // чтобы команды могли корректно реагировать на Ctrl+C, Ctrl+Z
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        // Заменяем процесс на внешнюю команду
        execvp(args[0], args);

        // Если execvp вернулся - ошибка (команда не найдена)
        perror(args[0]);
        exit(127);  // Код 127 - команда не найдена (стандарт POSIX)
    }

    // Родительский процесс (shell):
    if(!g_in_background){
        setpgid(pid, pid);  // Гарантируем что дочерний процесс в своей группе
        // Передаём управление терминалом дочернему процессу
        // чтобы он мог получать сигналы от Ctrl+C/Ctrl+Z
        tcsetpgrp(STDIN_FILENO, pid);
    } else {
        // Для фоновой задачи помещаем в группу текущего процесса который уже в фоновой группе созданной execute_background
        setpgid(pid, getpgrp());
    }
    
    int status;
    // Ожидаем завершения или остановки процесса (WUNTRACED для Ctrl+Z)
    waitpid(pid, &status, WUNTRACED);
    
    // Возвращаем управление терминалом shell'у
    if(!g_in_background){
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }

    // Процесс завершился нормально - возвращаем код выхода
    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }
    
    // Если subshell остановлен (Ctrl+Z), создаём job
    if(WIFSTOPPED(status)){
        char *cmd_str = ast_to_string(root);
        Job *job = job_create(pid, cmd_str, JOB_STOPPED);
        if(job){
            job_add_process(job, pid, cmd_str);
            job_list_add(job_list_get(), job);
            printf("\n[%d] Stopped   %s\n", job->job_id, cmd_str);
        }
        free(cmd_str);
        return 0;
    }
    
    return 1;
}
// Также определяет где нужно перенаправлять stderr
static int collect_pipeline_commands(ASTNode *root, ASTNode **commands, int *pipe_stderr, int max_commands) {
    int count = 0;
    ASTNode *current = root;
    
    // Проходим по цепочке pipeline
    while (current && (current->type == AST_PIPELINE || current->type == AST_PIPELINE_ERR)) {
        if (count >= max_commands - 1) {
            fprintf(stderr, "pipeline: too many commands\n");
            return -1;
        }
        commands[count] = current->data.binary.left;  // Левая часть - команда
        // Запоминаем нужно ли перенаправить stderr
        pipe_stderr[count] = (current->type == AST_PIPELINE_ERR);
        count++;
        current = current->data.binary.right;  // Идём вправо по цепочке
    }
    
    // Последняя команда в цепочке (самая правая)
    if (current) {
        commands[count++] = current;
    }
    
    return count;
}

// Выполнение одной команды в pipeline (вызывается в дочернем процессе)
static void execute_pipeline_command(ASTNode *node) {
    if (node->type == AST_COMMAND) {
        char **args = node->data.command.args;
        if (args && args[0]) {
            if (is_builtin(args[0])) {
                int code = execute_builtin(args);
                exit(code);
            }
            execvp(args[0], args);
            perror(args[0]);
            exit(127);
        }
        exit(0);
    } else {
        int code = executor_execute(node);
        exit(code);
    }
}

// Выполнение pipeline (cmd1 | cmd2 | cmd3)
// Создаёт pipes между командами, fork для каждой команды, настраивает stdin/stdout/stderr
static int execute_pipeline(ASTNode *root) {
    ASTNode *commands[64];
    int pipe_stderr[64] = {0};    
    int cmd_count = collect_pipeline_commands(root, commands, pipe_stderr, 64);
    if (cmd_count < 0) {
        return 1;
    }
    
    if (cmd_count == 1) {
        return executor_execute(commands[0]);
    }
    
    // Создаём pipes для связи между командами
    int pipes[cmd_count - 1][2];
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            // При ошибке закрываем уже созданные pipes
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return 1;
        }
    }
    
    // Определяем PGID для процессов pipeline
    // Если в фоне - используем текущую группу (созданную execute_background)
    // Если на переднем плане - первый процесс станет лидером новой группы
    pid_t pipeline_pgid = g_in_background ? getpgrp() : 0;
    
    // Создаём процесс для каждой команды в pipeline
    pid_t pids[cmd_count];
    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("fork");
            // При ошибке закрываем все pipes
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return 1;
        }
        
        if (pids[i] == 0) {
            // Дочерний процесс: присоединяемся к группе pipeline
            // Для первого процесса на переднем плане: создаём новую группу (pgid=0 -> свой PID)
            // Для остальных или в фоне: присоединяемся к существующей группе
            if (g_in_background) {
                // В фоне: все процессы в группе фоновой задачи
                setpgid(0, pipeline_pgid);
            } else {
                // На переднем плане: первый создаёт группу, остальные присоединяются
                setpgid(0, pids[0] == 0 ? 0 : pids[0]);
            }
            
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            
            // Настройка stdin: читаем из предыдущего pipe (если не первая команда)
            if (i > 0) {
                // Заменяем stdin на читающий конец предыдущего pipe
                if (dup2(pipes[i-1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }
            
            // Настройка stdout: пишем в следующий pipe (если не последняя команда)
            if (i < cmd_count - 1) {
                // Заменяем stdout на пишущий конец текущего pipe
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
                // Для |& (pipe stderr) также перенаправляем stderr
                if (pipe_stderr[i]) {
                    if (dup2(pipes[i][1], STDERR_FILENO) < 0) {
                        perror("dup2");
                        exit(1);
                    }
                }
            }
            
            // Закрываем все копии pipe дескрипторов (уже сделали dup2)
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            execute_pipeline_command(commands[i]);
            exit(1);
        }
        
        // Родитель: гарантируем правильную группу для каждого дочернего процесса
        if (g_in_background) {
            setpgid(pids[i], pipeline_pgid);
        } else {
            // На переднем плане: первый процесс - лидер, остальные в его группе
            if (i == 0) {
                setpgid(pids[i], pids[i]);  // Первый становится лидером
            } else {
                setpgid(pids[i], pids[0]);  // Остальные в группу первого
            }
        }
    }
    
    // Родитель закрывает все pipes (дочерние процессы уже сделали dup2)
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Передаём терминал группе pipeline ТОЛЬКО если не в фоне
    if (!g_in_background) {
        tcsetpgrp(STDIN_FILENO, pids[0]);
    }
    
    int last_status = 0;
    int any_stopped = 0;
    
    // Ожидаем завершения всех команд в pipeline
    for (int i = 0; i < cmd_count; i++) {
        int status;
        waitpid(pids[i], &status, WUNTRACED);
        
        // Если хотя бы один процесс остановлен (Ctrl+Z)
        if (WIFSTOPPED(status)) {
            any_stopped = 1;
        }
        
        // Код возврата pipeline = код возврата последней команды
        if (i == cmd_count - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else {
                last_status = 1;
            }
        }
    }
    
    // Возвращаем управление терминалом shell'у ТОЛЬКО если не в фоне
    if (!g_in_background) {
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    
    // Если pipeline остановлен - создаём job (только на переднем плане)
    if (any_stopped && !g_in_background) {
        char *cmd_str = ast_to_string(root);
        Job *job = job_create(pids[0], cmd_str, JOB_STOPPED);
        if(job){
            // Добавляем все процессы pipeline в job
            for(int i = 0; i < cmd_count; i++){
                job_add_process(job, pids[i], cmd_str);
            }
            job_list_add(job_list_get(), job);
            printf("\n[%d] Stopped   %s\n", job->job_id, cmd_str);
        }
        free(cmd_str);
        return 0;
    }
    
    return last_status;
}

// Вспомогательная структура для хранения информации о редиректе
typedef struct {
    RedirectType type;
    int fd;  // открытый файловый дескриптор
} RedirectInfo;

// Собрать все редиректы из вложенной структуры AST в массив
// Возвращает количество редиректов, заполняет массив redirects
// Также возвращает финальную команду (не-редирект узел)
static int collect_redirects(ASTNode *node, RedirectInfo **redirects, ASTNode **command) {
    int count = 0;
    int capacity = 4;
    RedirectInfo *redir_array = malloc(capacity * sizeof(RedirectInfo));
    if (!redir_array) {
        perror("collect_redirects: malloc");
        return -1;
    }
    
    // Идем по цепочке редиректов, собирая их в массив
    ASTNode *current = node;
    while (current && current->type == AST_REDIRECT) {
        // Расширяем массив при необходимости
        if (count >= capacity) {
            capacity *= 2;
            RedirectInfo *new_array = realloc(redir_array, capacity * sizeof(RedirectInfo));
            if (!new_array) {
                perror("collect_redirects: realloc");
                // Закрываем уже открытые файлы
                for (int i = 0; i < count; i++) {
                    if (redir_array[i].fd >= 0) close(redir_array[i].fd);
                }
                free(redir_array);
                return -1;
            }
            redir_array = new_array;
        }
        
        // Открываем файл согласно типу редиректа
        int fd = -1;
        switch (current->data.redirect.type) {
            case REDIR_IN:
                fd = open(current->data.redirect.filename, O_RDONLY);
                break;
            case REDIR_OUT:
                fd = open(current->data.redirect.filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                break;
            case REDIR_OUT_APPEND:
                fd = open(current->data.redirect.filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                break;
            case REDIR_ERR:
                fd = open(current->data.redirect.filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                break;
            case REDIR_ERR_APPEND:
                fd = open(current->data.redirect.filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                break;
        }
        
        if (fd < 0) {
            perror(current->data.redirect.filename);
            // Закрываем уже открытые файлы
            for (int i = 0; i < count; i++) {
                if (redir_array[i].fd >= 0) close(redir_array[i].fd);
            }
            free(redir_array);
            return -1;
        }
        
        redir_array[count].type = current->data.redirect.type;
        redir_array[count].fd = fd;
        count++;
        
        current = current->data.redirect.command;
    }
    
    *redirects = redir_array;
    *command = current;
    return count;
}

// Выполнение перенаправления ввода/вывода
// Собирает все редиректы, открывает файлы, применяет только последний для каждого дескриптора
static int execute_redirect(ASTNode *root){
    RedirectInfo *redirects = NULL;
    ASTNode *command = NULL;
    
    // Собираем все редиректы в массив
    int redir_count = collect_redirects(root, &redirects, &command);
    if (redir_count < 0) {
        return 1;
    }
    
    // Сохраняем оригинальные дескрипторы
    int saved_stdin = -1, saved_stdout = -1, saved_stderr = -1;
    int stdin_redirected = 0, stdout_redirected = 0, stderr_redirected = 0;
    
    // Проходим по редиректам от начала массива
    // redirects[0] = последний (внешний) редирект в команде
    // redirects[count-1] = первый (внутренний) редирект
    // Для каждого дескриптора применяем только первый найденный (= последний в команде)
    for (int i = 0; i < redir_count; i++) {
        switch (redirects[i].type) {
            case REDIR_IN:
                if (!stdin_redirected) {
                    saved_stdin = dup(STDIN_FILENO);
                    if (saved_stdin < 0 || dup2(redirects[i].fd, STDIN_FILENO) < 0) {
                        perror("redirect stdin");
                        goto cleanup_error;
                    }
                    stdin_redirected = 1;
                }
                break;
                
            case REDIR_OUT:
            case REDIR_OUT_APPEND:
                if (!stdout_redirected) {
                    saved_stdout = dup(STDOUT_FILENO);
                    if (saved_stdout < 0 || dup2(redirects[i].fd, STDOUT_FILENO) < 0) {
                        perror("redirect stdout");
                        goto cleanup_error;
                    }
                    stdout_redirected = 1;
                }
                break;
                
            case REDIR_ERR:
            case REDIR_ERR_APPEND:
                if (!stdout_redirected) {
                    saved_stdout = dup(STDOUT_FILENO);
                    if (saved_stdout < 0 || dup2(redirects[i].fd, STDOUT_FILENO) < 0) {
                        perror("redirect stdout");
                        goto cleanup_error;
                    }
                    stdout_redirected = 1;
                }
                if (!stderr_redirected) {
                    saved_stderr = dup(STDERR_FILENO);
                    if (saved_stderr < 0 || dup2(redirects[i].fd, STDERR_FILENO) < 0) {
                        perror("redirect stderr");
                        goto cleanup_error;
                    }
                    stderr_redirected = 1;
                }
                break;
        }
    }
    
    // Закрываем все открытые файловые дескрипторы
    for (int i = 0; i < redir_count; i++) {
        close(redirects[i].fd);
    }
    free(redirects);
    
    // Выполняем команду
    int code = executor_execute(command);
    
    // Восстанавливаем оригинальные дескрипторы
    if (saved_stdin >= 0) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    
    return code;

cleanup_error:
    // Закрываем все открытые дескрипторы при ошибке
    for (int i = 0; i < redir_count; i++) {
        close(redirects[i].fd);
    }
    free(redirects);
    if (saved_stdin >= 0) close(saved_stdin);
    if (saved_stdout >= 0) close(saved_stdout);
    if (saved_stderr >= 0) close(saved_stderr);
    return 1;
}

// Выполнение логических операторов && и ||
// && - выполняет правую часть только если левая успешна (код 0)
// || - выполняет правую часть только если левая неуспешна (код != 0)
static int execute_and_or(ASTNode *root){
    int left_code = executor_execute(root->data.binary.left);

    if(root->type == AST_AND){
        if(left_code == 0){
            return executor_execute(root->data.binary.right);
        }
        return left_code;
    }

    if(root->type == AST_OR){
        if(left_code != 0){
            return executor_execute(root->data.binary.right);
        }
        return left_code;
    }

    return 1;
}

// Выполнение subshell (команды в скобках)
// Создаёт отдельный процесс для изоляции окружения
static int execute_subshell(ASTNode *root){
    pid_t pid = fork();
    
    if(pid < 0){
        perror("fork");
        return 1;
    }
    
    if(pid == 0){
        // Дочерний процесс: создаём изолированное окружение
        if(!g_in_background){
            setpgid(0, 0);  // Новая группа процессов
        }
        // Восстанавливаем обработку сигналов
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        
        // Выполняем команды внутри subshell и завершаемся
        int code = executor_execute(root->data.subshell);
        exit(code);
    }
    
    if(!g_in_background){
        setpgid(pid, pid);
        tcsetpgrp(STDIN_FILENO, pid);
    } else {
        // Для фоновой задачи помещаем в группу текущего процесса
        setpgid(pid, getpgrp());
    }
    
    int status;
    waitpid(pid, &status, WUNTRACED);
    
    if(!g_in_background){
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    
    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }
    
    // Если subshell остановлен (Ctrl+Z), создаём job
    if(WIFSTOPPED(status)){
        char *cmd_str = ast_to_string(root);
        Job *job = job_create(pid, cmd_str, JOB_STOPPED);
        if(job){
            job_add_process(job, pid, cmd_str);
            job_list_add(job_list_get(), job);
            printf("\n[%d] Stopped   %s\n", job->job_id, cmd_str);
        }
        free(cmd_str);
        return 0;
    }
    
    return 1;
}
