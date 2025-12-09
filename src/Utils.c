// Utils.c
// Вспомогательные функции общего назначения
// Основная функциональность:
// - print_prompt(): цветной prompt с user@host и текущим каталогом
// - buf_size_check(): проверка и автоматическое расширение буферов

#include "Utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#define DEFAULT_BUF_SIZE 32
#define HOST_NAME_MAX 256
#define USER_NAME_MAX 256
#define CWD_MAX_SIZE 256

// ANSI escape коды для цветов (формат ESC[38;2;R;G;Bm для RGB)
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD  "\x1b[1m"
#define COLOR_YELLOW    "\x1b[38;2;100;100;100m"
#define COLOR_PINK   "\x1b[38;2;249;132;239m"
#define COLOR_GREEN "\x1b[38;2;124;252;0m"           // Яркий зелёный
#define COLOR_GREEN_DARK "\x1b[38;2;60;135;0m"      // Тёмный зелёный
#define COLOR_BLUE  "\x1b[38;2;0;50;200m"
#define COLOR_LIGHT_BLUE "\x1b[38;2;0;190;255m"     // Голубой
#define COLOR_CYAN  "\x1b[38;2;0;255;255m"

// RGB цвета для фона (формат ESC[48;2;R;G;Bm) и powerline символа
#define BG_DARK_GRAY "\x1b[48;2;50;50;50m"          // Тёмно-серый фон для prompt
#define FG_LIGHT_GRAY "\x1b[38;2;100;100;100m"      // Светло-серый для разделителя
#define FG_DARK_GRAY "\x1b[38;2;50;50;50m"          // Цвет фона для powerline символа \ue0b0

// Кеш имени пользователя и хоста (инициализируются один раз)
static char g_utils_username[USER_NAME_MAX];
static char g_utils_hostname[HOST_NAME_MAX];
static int g_utils_prompt_initialized = 0;

static void init_utils_prompt(void){
    if (!g_utils_prompt_initialized) {
        struct passwd *pw = getpwuid(getuid());
        strncpy(g_utils_username, pw ? pw->pw_name : "user", sizeof(g_utils_username) - 1);
        gethostname(g_utils_hostname, sizeof(g_utils_hostname));
        g_utils_prompt_initialized = 1;
    }
}

void print_prompt(void){
    init_utils_prompt();
    
    char cwd[CWD_MAX_SIZE];
    getcwd(cwd, sizeof(cwd));
    
    char *home = getenv("HOME");
    char short_cwd[CWD_MAX_SIZE];
    char *display_cwd = cwd;
    
    if(home && strncmp(cwd, home, strlen(home)) == 0){
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
        display_cwd = short_cwd;
    }
    
    printf(BG_DARK_GRAY COLOR_BOLD COLOR_GREEN_DARK "%s" COLOR_GREEN "@%s" COLOR_RESET 
           BG_DARK_GRAY FG_LIGHT_GRAY "  " COLOR_RESET
           BG_DARK_GRAY COLOR_BOLD COLOR_LIGHT_BLUE "%s " COLOR_RESET 
           FG_DARK_GRAY "\ue0b0" COLOR_RESET " ", 
           g_utils_username, g_utils_hostname, display_cwd);
    
    fflush(stdout);
}

// Проверка размера буфера и автоматическое расширение
// Если required >= текущий размер, удваивает capacity
// Возвращает 1 при успехе, 0 при ошибке (realloc failed)
int buf_size_check(char **buf, size_t *buf_size, size_t required){
    if (!buf || !buf_size) return 0;
    
    // Текущий размер достаточен
    if(required < (*buf_size)){
        return 1;
    } else{
        // Удваиваем размер (или используем DEFAULT_BUF_SIZE если buf_size было 0)
        size_t new_cap = *buf_size ? (*buf_size * 2) : DEFAULT_BUF_SIZE;
        char *tmp = realloc(*buf, new_cap);
        if(!tmp) {
            perror("buf_size_check: realloc failed");
            return 0;
        }
        *buf = tmp;
        *buf_size = new_cap;
        return 1;
    }
}