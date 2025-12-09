// JobControl.c

#include "JobControl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

// Глобальный список всех задач (jobs)
static JobList g_job_list = {NULL, NULL, 1};
// Process group ID shell (для возврата управления терминалом)
static pid_t g_shell_pgid = 0;
// Сохранённые настройки терминала shell
static struct termios g_shell_tmodes;
// Файловый дескриптор терминала
static int g_terminal_fd = STDIN_FILENO;
// Флаг интерактивного режима (есть ли терминал)
static int g_is_interactive = 0;
// Маска сигналов для блокировки во время критических операций
static sigset_t g_child_mask;

// Инициализация системы job control
// Вызывается при старте shell для настройки списка задач и маски сигналов
void job_control_init(void) {
    // Инициализация списка задач
    g_job_list.head = NULL;
    g_job_list.tail = NULL;
    g_job_list.next_job_id = 1;
    
    // Проверяем интерактивный ли режим (есть ли терминал)
    g_terminal_fd = STDIN_FILENO;
    g_is_interactive = isatty(g_terminal_fd);
    
    if (!g_is_interactive) {
        return;  // В неинтерактивном режиме job control не нужен
    }
    
    // Создаём маску сигналов для блокировки во время критических операций
    // Блокируем SIGCHLD, SIGINT, SIGTSTP, SIGTTOU, SIGTTIN
    sigemptyset(&g_child_mask);
    sigaddset(&g_child_mask, SIGCHLD);
    sigaddset(&g_child_mask, SIGINT);
    sigaddset(&g_child_mask, SIGTSTP);
    sigaddset(&g_child_mask, SIGTTOU);
    sigaddset(&g_child_mask, SIGTTIN);
}

// Освобождение всех ресурсов job control
// Вызывается при завершении shell, освобождает память всех задач и процессов
void job_control_cleanup(void) {
    Job *current = g_job_list.head;
    
    while (current) {
        Job *next = current->next;
        
        // Освобождаем все процессы в задаче
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
    
    // Сбрасываем список
    g_job_list.head = NULL;
    g_job_list.tail = NULL;
    g_job_list.next_job_id = 1;
}

// Настройка терминала для job control
// 1. Игнорируем SIGTTOU чтобы tcsetpgrp не блокировал нас
// 2. Ждём пока shell станет foreground process group
// 3. Создаём свою process group (становимся лидером)
// 4. Забираем управление терминалом через tcsetpgrp
// 5. Сохраняем настройки терминала для восстановления
void job_control_setup_terminal(void) {
    if (!g_is_interactive) {
        return;
    }
    
    // Игнорируем SIGTTOU чтобы tcsetpgrp не блокировал нас
    // (фоновый процесс получает SIGTTOU при попытке tcsetpgrp)
    signal(SIGTTOU, SIG_IGN);
    
    g_shell_pgid = getpid();
    
    // Получаем текущую process group
    g_shell_pgid = getpgrp();
    
    // Ждём пока shell станет foreground process group
    // Отправляем себе SIGTTIN пока не получим терминал
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
    
    // Забираем терминал (делаем нашу process group foreground)
    if (tcsetpgrp(g_terminal_fd, g_shell_pgid) < 0) {
        perror("tcsetpgrp");
        // Не фатально для работы shell
    }
    
    // Сохраняем настройки терминала для восстановления после fg команд
    if (tcgetattr(g_terminal_fd, &g_shell_tmodes) < 0) {
        perror("tcgetattr");
    }
}

// Получение глобального списка задач
JobList* job_list_get(void) {
    return &g_job_list;
}

// Создание новой задачи (Job)
// pgid - process group ID задачи
// command_line - строковое представление команды для отображения
// state - начальное состояние (JOB_BACKGROUND, JOB_FOREGROUND, JOB_STOPPED)
Job* job_create(pid_t pgid, const char *command_line, JobState state) {
    Job *job = malloc(sizeof(Job));
    if (!job) {
        perror("malloc");
        return NULL;
    }
    
    // Присваиваем уникальный ID и инкрементируем счётчик
    job->job_id = g_job_list.next_job_id++;
    job->pgid = pgid;
    job->state = state;
    job->processes = NULL;
    job->command_line = strdup(command_line);
    if (!job->command_line) {
        free(job);
        return NULL;
    }
    job->notified = 0;  // Ещё не уведомляли о завершении
    job->next = NULL;
    job->prev = NULL;
    
    return job;
}

// Добавление процесса к задаче
// Задача может содержать несколько процессов
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
    proc->state = PROC_RUNNING;  // Изначально все процессы запущены
    proc->exit_status = -1;
    proc->command = strdup(command);
    if (!proc->command) {
        free(proc);
        return;
    }
    
    // Добавляем в начало списка процессов
    proc->next = job->processes;
    job->processes = proc;
}

