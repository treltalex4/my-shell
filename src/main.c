//main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#include "Lexer.h"
#include "Parser.h"
#include "Executor.h"
#include "getline.h"
#include "JobControl.h"
#include "Expander.h"

#define HOST_NAME_MAX 256
#define USER_NAME_MAX 256
#define CWD_MAX_SIZE 256

static char g_username[USER_NAME_MAX];
static char g_hostname[HOST_NAME_MAX];
int g_last_exit_code = 0;
pid_t g_last_bg_pid = 0;

static void init_prompt(void){
    struct passwd *pw = getpwuid(getuid());
    strncpy(g_username, pw ? pw->pw_name : "user", sizeof(g_username) - 1);
    gethostname(g_hostname, sizeof(g_hostname));
}

#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD  "\x1b[1m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_BLUE  "\x1b[34m"
#define COLOR_CYAN  "\x1b[36m"
#define COLOR_YELLOW    "\x1b[33m"
#define COLOR_MAGENTA   "\x1b[35m"

static void print_prompt(void){
    char cwd[CWD_MAX_SIZE];
    getcwd(cwd, sizeof(cwd));
    
    char *home = getenv("HOME");
    char short_cwd[CWD_MAX_SIZE];
    char *display_cwd = cwd;
    
    if(home && strncmp(cwd, home, strlen(home)) == 0){
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
        display_cwd = short_cwd;
    }
    
    printf(COLOR_BOLD COLOR_GREEN "%s@%s" COLOR_RESET ":" 
           COLOR_BOLD COLOR_BLUE "%s" COLOR_RESET "$ ", 
           g_username, g_hostname, display_cwd);
    
    fflush(stdout);
}

int main(){
    init_prompt();
    terminal_init();
    job_control_init();
    job_control_setup_terminal();
    job_control_setup_signals();
    
    Lexer lexer;
    Parser parser;

    for(;;){
        job_update_all(job_list_get());
        job_notify_completed(job_list_get());
        
        print_prompt();
        

        terminal_enable_raw_mode();
        char *line = read_full_command();

        terminal_disable_raw_mode();
        
        if(!line){
            putchar('\n');
            break;
        }

        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\n'){
            line[len - 1] = '\0';
        }


        if(line[0] == '\0'){
            free(line);
            continue;
        }

        lexer_init(&lexer, line);

        TokenArray tokens;
        if(!lexer_tokenize_all(&lexer, &tokens)){
            fprintf(stderr, "tokenize failed\n");
            lexer_destroy(&lexer);
            free(line);
            continue;
        }

        expander_expand(&tokens);

        parser_init(&parser, &tokens);
        ASTNode *tree = parser_parse(&parser);
        
        if(tree){
            g_last_exit_code = executor_execute(tree);
            ast_free(tree);
        }

        token_array_free(&tokens);
        lexer_destroy(&lexer);
        free(line);
    }

    return 0;
}