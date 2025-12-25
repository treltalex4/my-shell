//Builtins.c
#include "Builtins.h"
#include "JobControl.h"
#include "History.h"

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
static int builtin_unset(char **args);
//static int builtin_ls(char **args);
static int builtin_history(char **args);

// Проверка, является ли команда встроенной
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
        //"ls",
        "history",
        NULL
    };
    
    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(command, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Диспетчер встроенных команд
// Вызывает соответствующую функцию в зависимости от args[0]
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
    // else if(strcmp(args[0], "ls") == 0){
    //     return builtin_ls(args);
    // }
    else if(strcmp(args[0], "history") == 0){
        return builtin_history(args);
    }

    fprintf(stderr, "%s: builtin not found\n", args[0]);
    return 1;
}

// Изменение текущего каталога
// Поддерживает: cd, cd ~, cd -, cd ~/path
static int builtin_cd(char **args){
    const char *path = args[1];

    // Без аргументов или ~ - переход в HOME
    if(path == NULL || strcmp(path, "~") == 0){
        path = getenv("HOME");
        if(!path){
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    // cd - переход в предыдущий каталог (OLDPWD)
    else if(strcmp(path, "-") == 0){
        path = getenv("OLDPWD");
        if(!path){
            fprintf(stderr, "cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", path);
    }
    // Раскрытие ~/path в $HOME/path
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

    // Сохранение текущего каталога в OLDPWD перед переходом
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

// Вывод текущего рабочего каталога
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

// Вывод аргументов в stdout
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

// Выход из shell с кодом возврата
// Устанавливает флаг выхода, очистка происходит в main
extern int g_should_exit;
extern int g_exit_code;

static int builtin_exit(char **args){
    int code = 0;

    if(args[1] != NULL) code = atoi(args[1]);

    g_exit_code = code;
    g_should_exit = 1;
    return code;
}

// Вывод справки по встроенным командам
static int builtin_help(char **args){
    (void)args;
    printf("Built-in commands:\n");
    printf("  cd [dir]          Change directory (supports ~, -, ~/path)\n");
    printf("  pwd               Print current working directory\n");
    printf("  echo [args]       Print arguments\n");
    printf("  exit [code]       Exit shell\n");
    printf("  help              Show this help\n");
    printf("  jobs              List all jobs\n");
    printf("  fg [%%job_id]      Bring job to foreground\n");
    printf("  bg [%%job_id]      Resume job in background\n");
    printf("  kill [-sig] [%%id] Send signal to job (default: SIGTERM)\n");
    printf("  set [VAR=value]   Set environment variable (no args: print all)\n");
    printf("  unset [VAR]       Unset environment variable\n");
    printf("  history [clear]   Show command history or clear it\n");
    return 0;
}

// Вывод списка фоновых задач
static int builtin_jobs(char **args){
    (void)args;
    JobList *list = job_list_get();
    job_list_print(list);
    return 0;
}

// Перевод задачи на передний план
// Без аргументов берёт последнюю задачу (tail)
static int builtin_fg(char **args){
    JobList *list = job_list_get();
    Job *job = NULL;
    
    if(args[1] == NULL){
        // Последняя задача (tail)
        job = list->tail;
        if(!job){
            fprintf(stderr, "fg: no current job\n");
            return 1;
        }
    } else {
        // Поддержка формата %N (пропускаем % если есть)
        const char *id_str = args[1];
        if(id_str[0] == '%'){
            id_str++;
        }
        int job_id = atoi(id_str);
        job = job_list_find_by_id(list, job_id);
        if(!job){
            fprintf(stderr, "fg: job %d not found\n", job_id);
            return 1;
        }
    }
    
    printf("%s\n", job->command_line);

    // Определяем, нужно ли послать SIGCONT (если задача была остановлена)
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

// Возобновление остановленной задачи в фоне
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
        // Поддержка формата %N (пропускаем % если есть)
        const char *id_str = args[1];
        if(id_str[0] == '%'){
            id_str++;
        }
        int job_id = atoi(id_str);
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

// Отправка сигнала задаче
// Поддерживает: kill %job_id, kill -SIGNAL %job_id
static int builtin_kill(char **args){
    if(args[1] == NULL){
        fprintf(stderr, "kill: usage: kill [-signal] [%%job_id]\n");
        return 1;
    }
    
    int sig = SIGTERM;    int arg_idx = 1;
    
    // Парсинг сигнала (-SIGNAL)
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
    
    // Поддержка формата %job_id или просто job_id
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
    
    // Обновление статуса задачи в зависимости от сигнала
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

// Установка переменных окружения
// Без аргументов выводит все переменные
static int builtin_set(char **args){
    if(args[1] == NULL){
        extern char **environ;
        for(char **env = environ; *env != NULL; env++){
            printf("%s\n", *env);
        }
        return 0;
    }

    // Парсинг формата NAME=VALUE
    char *arg = args[1];
    char *eq = strchr(arg, '=');

    if(eq == NULL){
        fprintf(stderr, "set: incorrect format\n");
        return 1;
    }

    // Разделение на имя и значение (временно модифицируем строку)
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

// Удаление переменной окружения
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

// Обёртка для системной команды ls с цветным выводом
// static int builtin_ls(char **args){
//     (void)args;
//     return system("ls --color=auto");
// }

// Вывод истории команд или её очистка
// history - вывод всей истории с номерами
// history clear - очистка истории
static int builtin_history(char **args){
    if(args[1] != NULL && strcmp(args[1], "clear") == 0){
        history_clear();
        return 0;
    }
    
    int count = history_count();
    for(int i = 0; i < count; i++){
        const char *cmd = history_get(i);
        if(cmd){
            printf("%5d  %s\n", i + 1, cmd);
        }
    }
    
    return 0;
}