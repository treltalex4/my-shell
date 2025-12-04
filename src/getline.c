//getline.c
#include "getline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define DEFAULT_BUF_SIZE 256


static struct termios g_orig_termios;
static int g_termios_saved = 0;


typedef enum {
    KEY_CHAR,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_CTRL_A,
    KEY_CTRL_E,
    KEY_CTRL_U,
    KEY_CTRL_K,
    KEY_CTRL_D,
    KEY_CTRL_C,
    KEY_CTRL_L,
    KEY_TAB,
    KEY_ESC,
    KEY_NONE
} KeyType;


void terminal_init(void) {
    if (!g_termios_saved) {
        tcgetattr(STDIN_FILENO, &g_orig_termios);
        g_termios_saved = 1;
    }
}

void terminal_enable_raw_mode(void) {
    if (!g_termios_saved) {
        terminal_init();
    }
    
    struct termios raw = g_orig_termios;
    
    raw.c_lflag &= ~(ICANON | ECHO);
    
    raw.c_iflag &= ~(IXON);
    
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void terminal_disable_raw_mode(void) {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    }
}


static KeyType read_key(char *out_char) {
    char c;
    ssize_t nread = read(STDIN_FILENO, &c, 1);
    
    if (nread <= 0) return KEY_NONE;
    

    if (c == 1) return KEY_CTRL_A;
    if (c == 3) return KEY_CTRL_C;
    if (c == 4) return KEY_CTRL_D;
    if (c == 5) return KEY_CTRL_E;
    if (c == 11) return KEY_CTRL_K;
    if (c == 12) return KEY_CTRL_L;
    if (c == 21) return KEY_CTRL_U;
    if (c == '\t') return KEY_TAB;
    if (c == '\n' || c == '\r') return KEY_ENTER;
    if (c == 127 || c == 8) return KEY_BACKSPACE;
    

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;
        
        if (seq[0] == '[') {

            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
                case '3':
                    read(STDIN_FILENO, &seq[2], 1);
                    if (seq[2] == '~') return KEY_DELETE;
                    break;
                case '1':
                    read(STDIN_FILENO, &seq[2], 1);
                    if (seq[2] == '~') return KEY_HOME;
                    break;
                case '4':
                    read(STDIN_FILENO, &seq[2], 1);
                    if (seq[2] == '~') return KEY_END;
                    break;
            }
        } else if (seq[0] == 'O') {

            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }
        return KEY_ESC;
    }
    

    if (c >= 32 && c <= 126) {
        *out_char = c;
        return KEY_CHAR;
    }
    
    return KEY_NONE;
}


