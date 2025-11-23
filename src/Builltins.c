//Builtins.c
#include "Builtins.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define PATH_MAX_SIZE 1024

static int builtin_cd(char **args);
static int builtin_pwd(char **args);
static int builtin_echo(char **args);
static int builtin_exit(char **args);
static int builtin_help(char **args);

int is_builtin(const char *command) {
    static const char *builtins[] = {
        "cd",
        "pwd",
        "echo",
        "exit",
        "help",
        NULL
    };
    
    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(command, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int execute_builtin(char **args){
    if(strcmp(args[0], "cd") == 0){
        return builtin_cd(args);
    }
    else if(strcmp(args[0], "pwd") == 0){
        return builtin_pwd(args);
    }
    else if(strcmp(args[0], "echo") == 0){
        return builtin_echo(args);
    }
    else if(strcmp(args[0], "exit") == 0){
        return builtin_exit(args);
    }
    else if(strcmp(args[0], "help") == 0){
        return builtin_help(args);
    }

    fprintf(stderr, "%s: builtin not found\n", args[0]);
    return 1;
}

static int builtin_cd(char **args){
    const char *path = args[1];

    if(path == NULL || strcmp(path, "~") == 0){
        path = getenv("HOME");
        if(!path){
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }

    else if(strcmp(path, "-") == 0){
        path = getenv("OLDPWD");
        if(!path){
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", path);
    }

    else if(path[0] == '~' && path[1] == '/'){
        char *home = getenv("HOME");
        if(!home){
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }

        static char full_path[PATH_MAX_SIZE];
        snprintf(full_path, sizeof(full_path), "%s%s", home, path + 1);
        path = full_path;
    }

    char cwd[PATH_MAX_SIZE];
    if(getcwd(cwd, sizeof(cwd)) != NULL){
        setenv("OLDPWD", cwd, 1);
    }

    if(chdir(path) == -1){
        perror("builtin_cd: failed to change directory");
        return 1;
    }

    if(getcwd(cwd, sizeof(cwd)) != NULL){
        setenv("PWD", cwd, 1);
    }

    return 0;
}

static int builtin_pwd(char **args){
    (void)args;  // Unused parameter
    char cwd[PATH_MAX_SIZE];
    if(!getcwd(cwd, sizeof(cwd))){
        perror("builtin_pwd: failed to get current directory");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}

static int builtin_echo(char **args){
    for(size_t i = 1; args[i] != NULL; ++i){
        printf("%s", args[i]);

        if(args[i + 1] != NULL){
            putchar(' ');
        }
    }

    putchar('\n');
    return 0;
}

static int builtin_exit(char **args){
    int code = 0;

    if(args[1] != NULL) code = atoi(args[1]);

    exit(code);
}

static int builtin_help(char **args){
    (void)args;  // Unused parameter
    printf("Built-in commands:\n");
    printf("  cd [dir]       Change directory\n");
    printf("  pwd            Print current working directory\n");
    printf("  echo [args]    Print arguments\n");
    printf("  exit [code]    Exit shell\n");
    printf("  help           Show this help\n");
    return 0;
}