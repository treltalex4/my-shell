//History.c
// Модуль для управления историей команд shell
// Реализует сохранение/загрузку истории из файла ~/.myshell_history
// Поддерживает до MAX_HISTORY команд с циклической перезаписью

#include "History.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static History g_history = {0}; // Глобальная структура для хранения истории

// Инициализация структуры истории
// Выделяет память под массив строк (MAX_HISTORY элементов)
void history_init(void){
    if(!g_history.lines){
        g_history.capacity = MAX_HISTORY;
        g_history.lines = malloc(MAX_HISTORY * sizeof(char*));
        if(!g_history.lines){
            perror("history_init: malloc failed");
            return;
        }
        g_history.count = 0;
    }
}

// Загрузка истории из файла ~/.myshell_history
// Вызывается при старте shell
// Читает построчно из файла и добавляет в массив g_history
void history_load(void){
    history_init();

    char path[MAX_PATH_SIZE];
    const char *home = getenv("HOME");
    if(!home) return;

    snprintf(path, sizeof(path), "%s/"HISTORY_FILE, home);

    FILE *f = fopen(path, "r");
    if(!f) return;

    char line[LINE_MAX_SIZE];
    while(fgets(line, sizeof(line), f) && g_history.count < MAX_HISTORY){
        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\n'){
            line[len - 1] = '\0';
        }

        if(len > 1){
            g_history.lines[g_history.count] = strdup(line);
            if(g_history.lines[g_history.count]){
                g_history.count++;
            }
        }
    }
    fclose(f);
}

// Сохранение истории в файл ~/.myshell_history
// Вызывается при выходе из shell (exit или Ctrl+D)
// Перезаписывает файл полностью, сохраняя все команды из памяти
void history_save(void) {
    char path[256];
    const char *home = getenv("HOME");
    if (!home) return;
    
    snprintf(path, sizeof(path), "%s/.myshell_history", home);
    
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("history_save: cannot open file");
        return;
    }
    
    for (int i = 0; i < g_history.count; i++) {
        fprintf(f, "%s\n", g_history.lines[i]);
    }
    fclose(f);
}

// Добавление команды в историю
// Вызывается после успешного выполнения каждой команды
// Если достигнут лимит MAX_HISTORY, удаляет самую старую команду (FIFO)
void history_add(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    
    if (g_history.count >= MAX_HISTORY) {
        free(g_history.lines[0]);
        memmove(g_history.lines, g_history.lines + 1,
                (MAX_HISTORY - 1) * sizeof(char*));
        g_history.count--;
    }
    
    g_history.lines[g_history.count] = strdup(cmd);
    if (g_history.lines[g_history.count]) {
        g_history.count++;
    }
}

// Получение строки команды по индексу
// Используется при навигации стрелками UP/DOWN в режиме редактирования
// Возвращает NULL если индекс вне диапазона
const char* history_get(int index) {
    if (index < 0 || index >= g_history.count) return NULL;
    return g_history.lines[index];
}

// Получение текущего количества команд в истории
// Используется для проверки границ при навигации
int history_count(void){
    return g_history.count;
}

// Очистка истории команд
// Освобождает память всех строк и обнуляет счётчик
// Используется командой "history clear"
void history_clear(void) {
    for (int i = 0; i < g_history.count; i++) {
        free(g_history.lines[i]);
    }
    g_history.count = 0;
}

// Полное освобождение памяти истории
// Вызывается при завершении shell
void history_free(void) {
    history_clear();
    free(g_history.lines);
    g_history.lines = NULL;
    g_history.capacity = 0;
}