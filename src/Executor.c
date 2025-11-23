//Executor.c
#include "Executor.h"
#include "Builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

static int execute_command(ASTNode *root);
static int execute_pipeline(ASTNode *root);
static int execute_redirect(ASTNode *root);
static int execute_and_or(ASTNode *root);
static int execute_subshell(ASTNode *root);

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
        return execute_command(root);

    default:
        fprintf(stderr, "executor_execute: unknown node type\n");
        return 1;
    }
}

static int execute_command(ASTNode *root){
    char **args = root->data.command.args;

    if(!args || !args[0]){
        fprintf(stderr, "execute_command: no command\n");
        return 1;
    }

    if(is_builtin(args[0])){
        return execute_builtin(args);
    }

    pid_t pid = fork();

    if(pid < 0){
        perror("fork");
        return 1;
    }

    if(pid == 0){
        execvp(args[0], args);

        perror(args[0]);
        exit(127);
    }

    int status;
    wait(&status);

    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }

    return 1;
}

static int execute_pipeline(ASTNode *root){
    int fd[2];
    if(pipe(fd) < 0){
        perror("pipe");
        return 1;
    }

    pid_t pid_left = fork();

    if(pid_left < 0){
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return 1;
    }

    if(pid_left == 0){
        close(fd[0]);

        if(dup2(fd[1], STDOUT_FILENO) < 0){
            perror("dup2");
            exit(1);
        }

        if(root->type == AST_PIPELINE_ERR){
            if(dup2(fd[1], STDERR_FILENO) < 0){
                perror("dup2");
                exit(1);
            }
        }

        close(fd[1]);

        int code = executor_execute(root->data.binary.left);
        exit(code);
    }

    pid_t pid_right = fork();
    if(pid_right < 0){
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return 1;
    }

    if(pid_right == 0){
        close(fd[1]);

        if(dup2(fd[0], STDIN_FILENO) < 0){
            perror("dup2");
            exit(1);
        }

        close(fd[0]);

        int code = executor_execute(root->data.binary.right);
        exit(code);
    }

    close(fd[0]);
    close(fd[1]);

    int status_left, status_right;
    waitpid(pid_left, &status_left, 0);
    waitpid(pid_right, &status_right, 0);

    if (WIFEXITED(status_right)) {
        return WEXITSTATUS(status_right);
    }

    return 1;
}

static int execute_redirect(ASTNode *root){
    int file_fd, saved_fd = -1;

    switch(root->data.redirect.type) {
    case REDIR_IN:
        file_fd = open(root->data.redirect.filename, O_RDONLY);
        if(file_fd < 0){
            perror(root->data.redirect.filename);
            return 1;
        }

        saved_fd = dup(STDIN_FILENO);
        if(saved_fd < 0){
            perror("dup");
            close(file_fd);
            return 1;
        }
        if(dup2(file_fd, STDIN_FILENO) < 0){
            perror("dup2");
            close(file_fd);
            close(saved_fd);
            return 1;
        }
        close(file_fd);
        break;
    
    case REDIR_OUT:
        file_fd = open(root->data.redirect.filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(file_fd < 0){
            perror(root->data.redirect.filename);
            return 1;
        }

        saved_fd = dup(STDOUT_FILENO);
        if(saved_fd < 0){
            perror("dup");
            close(file_fd);
            return 1;
        }
        if(dup2(file_fd, STDOUT_FILENO) < 0){
            perror("dup2");
            close(file_fd);
            close(saved_fd);
            return 1;
        }
        close(file_fd);
        break;

    case REDIR_OUT_APPEND:
        file_fd = open(root->data.redirect.filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if(file_fd < 0){
            perror(root->data.redirect.filename);
            return 1;
        }

        saved_fd = dup(STDOUT_FILENO);
        if(saved_fd < 0){
            perror("dup");
            close(file_fd);
            return 1;
        }
        if(dup2(file_fd, STDOUT_FILENO) < 0){
            perror("dup2");
            close(file_fd);
            close(saved_fd);
            return 1;
        }
        close(file_fd);
        break;

    case REDIR_ERR:
        file_fd = open(root->data.redirect.filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(file_fd < 0){
            perror(root->data.redirect.filename);
            return 1;
        }

        saved_fd = dup(STDERR_FILENO);
        if(saved_fd < 0){
            perror("dup");
            close(file_fd);
            return 1;
        }
        if(dup2(file_fd, STDERR_FILENO) < 0){
            perror("dup2");
            close(file_fd);
            close(saved_fd);
            return 1;
        }
        close(file_fd);
        break;

    case REDIR_ERR_APPEND:
        file_fd = open(root->data.redirect.filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if(file_fd < 0){
            perror(root->data.redirect.filename);
            return 1;
        }

        saved_fd = dup(STDERR_FILENO);
        if(saved_fd < 0){
            perror("dup");
            close(file_fd);
            return 1;
        }
        if(dup2(file_fd, STDERR_FILENO) < 0){
            perror("dup2");
            close(file_fd);
            close(saved_fd);
            return 1;
        }
        close(file_fd);
        break;
        
    
    default:
        fprintf(stderr, "execute_redirect: unknown redirect type\n");
        return 1;
    }

    int code = executor_execute(root->data.redirect.command);

    if(saved_fd >= 0){
        int restore_result = -1;
        switch(root->data.redirect.type) {
        case REDIR_IN:
            restore_result = dup2(saved_fd, STDIN_FILENO);
            break;

        case REDIR_OUT:
        case REDIR_OUT_APPEND:
            restore_result = dup2(saved_fd, STDOUT_FILENO);
            break;

        case REDIR_ERR:
        case REDIR_ERR_APPEND:
            restore_result = dup2(saved_fd, STDERR_FILENO);
            break;
        
        default:
            break;
        }
        
        if(restore_result < 0){
            perror("dup2 restore");
        }
        close(saved_fd);
    }

    return code;
}

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

static int execute_subshell(ASTNode *root){
    pid_t pid = fork();
    
    if(pid < 0){
        perror("fork");
        return 1;
    }
    
    if(pid == 0){
        int code = executor_execute(root->data.subshell);
        exit(code);
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    if(WIFEXITED(status)){
        return WEXITSTATUS(status);
    }
    
    return 1;
}