char* my_getline(void) {    
    size_t cap = DEFAULT_BUF_SIZE;
    size_t len = 0;
    size_t cursor = 0;
    
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    buf[0] = '\0';
    
    char c;
    int done = 0;
    
    while (!done) {
        KeyType key = read_key(&c);
        
        switch (key) {
        case KEY_CHAR:

            if (len + 2 >= cap) {
                cap *= 2;
                char *new_buf = realloc(buf, cap);
                if (!new_buf) {
                    free(buf);
                    return NULL;
                }
                buf = new_buf;
            }
            

            memmove(buf + cursor + 1, buf + cursor, len - cursor + 1);
            buf[cursor] = c;
            len++;
            cursor++;
            

            write(STDOUT_FILENO, &c, 1);
            if (cursor < len) {
                write(STDOUT_FILENO, "\x1b[s", 3);
                write(STDOUT_FILENO, buf + cursor, len - cursor);
                write(STDOUT_FILENO, "\x1b[u", 3);
            }
            break;
            
        case KEY_BACKSPACE:
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor, len - cursor + 1);
                len--;
                cursor--;
                
                write(STDOUT_FILENO, "\x1b[D", 3);
                write(STDOUT_FILENO, "\x1b[s", 3);
                write(STDOUT_FILENO, buf + cursor, len - cursor);
                write(STDOUT_FILENO, " ", 1);
                write(STDOUT_FILENO, "\x1b[u", 3);
            }
            break;
            
        case KEY_DELETE:
            if (cursor < len) {
                memmove(buf + cursor, buf + cursor + 1, len - cursor);
                len--;
                
                write(STDOUT_FILENO, "\x1b[s", 3);
                write(STDOUT_FILENO, buf + cursor, len - cursor);
                write(STDOUT_FILENO, " ", 1);
                write(STDOUT_FILENO, "\x1b[u", 3);
            }
            break;
            
        case KEY_LEFT:
            if (cursor > 0) {
                cursor--;
                write(STDOUT_FILENO, "\x1b[D", 3);
            }
            break;
            
        case KEY_RIGHT:
            if (cursor < len) {
                cursor++;
                write(STDOUT_FILENO, "\x1b[C", 3);
            }
            break;
            
        case KEY_HOME:
        case KEY_CTRL_A:
            if (cursor > 0) {
                char seq[32];
                snprintf(seq, sizeof(seq), "\x1b[%zuD", cursor);
                write(STDOUT_FILENO, seq, strlen(seq));
                cursor = 0;
            }
            break;
            
        case KEY_END:
        case KEY_CTRL_E:
            if (cursor < len) {
                char seq[32];
                snprintf(seq, sizeof(seq), "\x1b[%zuC", len - cursor);
                write(STDOUT_FILENO, seq, strlen(seq));
                cursor = len;
            }
            break;
            
        case KEY_CTRL_U:

            if (cursor > 0) {
                memmove(buf, buf + cursor, len - cursor + 1);
                len -= cursor;
                

                char seq[32];
                snprintf(seq, sizeof(seq), "\x1b[%zuD", cursor);
                write(STDOUT_FILENO, seq, strlen(seq));
                write(STDOUT_FILENO, "\x1b[K", 3);
                write(STDOUT_FILENO, buf, len);
                

                if (len > 0) {
                    snprintf(seq, sizeof(seq), "\x1b[%zuD", len);
                    write(STDOUT_FILENO, seq, strlen(seq));
                }
                cursor = 0;
            }
            break;
            
        case KEY_CTRL_K:

            if (cursor < len) {
                buf[cursor] = '\0';
                len = cursor;
                write(STDOUT_FILENO, "\x1b[K", 3);
            }
            break;
            
        case KEY_CTRL_L:
            write(STDOUT_FILENO, "\x1b[H\x1b[2J", 7);
            write(STDOUT_FILENO, "> ", 2);
            write(STDOUT_FILENO, buf, len);
            if (cursor < len) {
                char seq[32];
                snprintf(seq, sizeof(seq), "\x1b[%zuD", len - cursor);
                write(STDOUT_FILENO, seq, strlen(seq));
            }
            break;
            
        case KEY_CTRL_D:
            if (len == 0) {

                free(buf);
                return NULL;
            }

            if (cursor < len) {
                memmove(buf + cursor, buf + cursor + 1, len - cursor);
                len--;
                write(STDOUT_FILENO, "\x1b[s", 3);
                write(STDOUT_FILENO, buf + cursor, len - cursor);
                write(STDOUT_FILENO, " ", 1);
                write(STDOUT_FILENO, "\x1b[u", 3);
            }
            break;
            
        case KEY_CTRL_C:

            write(STDOUT_FILENO, "^C\n", 3);
            len = 0;
            cursor = 0;
            buf[0] = '\0';

            buf[0] = '\n';
            buf[1] = '\0';
            return buf;
            
        case KEY_ENTER:
            write(STDOUT_FILENO, "\n", 1);
            done = 1;
            break;
            
        case KEY_UP:
        case KEY_DOWN:

            break;
            
        case KEY_TAB:

            break;
            
        default:
            break;
        }
    }
    

    if (len + 2 >= cap) {
        char *new_buf = realloc(buf, len + 2);
        if (!new_buf) {
            free(buf);
            return NULL;
        }
        buf = new_buf;
    }
    buf[len] = '\n';
    buf[len + 1] = '\0';
    
    return buf;
}


int has_unclosed_syntax(const char *str) {
    int single = 0;
    int double_q = 0;
    int brace = 0;
    size_t len = strlen(str);
    
    for (size_t i = 0; str[i] != '\0'; ++i) {
        if (str[i] == '\'' && !double_q) {
            single = !single;
        }
        else if (str[i] == '"' && !single) {
            double_q = !double_q;
        }
        else if (str[i] == '$' && str[i+1] == '{' && !single) {
            brace++;
            i++;
        }
        else if (str[i] == '}' && brace > 0 && !single) {
            brace--;
        }
    }
    

    int backslash_continue = 0;
    if (len > 0 && str[len - 1] == '\\') {
        backslash_continue = 1;
    }
    if (len > 1 && str[len - 2] == '\\' && str[len - 1] == '\n') {
        backslash_continue = 1;
    }
    
    return single || double_q || brace || backslash_continue;
}


char* str_concat(char *s1, const char *s2) {
    if (!s2) return s1;
    if (!s1) return strdup(s2);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    char *result = realloc(s1, len1 + len2 + 1);
    if (!result) {
        free(s1);
        return NULL;
    }
    
    memcpy(result + len1, s2, len2 + 1);
    return result;
}


char* read_full_command(void) {
    char *command = my_getline();
    if (!command) {
        return NULL;
    }
    
    while (has_unclosed_syntax(command)) {
        write(STDOUT_FILENO, "> ", 2);
        
        char *next_line = my_getline();
        if (!next_line) {
            free(command);
            return NULL;
        }
        

        size_t cmd_len = strlen(command);
        if (cmd_len > 0 && command[cmd_len - 1] == '\\') {
            command[cmd_len - 1] = '\0';
        }
        else if (cmd_len > 1 && command[cmd_len - 2] == '\\' && command[cmd_len - 1] == '\n') {
            command[cmd_len - 2] = '\0';
        }
        
        command = str_concat(command, next_line);
        free(next_line);
        
        if (!command) {
            return NULL;
        }
    }
    
    return command;
}