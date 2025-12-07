//Utils.c
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

#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD  "\x1b[1m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_BLUE  "\x1b[34m"
#define COLOR_CYAN  "\x1b[36m"
#define COLOR_YELLOW    "\x1b[33m"
#define COLOR_MAGENTA   "\x1b[35m"

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
    
    printf(COLOR_BOLD COLOR_YELLOW "%s@%s" COLOR_RESET ":" 
           COLOR_BOLD COLOR_MAGENTA "%s" COLOR_RESET "$ ", 
           g_utils_username, g_utils_hostname, display_cwd);
    
    fflush(stdout);
}

int buf_size_check(char **buf, size_t *buf_size, size_t required){
    if (!buf || !buf_size) return 0;
    
    if(required < (*buf_size)){
        return 1;
    } else{
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