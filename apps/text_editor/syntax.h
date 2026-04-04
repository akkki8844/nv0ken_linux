#ifndef SYNTAX_H
#define SYNTAX_H

typedef enum {
    LANG_NONE,
    LANG_C,
    LANG_PYTHON,
    LANG_JS,
    LANG_SHELL,
    LANG_ASM,
    LANG_MARKDOWN,
    LANG_JSON,
    LANG_MAKEFILE,
} SyntaxLang;

typedef enum {
    TOK_NORMAL,
    TOK_KEYWORD,
    TOK_TYPE,
    TOK_FUNCTION,
    TOK_STRING,
    TOK_NUMBER,
    TOK_COMMENT,
    TOK_PREPROCESSOR,
    TOK_VARIABLE,
    TOK_LABEL,
    TOK_DECORATOR,
    TOK_BOLD,
    TOK_ITALIC,
    TOK_HEADING1,
    TOK_HEADING2,
    TOK_HEADING3,
} SyntaxTokenType;

typedef struct {
    int             start;
    int             end;
    SyntaxTokenType type;
} SyntaxToken;

typedef struct SyntaxState SyntaxState;

SyntaxLang         syntax_detect(const char *filename);
SyntaxState       *syntax_state_new(SyntaxLang lang);
void               syntax_state_free(SyntaxState *s);
void               syntax_highlight_line(SyntaxState *s, const char *line, int len);
const SyntaxToken *syntax_tokens(SyntaxState *s, int *count);
SyntaxTokenType    syntax_token_at(SyntaxState *s, int col);
void               syntax_reset(SyntaxState *s);

#endif