//JobControl.h
#pragma once

#include <sys/types.h>
#include <termios.h>


typedef enum {
    PROC_RUNNING,    
    PROC_STOPPED,    
    PROC_COMPLETED   
} ProcessState;

typedef struct Process {
    pid_t pid;               
    ProcessState state;      
    int exit_status;         
    char *command;           
    struct Process *next;    
} Process;

typedef enum {
    JOB_FOREGROUND,  
    JOB_BACKGROUND,  
    JOB_STOPPED,     
    JOB_COMPLETED    
} JobState;

typedef struct Job {
    int job_id;              
    pid_t pgid;              
    JobState state;          
    Process *processes;      
    char *command_line;      
    int notified;            
    struct Job *next;        
    struct Job *prev;        
} Job;

typedef struct {
    Job *head;               
    Job *tail;               
    int next_job_id;         
} JobList;

void job_control_init(void);
void job_control_cleanup(void);
void job_control_setup_terminal(void);

JobList* job_list_get(void);

Job* job_create(pid_t pgid, const char *command_line, JobState state);
void job_add_process(Job *job, pid_t pid, const char *command);
void job_list_add(JobList *list, Job *job);
void job_list_remove(JobList *list, Job *job);

Job* job_list_find_by_id(JobList *list, int job_id);
Job* job_list_find_by_pgid(JobList *list, pid_t pgid);
Job* job_list_find_by_pid(JobList *list, pid_t pid);

void job_update_all(JobList *list);
int job_is_completed(Job *job);
int job_is_stopped(Job *job);

void job_list_print(JobList *list);
void job_print(Job *job);
const char* job_state_to_string(JobState state);

int job_kill(Job *job, int signal);
int job_foreground(Job *job, int cont);
int job_background(Job *job, int cont);

//pid_t job_control_get_shell_pgid(void);
int job_control_get_terminal_fd(void);
int job_control_is_interactive(void);

void job_control_setup_signals(void);
void job_handle_sigchld(int sig);