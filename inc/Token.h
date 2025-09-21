typedef enum TokenType {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_REDIRECT_IN,
    TOKEN_REDIRECT_OUT,
    TOKEN_REDIRECT_APPEND,
    TOKEN_SEQUENCE,
    TOKEN_BACKGROUND,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct Token {
    TokenType type;
    char *lexeme;
} Token;

void token_destroy(Token *token);