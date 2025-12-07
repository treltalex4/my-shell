//History.h
#pragma once

#define HISTORY_FILE ".myshell_history"
#define MAX_HISTORY 500
#define MAX_PATH_SIZE 256
#define LINE_MAX_SIZE 1024

typedef struct {
    char **lines;
    int count;
    int capacity;
} History;

void history_init(void);
void history_load(void);
void history_save(void);
void history_add(const char *cmd);
const char *history_get(int index);
const char *history_get_last(void);
int history_count(void);
// void history_clear(void);
// History* history_get_global(void);