// Добавление задачи в список
// Использует блокировку сигналов для предотвращения race conditions с SIGCHLD
void job_list_add(JobList *list, Job *job) {
    if (!list || !job) {
        return;
    }

    // Блокируем сигналы на время модификации списка
    sigset_t old_mask;
    sigprocmask(SIG_BLOCK, &g_child_mask, &old_mask);
    
    // Добавляем в конец списка (tail)
    if (list->tail) {
        list->tail->next = job;
        job->prev = list->tail;
        list->tail = job;
    } else {
        // Список пустой - это первая задача
        list->head = list->tail = job;
    }

    // Восстанавливаем маску сигналов
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

// Удаление задачи из списка и освобождение памяти
void job_list_remove(JobList *list, Job *job) {
    if (!list || !job) {
        return;
    }
    
    // Отсоединяем от списка (обновляем prev/next указатели)
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
    
    // Освобождаем все процессы задачи
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

// Поиск задачи по ID (используется в fg/bg/kill командах)
Job* job_list_find_by_id(JobList *list, int job_id){
    if(!list){
        return NULL;
    }

    for(Job *j = list->head; j; j = j->next){
        if(j->job_id == job_id) return j;
    }

    return NULL;
}

// Поиск задачи по process group ID
Job* job_list_find_by_pgid(JobList *list, pid_t pgid){
    if(!list){
        return NULL;
    }

    for(Job *j = list->head; j; j = j->next){
        if(j->pgid == pgid) return j;
    }

    return NULL;
}

// Поиск задачи по PID любого из процессов в задаче
// (задача может содержать несколько процессов, например pipeline)
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

// Обновление статусов всех задач и процессов
// Вызывается из обработчика SIGCHLD и перед выводом jobs
// Использует waitpid с WNOHANG для неблокирующей проверки статусов
void job_update_all(JobList *list){
    if(!list) return;

    for(Job *j = list->head; j; j = j->next){
        for(Process *p = j->processes; p; p = p->next){
            // Пропускаем уже завершённые процессы
            if(p->state == PROC_COMPLETED){
                continue;
            }

            int status;
            // WNOHANG - не блокируемся если процесс ещё работает
            // WUNTRACED - получаем информацию об остановленных (Ctrl+Z)
            // WCONTINUED - получаем информацию о продолженных (SIGCONT)
            pid_t result = waitpid(p->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

            if(result == 0){
                continue;  // Процесс ещё работает
            }

            if(result < 0){
                if(errno == ECHILD){
                    // Процесса больше нет - помечаем как завершённый
                    p->state = PROC_COMPLETED;
                }
                continue;
            }

            // Обрабатываем различные статусы завершения
            if(WIFEXITED(status)){
                // Нормальное завершение через exit()
                p->state = PROC_COMPLETED;
                p->exit_status = WEXITSTATUS(status);
            }
            else if(WIFSIGNALED(status)){
                // Завершение по сигналу (например, SIGKILL)
                p->state = PROC_COMPLETED;
                p->exit_status = 128 + WTERMSIG(status);  // Стандартное соглашение
            }
            else if(WIFSTOPPED(status)){
                // Остановлен (Ctrl+Z)
                p->state = PROC_STOPPED;
            }
            else if(WIFCONTINUED(status)){
                // Продолжен после остановки (SIGCONT)
                p->state = PROC_RUNNING;
            }
        }

        // Обновляем состояние задачи на основе состояний процессов
        if(job_is_completed(j)){
            j->state = JOB_COMPLETED;
        }
        else if(job_is_stopped(j)){
            j->state = JOB_STOPPED;
        }
    }
}

// Проверка завершены ли все процессы в задаче
int job_is_completed(Job *job){
    if(!job){
        return -1;
    }

    for(Process *p = job->processes; p; p = p->next){
        if(p->state != PROC_COMPLETED){
            return 0;  // Есть незавершённые - задача не завершена
        }
    }
    return 1;  // Все процессы завершены
}

// Проверка остановлена ли задача (Ctrl+Z)
// Задача считается stopped если все процессы либо stopped, либо completed,
// и есть хотя бы один stopped
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

// Вывод списка всех задач (команда jobs)
// Не показывает завершённые задачи (они выводятся через job_notify_completed)
void job_list_print(JobList *list){
    if(!list){
        return;
    }

    // Определяем current (+) и previous (-) задачи
    Job *current_job = list->tail;  // Последняя добавленная
    Job *previous_job = current_job ? current_job->prev : NULL;

    for(Job *j = list->head; j; j = j->next){
        // Пропускаем завершённые jobs - они выведутся через job_notify_completed
        if(j->state == JOB_COMPLETED){
            continue;
        }
        
        // Маркер: '+' для current, '-' для previous, ' ' для остальных
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

// Преобразование состояния задачи в строку для отображения
const char* job_state_to_string(JobState state){
    switch(state){
        case JOB_BACKGROUND:
        case JOB_FOREGROUND: return "Running";

        case JOB_STOPPED: return "Stopped";

        case JOB_COMPLETED: return "Done";

        default: return "Unknown";
    }
}

// Вывод информации об одной задаче
void job_print(Job *job){
    if(!job){
        return;
    }

    JobList *list = job_list_get();

    // Определяем маркер (+ для current, - для previous)
    char marker = ' ';
    if(list->tail == job){
        marker = '+';
    }
    else if(list->tail && list->tail->prev == job){
        marker = '-';
    }

    printf("[%d]%c %s\t%s\n", job->job_id, marker, job_state_to_string(job->state), job->command_line);
}

// Уведомление о завершённых задачах и их удаление из списка
// Вызывается в начале каждого REPL цикла
void job_notify_completed(JobList *list){
    if(!list){
        return;
    }
    
    Job *j = list->head;
    while(j){
        Job *next = j->next;
        
        if(j->state == JOB_COMPLETED && !j->notified){
            // Выводим уведомление о завершении один раз
            job_print(j);
            j->notified = 1;
            
            // Удаляем завершённый job из списка
            job_list_remove(list, j);
        }
        
        j = next;
    }
}

// Отправка сигнала всем процессам в задаче
// Использует kill(-pgid, signal) для отправки всей process group
int job_kill(Job *job, int signal){
    if(!job){
        return -1;
    }

    // Минус перед pgid означает "всей process group"
    return kill(-job->pgid, signal);
}

// Перевод задачи на передний план (команда fg)
// cont - нужно ли отправить SIGCONT (если задача была остановлена)
// 1. Переводим задачу в состояние JOB_FOREGROUND
// 2. Передаём терминал задаче через tcsetpgrp
// 3. Если нужно - отправляем SIGCONT для продолжения выполнения
// 4. Ждём завершения или остановки задачи через waitpid
// 5. Возвращаем терминал shell
int job_foreground(Job *job, int cont){
    if(!job){
        return -1;
    }

    job->state = JOB_FOREGROUND;
    job->notified = 0;

    // Передаём управление терминалом задаче
    if(g_is_interactive){
        tcsetpgrp(g_terminal_fd, job->pgid);
    }

    if(cont){
        // Продолжаем выполнение остановленной задачи
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
    // Ждём пока задача не завершится или не остановится
    while(!job_is_completed(job) && !job_is_stopped(job)){
        // Ждём любой процесс из process group задачи
        pid = waitpid(-job->pgid, &status, WUNTRACED);
        if(pid > 0){
            // Обновляем статус конкретного процесса
            for(Process *p = job->processes; p; p = p->next){
                if(p->pid == pid){
                    if(WIFSTOPPED(status)){
                        // Процесс остановлен (Ctrl+Z)
                        p->state = PROC_STOPPED;
                        job->state = JOB_STOPPED;
                    } else if(WIFEXITED(status) || WIFSIGNALED(status)){
                        // Процесс завершён
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

    // Возвращаем терминал shell и восстанавливаем настройки терминала
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

// Возобновление выполнения задачи в фоне (команда bg)
// cont - нужно ли отправить SIGCONT (обычно да для остановленных задач)
int job_background(Job *job, int cont){
    if(!job){
        return -1;
    }

    job->state = JOB_BACKGROUND;
    job->notified = 0;

    if(cont){
        // Отправляем SIGCONT для продолжения выполнения
        kill(-job->pgid, SIGCONT);
    }

    return 0;
}

// Получение файлового дескриптора терминала
int job_control_get_terminal_fd(void){
    return g_terminal_fd;
}

// Проверка интерактивного режима (есть ли терминал)
int job_control_is_interactive(void){
    return g_is_interactive;
}

// Настройка обработчиков сигналов для shell
// Shell игнорирует большинство сигналов, чтобы не прерываться
// SIGCHLD обрабатывается для отслеживания завершения дочерних процессов
void job_control_setup_signals(void){
    signal(SIGCHLD, job_handle_sigchld);  // Обработчик завершения дочерних процессов
    signal(SIGINT, SIG_IGN);   // Игнорируем Ctrl+C (передаём дочернему процессу)
    signal(SIGTSTP, SIG_IGN);  // Игнорируем Ctrl+Z (передаём дочернему процессу)
    signal(SIGTTOU, SIG_IGN);  // Игнорируем сигнал при попытке записи в терминал из фона
    signal(SIGTTIN, SIG_IGN);  // Игнорируем сигнал при попытке чтения из терминала из фона
}

// Обработчик сигнала SIGCHLD
// Вызывается когда дочерний процесс завершается или меняет состояние
// Сохраняем errno чтобы не влиять на прерванные системные вызовы
void job_handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;  // Сохраняем errno (может быть испорчен waitpid)
    job_update_all(&g_job_list);  // Обновляем статусы всех задач
    errno = saved_errno;  // Восстанавливаем errno
}