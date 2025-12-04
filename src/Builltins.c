//Builtins.c
#include "Builtins.h"
#include "JobControl.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define PATH_MAX_SIZE 1024
#define ENV_MAX_NAME 128
#define ENV_MAX_VAL 256

static int builtin_cd(char **args);
static int builtin_pwd(char **args);
static int builtin_echo(char **args);
static int builtin_exit(char **args);
static int builtin_help(char **args);
static int builtin_jobs(char **args);
static int builtin_fg(char **args);
static int builtin_bg(char **args);
static int builtin_kill(char **args);
static int builtin_set(char **args);
static int builtin_unset(char **args);

int is_builtin(const char *command) {
    static const char *builtins[] = {
        "cd",
        "pwd",
        "echo",
        "exit",
        "help",
        "jobs",
        "fg",
        "bg",
        "kill",
        "set",
        "unset",
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
    else if(strcmp(args[0], "jobs") == 0){
        return builtin_jobs(args);
    }
    else if(strcmp(args[0], "fg") == 0){
        return builtin_fg(args);
    }
    else if(strcmp(args[0], "bg") == 0){
        return builtin_bg(args);
    }
    else if(strcmp(args[0], "kill") == 0){
        return builtin_kill(args);
    }
    else if(strcmp(args[0], "set") == 0){
        return builtin_set(args);
    }
    else if(strcmp(args[0], "unset") == 0){
        return builtin_unset(args);
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
    (void)args;
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

    job_control_cleanup();
    exit(code);
}

static int builtin_help(char **args){
    (void)args;
    printf("Built-in commands:\n");
    printf("  cd [dir]       Change directory\n");
    printf("  pwd            Print current working directory\n");
    printf("  echo [args]    Print arguments\n");
    printf("  exit [code]    Exit shell\n");
    printf("  help           Show this help\n");
    printf("  jobs           List all jobs\n");
    printf("  fg [job_id]    Bring job to foreground\n");
    printf("  bg [job_id]    Resume job in background\n");
    printf("  kill [job_id]  Send SIGTERM to job\n");
    return 0;
}

static int builtin_jobs(char **args){
    (void)args;
    JobList *list = job_list_get();
    job_list_print(list);
    return 0;
}

static int builtin_fg(char **args){
    JobList *list = job_list_get();
    Job *job = NULL;
    
    if(args[1] == NULL){

        job = list->tail;
        if(!job){
            fprintf(stderr, "fg: no current job\n");
            return 1;
        }
    } else {

        int job_id = atoi(args[1]);
        job = job_list_find_by_id(list, job_id);
        if(!job){
            fprintf(stderr, "fg: job %d not found\n", job_id);
            return 1;
        }
    }
    
    printf("%s\n", job->command_line);

    int cont = (job->state == JOB_STOPPED);
    
    if(!cont){
        for(Process *p = job->processes; p; p = p->next){
            if(p->state == PROC_STOPPED){
                cont = 1;
                break;
            }
        }
    }
    
    if(job_foreground(job, cont) < 0){
        return 1;
    }
    
    return 0;
}

static int builtin_bg(char **args){
    JobList *list = job_list_get();
    Job *job = NULL;
    
    if(args[1] == NULL){

        job = list->tail;
        if(!job){
            fprintf(stderr, "bg: no current job\n");
            return 1;
        }
    } else {

        int job_id = atoi(args[1]);
        job = job_list_find_by_id(list, job_id);
        if(!job){
            fprintf(stderr, "bg: job %d not found\n", job_id);
            return 1;
        }
    }
    
    if(job->state != JOB_STOPPED){
        fprintf(stderr, "bg: job %d is not stopped\n", job->job_id);
        return 1;
    }
    
    printf("[%d]+ %s &\n", job->job_id, job->command_line);
    
    if(job_background(job, 1) < 0){
        return 1;
    }
    
    return 0;
}

static int builtin_kill(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "kill: usage: kill [-signal] [%%job_id]\n");
        return 1;
    }
    
    int sig = SIGTERM;    int arg_idx = 1;
    
    if(args[1][0] == '-' && args[1][1] != '\0'){
        const char *sig_str = args[1] + 1;
        
        if(strcmp(sig_str, "STOP") == 0) sig = SIGSTOP;
        else if(strcmp(sig_str, "CONT") == 0) sig = SIGCONT;
        else if(strcmp(sig_str, "TERM") == 0) sig = SIGTERM;
        else if(strcmp(sig_str, "KILL") == 0) sig = SIGKILL;
        else if(strcmp(sig_str, "INT") == 0) sig = SIGINT;
        else if(strcmp(sig_str, "HUP") == 0) sig = SIGHUP;
        else if(strcmp(sig_str, "QUIT") == 0) sig = SIGQUIT;
        else if(strcmp(sig_str, "TSTP") == 0) sig = SIGTSTP;
        else {
            sig = atoi(sig_str);
            if(sig <= 0){
                fprintf(stderr, "kill: invalid signal: %s\n", sig_str);
                return 1;
            }
        }
        
        arg_idx = 2;
        if(args[arg_idx] == NULL){
            fprintf(stderr, "kill: usage: kill [-signal] [%%job_id]\n");
            return 1;
        }
    }
    
    JobList *list = job_list_get();
    int job_id;
    
    if(args[arg_idx][0] == '%'){
        job_id = atoi(args[arg_idx] + 1);
    } else {
        job_id = atoi(args[arg_idx]);
    }
    
    Job *job = job_list_find_by_id(list, job_id);
    
    if(!job){
        fprintf(stderr, "kill: job %d not found\n", job_id);
        return 1;
    }
    
    if(job_kill(job, sig) < 0){
        return 1;
    }
    
    if(sig == SIGSTOP || sig == SIGTSTP){
        job->state = JOB_STOPPED;
        for(Process *p = job->processes; p; p = p->next){
            p->state = PROC_STOPPED;
        }
        printf("[%d]+ Stopped\t%s\n", job->job_id, job->command_line);
    } else if(sig == SIGCONT){
        job->state = JOB_BACKGROUND;
        for(Process *p = job->processes; p; p = p->next){
            if(p->state == PROC_STOPPED) p->state = PROC_RUNNING;
        }
        printf("[%d]+ %s &\n", job->job_id, job->command_line);
    } else if(sig == SIGTERM || sig == SIGKILL){
        printf("[%d]+ Terminated\t%s\n", job->job_id, job->command_line);
    }
    
    return 0;
}

static int builtin_set(char **args){
    if(args[1] == NULL){
        extern char **environ;
        for(char **env = environ; *env != NULL; env++){
            printf("%s\n", *env);
        }
        return 0;
    }

    char *arg = args[1];
    char *eq = strchr(arg, '=');

    if(eq == NULL){
        fprintf(stderr, "set: incorrect format\n");
        return 1;
    }

    *eq = '\0';
    char *name = arg;
    char *value = eq + 1;

    if(setenv(name, value, 1) != 0){
        perror("setenv");
        *eq = '=';
        return 1;
    }
    *eq = '=';

    return 0;
    
}

static int builtin_unset(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "unset: not enough arguments\n");
        return 1;
    }

    if(unsetenv(args[1]) != 0){
        perror("unset");
        return 1;
    }

    return 0;
}