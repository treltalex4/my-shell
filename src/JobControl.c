//JobControl.c

#include "JobControl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

static JobList g_job_list = {NULL, NULL, 1};
static pid_t g_shell_pgid = 0;
static struct termios g_shell_tmodes;
static int g_terminal_fd = STDIN_FILENO;
static int g_is_interactive = 0;
static sigset_t g_child_mask;

void job_control_init(void) {
    
    g_job_list.head = NULL;
    g_job_list.tail = NULL;
    g_job_list.next_job_id = 1;
    
    
    g_terminal_fd = STDIN_FILENO;
    g_is_interactive = isatty(g_terminal_fd);
    
    if (!g_is_interactive) {
        return;
    }
    
    
    sigemptyset(&g_child_mask);
    sigaddset(&g_child_mask, SIGCHLD);
    sigaddset(&g_child_mask, SIGINT);
    sigaddset(&g_child_mask, SIGTSTP);
    sigaddset(&g_child_mask, SIGTTOU);
    sigaddset(&g_child_mask, SIGTTIN);
}

void job_control_cleanup(void) {
    Job *current = g_job_list.head;
    
    while (current) {
        Job *next = current->next;
        
        Process *proc = current->processes;
        while (proc) {
            Process *next_proc = proc->next;
            free(proc->command);
            free(proc);
            proc = next_proc;
        }
        
        free(current->command_line);
        free(current);
        
        current = next;
    }
    
    g_job_list.head = NULL;
    g_job_list.tail = NULL;
    g_job_list.next_job_id = 1;
}

void job_control_setup_terminal(void) {
    if (!g_is_interactive) {
        return;
    }
    
    // Игнорируем SIGTTOU чтобы tcsetpgrp не блокировал нас
    signal(SIGTTOU, SIG_IGN);
    
    g_shell_pgid = getpid();
    
    // Получаем текущую process group
    g_shell_pgid = getpgrp();
    
    // Ждём пока shell станет foreground process group
    // (важно если shell запущен из другого shell)
    while (tcgetpgrp(g_terminal_fd) != g_shell_pgid) {
        kill(-g_shell_pgid, SIGTTIN);
        g_shell_pgid = getpgrp();
    }
    
    // Пытаемся создать свою process group (0,0 = текущий процесс становится лидером)
    // EPERM означает что мы уже session leader - это нормально
    if (setpgid(0, 0) < 0 && errno != EPERM && errno != EACCES) {
        // Не фатально - продолжаем
    }
    
    g_shell_pgid = getpgrp();
    
    // Забираем терминал
    if (tcsetpgrp(g_terminal_fd, g_shell_pgid) < 0) {
        perror("tcsetpgrp");
        // Не фатально для работы shell
    }
    
    // Сохраняем настройки терминала
    if (tcgetattr(g_terminal_fd, &g_shell_tmodes) < 0) {
        perror("tcgetattr");
    }
}

JobList* job_list_get(void) {
    return &g_job_list;
}

Job* job_create(pid_t pgid, const char *command_line, JobState state) {
    Job *job = malloc(sizeof(Job));
    if (!job) {
        perror("malloc");
        return NULL;
    }
    
    job->job_id = g_job_list.next_job_id++;
    job->pgid = pgid;
    job->state = state;
    job->processes = NULL;
    job->command_line = strdup(command_line);
    if (!job->command_line) {
        free(job);
        return NULL;
    }
    job->notified = 0;
    job->next = NULL;
    job->prev = NULL;
    
    return job;
}

void job_add_process(Job *job, pid_t pid, const char *command) {
    if (!job) {
        return;
    }
    
    Process *proc = malloc(sizeof(Process));
    if (!proc) {
        perror("malloc");
        return;
    }
    
    proc->pid = pid;
    proc->state = PROC_RUNNING;
    proc->exit_status = -1;
    proc->command = strdup(command);
    if (!proc->command) {
        free(proc);
        return;
    }
    
    proc->next = job->processes;
    job->processes = proc;
}

