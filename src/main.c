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
#include "History.h"
#include "Utils.h"

int g_last_exit_code = 0;
pid_t g_last_bg_pid = 0;
int g_should_exit = 0;      // Флаг для выхода из shell
int g_exit_code = 0;        // Код выхода при exit
static int g_exit_attempt = 0;

int main(){
    terminal_init();
    job_control_init();
    job_control_setup_terminal();
    job_control_setup_signals();
    history_load();
    
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
            // Проверка на остановленные задачи
            int has_stopped = 0;
            JobList *list = job_list_get();
            for(Job *j = list->head; j; j = j->next){
                if(job_is_stopped(j)){
                    has_stopped = 1;
                    break;
                }
            }
            
            if(has_stopped && g_exit_attempt == 0){
                printf("\nThere are stopped jobs.\n");
                g_exit_attempt = 1;
                continue;
            }
            
            putchar('\n');
            break;
        }
        
        g_exit_attempt = 0;

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
            history_add(line);
            ast_free(tree);
        }

        token_array_free(&tokens);
        lexer_destroy(&lexer);
        free(line);
        
        // Проверяем флаг выхода (установлен командой exit)
        if(g_should_exit){
            break;
        }
    }

    history_save();
    history_free();
    job_control_cleanup();
    return g_exit_code;
}