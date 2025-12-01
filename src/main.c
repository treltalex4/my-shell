#define _POSIX_C_SOURCE 200809L

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

#define HOST_NAME_MAX 256
#define USER_NAME_MAX 256
#define CWD_MAX_SIZE 256

static char g_username[USER_NAME_MAX];
static char g_hostname[HOST_NAME_MAX];

static void init_prompt(void){
    struct passwd *pw = getpwuid(getuid());
    strncpy(g_username, pw ? pw->pw_name : "user", sizeof(g_username) - 1);
    gethostname(g_hostname, sizeof(g_hostname));
}

static void print_prompt(void){
    char cwd[CWD_MAX_SIZE];
    getcwd(cwd, sizeof(cwd));
    
    char *home = getenv("HOME");
    char short_cwd[CWD_MAX_SIZE];
    
    if(home && strncmp(cwd, home, strlen(home)) == 0){
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
        printf("%s@%s:%s$ ", g_username, g_hostname, short_cwd);
    } else printf("%s@%s:%s$ ", g_username, g_hostname, cwd);
    
    
}

static int has_unclosed_quotes(const char *str){
    int single = 0;
    int double_q = 0;
    
    for(size_t i = 0; str[i] != '\0'; ++i){
        if(str[i] == '\'' && !double_q){
            single = !single;
        }
        if(str[i] == '"' && !single){
            double_q = !double_q;
        }
    }
    
    return single || double_q;
}

static char* str_concat(char *s1, const char *s2){
    if(!s2) return s1;
    if(!s1) return strdup(s2);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    char *result = realloc(s1, len1 + len2 + 1);
    if(!result){
        perror("str_concat: realloc failed");
        free(s1);
        return NULL;
    }
    
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

static char* read_command(void){
    print_prompt();
    fflush(stdout);
    
    char *command = my_getline();
    if(!command){
        return NULL;
    }
    
    while(has_unclosed_quotes(command)){
        printf("> ");
        fflush(stdout);
        
        char *next_line = my_getline();
        if(!next_line){
            free(command);
            return NULL;
        }
        
        command = str_concat(command, next_line);
        free(next_line);
        
        if(!command){
            fprintf(stderr, "Error: failed to concatenate command lines\n");
            return NULL;
        }
    }
    
    return command;
}

int main(){
    init_prompt();
    job_control_init();
    job_control_setup_terminal();
    job_control_setup_signals();
    
    Lexer lexer;
    Parser parser;

    for(;;){
        // Обновляем статусы фоновых задач
        job_update_all(job_list_get());
        // Выводим уведомления о завершённых задачах
        job_notify_completed(job_list_get());
        
        char *line = read_command();
        if(!line){
            putchar('\n');
            break;
        }

        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\n'){
            line[len - 1] = '\0';
        }

        lexer_init(&lexer, line);

        TokenArray tokens;
        if(!lexer_tokenize_all(&lexer, &tokens)){
            fprintf(stderr, "tokenize failed\n");
            lexer_destroy(&lexer);
            free(line);
            continue;
        }

        parser_init(&parser, &tokens);
        ASTNode *tree = parser_parse(&parser);
        
        if(tree){
            executor_execute(tree);
            ast_free(tree);
        }

        token_array_free(&tokens);
        lexer_destroy(&lexer);
        free(line);
    }

    return 0;
}