void job_list_add(JobList *list, Job *job) {
    if (!list || !job) {
        return;
    }

    sigset_t old_mask;
    sigprocmask(SIG_BLOCK, &g_child_mask, &old_mask);
    
    if (list->tail) {
        list->tail->next = job;
        job->prev = list->tail;
        list->tail = job;
    } else {
        list->head = list->tail = job;
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

void job_list_remove(JobList *list, Job *job) {
    if (!list || !job) {
        return;
    }
    
    if (job->prev) {
        job->prev->next = job->next;
    } else {
        list->head = job->next;
    }
    
    if (job->next) {
        job->next->prev = job->prev;
    } else {
        list->tail = job->prev;
    }
    
    Process *proc = job->processes;
    while (proc) {
        Process *next_proc = proc->next;
        free(proc->command);
        free(proc);
        proc = next_proc;
    }
    
    free(job->command_line);
    free(job);
}

Job* job_list_find_by_id(JobList *list, int job_id){
    if(!list){
        return NULL;
    }

    for(Job *j = list->head; j; j = j->next){
        if(j->job_id == job_id) return j;
    }

    return NULL;
}

Job* job_list_find_by_pgid(JobList *list, pid_t pgid){
    if(!list){
        return NULL;
    }

    for(Job *j = list->head; j; j = j->next){
        if(j->pgid == pgid) return j;
    }

    return NULL;
}

Job* job_list_find_by_pid(JobList *list, pid_t pid){
    if(!list){
        return NULL;
    }

    for(Job *j = list->head; j; j = j->next){
        for(Process *p = j->processes; p; p = p->next){
            if(p->pid == pid) return j;
        }
    }

    return NULL;
}

void job_update_all(JobList *list){
    if(!list) return;

    for(Job *j = list->head; j; j = j->next){
        for(Process *p = j->processes; p; p = p->next){
            if(p->state == PROC_COMPLETED){
                continue;
            }

            int status;
            pid_t result = waitpid(p->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

            if(result == 0){
                continue;
            }

            if(result < 0){
                if(errno == ECHILD){
                    p->state = PROC_COMPLETED;
                }
                continue;
            }

            if(WIFEXITED(status)){
                p->state = PROC_COMPLETED;
                p->exit_status = WEXITSTATUS(status);
            }
            else if(WIFSIGNALED(status)){
                p->state = PROC_COMPLETED;
                p->exit_status = 128 + WTERMSIG(status);
            }
            else if(WIFSTOPPED(status)){
                p->state = PROC_STOPPED;
            }
            else if(WIFCONTINUED(status)){
                p->state = PROC_RUNNING;
            }
        }

        if(job_is_completed(j)){
            j->state = JOB_COMPLETED;
        }
        else if(job_is_stopped(j)){
            j->state = JOB_STOPPED;
        }
    }
}

int job_is_completed(Job *job){
    if(!job){
        return -1;
    }

    for(Process *p = job->processes; p; p = p->next){
        if(p->state != PROC_COMPLETED){
            return 0;
        }
    }
    return 1;
}

int job_is_stopped(Job *job){
    if(!job){
        return -1;
    }

    int has_stopped = 0;
    for(Process *p = job->processes; p; p = p->next){
        if(p->state == PROC_RUNNING){
            return 0;  // Есть работающий - не stopped
        }
        if(p->state == PROC_STOPPED){
            has_stopped = 1;
        }
    }
    return has_stopped;  // stopped только если есть хотя бы один STOPPED
}

void job_list_print(JobList *list){
    if(!list){
        return;
    }

    Job *current_job = list->tail;
    Job *previous_job = current_job ? current_job->prev : NULL;

    for(Job *j = list->head; j; j = j->next){
        // Пропускаем завершённые jobs - они выведутся через job_notify_completed
        if(j->state == JOB_COMPLETED){
            continue;
        }
        
        char marker = ' ';
        if(j == current_job){
            marker = '+';
        }
        else if(j == previous_job){
            marker = '-';
        }

        printf("[%d]%c %s\t%s\n", j->job_id, marker, job_state_to_string(j->state), j->command_line);
    }
}

const char* job_state_to_string(JobState state){
    switch(state){
        case JOB_BACKGROUND:
        case JOB_FOREGROUND: return "Running";

        case JOB_STOPPED: return "Stopped";

        case JOB_COMPLETED: return "Done";

        default: return "Unknown";
    }
}

void job_print(Job *job){
    if(!job){
        return;
    }

    JobList *list = job_list_get();

    char marker = ' ';
    if(list->tail == job){
        marker = '+';
    }
    else if(list->tail && list->tail->prev == job){
        marker = '-';
    }

    printf("[%d]%c %s\t%s\n", job->job_id, marker, job_state_to_string(job->state), job->command_line);
}

void job_notify_completed(JobList *list){
    if(!list){
        return;
    }
    
    Job *j = list->head;
    while(j){
        Job *next = j->next;
        
        if(j->state == JOB_COMPLETED && !j->notified){
            // Выводим уведомление о завершении
            job_print(j);
            j->notified = 1;
            
            // Удаляем завершённый job из списка
            job_list_remove(list, j);
        }
        
        j = next;
    }
}

int job_kill(Job *job, int signal){
    if(!job){
        return -1;
    }

    return kill(-job->pgid, signal);
}

int job_foreground(Job *job, int cont){
    if(!job){
        return -1;
    }

    job->state = JOB_FOREGROUND;
    job->notified = 0;

    if(g_is_interactive){
        tcsetpgrp(g_terminal_fd, job->pgid);
    }

    if(cont){
        kill(-job->pgid, SIGCONT);
        // Обновляем состояние процессов - они теперь running
        for(Process *p = job->processes; p; p = p->next){
            if(p->state == PROC_STOPPED){
                p->state = PROC_RUNNING;
            }
        }
    }

    int status;
    pid_t pid;
    while(!job_is_completed(job) && !job_is_stopped(job)){
        pid = waitpid(-job->pgid, &status, WUNTRACED);
        if(pid > 0){
            // Обновляем статус конкретного процесса
            for(Process *p = job->processes; p; p = p->next){
                if(p->pid == pid){
                    if(WIFSTOPPED(status)){
                        p->state = PROC_STOPPED;
                        job->state = JOB_STOPPED;
                    } else if(WIFEXITED(status) || WIFSIGNALED(status)){
                        p->state = PROC_COMPLETED;
                        p->exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                    }
                    break;
                }
            }
        } else if(pid < 0 && errno == ECHILD){
            break;  // Нет больше дочерних процессов
        }
    }
    
    // Обновляем состояние job если все процессы завершились
    if(job_is_completed(job)){
        job->state = JOB_COMPLETED;
    }

    if(g_is_interactive){
        tcsetpgrp(g_terminal_fd, g_shell_pgid);
        tcsetattr(g_terminal_fd, TCSADRAIN, &g_shell_tmodes);
    }

    // Выводим информацию если job был остановлен (Ctrl+Z)
    if(job->state == JOB_STOPPED){
        putchar('\n');  // Новая строка после ^Z
        job_print(job);
    }

    return 0;
}

int job_background(Job *job, int cont){
    if(!job){
        return -1;
    }

    job->state = JOB_BACKGROUND;
    job->notified = 0;

    if(cont){
        kill(-job->pgid, SIGCONT);
    }

    return 0;
}

int job_control_get_terminal_fd(void){
    return g_terminal_fd;
}

int job_control_is_interactive(void){
    return g_is_interactive;
}

void job_control_setup_signals(void){
    signal(SIGCHLD, job_handle_sigchld);
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

void job_handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    job_update_all(&g_job_list);
    errno = saved_errno;
}