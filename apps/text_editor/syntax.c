#include "syntax.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) return NULL;
    return q;
}

/* -----------------------------------------------------------------------
 * Keyword tables
 * --------------------------------------------------------------------- */

static const char *C_KEYWORDS[] = {
    "auto","break","case","char","const","continue","default","do",
    "double","else","enum","extern","float","for","goto","if","inline",
    "int","long","register","restrict","return","short","signed","sizeof",
    "static","struct","switch","typedef","union","unsigned","void",
    "volatile","while","_Bool","_Complex","_Imaginary","NULL","true","false",
    NULL
};

static const char *C_TYPES[] = {
    "uint8_t","uint16_t","uint32_t","uint64_t",
    "int8_t","int16_t","int32_t","int64_t",
    "size_t","ssize_t","ptrdiff_t","uintptr_t","intptr_t",
    "FILE","DIR","pid_t","uid_t","gid_t","mode_t","off_t",
    NULL
};

static const char *PY_KEYWORDS[] = {
    "False","None","True","and","as","assert","async","await",
    "break","class","continue","def","del","elif","else","except",
    "finally","for","from","global","if","import","in","is","lambda",
    "nonlocal","not","or","pass","raise","return","try","while","with","yield",
    NULL
};

static const char *JS_KEYWORDS[] = {
    "break","case","catch","class","const","continue","debugger","default",
    "delete","do","else","export","extends","false","finally","for",
    "function","if","import","in","instanceof","let","new","null","return",
    "static","super","switch","this","throw","true","try","typeof","var",
    "void","while","with","yield","async","await","of","from","=>",
    NULL
};

static const char *SHELL_KEYWORDS[] = {
    "if","then","else","elif","fi","for","while","do","done","case",
    "esac","function","in","return","exit","echo","export","source",
    "local","readonly","declare","set","unset","shift","trap","eval",
    NULL
};

static const char *ASM_KEYWORDS[] = {
    "mov","push","pop","call","ret","jmp","je","jne","jl","jle","jg","jge",
    "jz","jnz","cmp","test","add","sub","mul","div","imul","idiv","and",
    "or","xor","not","shl","shr","sar","lea","nop","int","syscall","iret",
    "hlt","sti","cli","lgdt","lidt","ltr","xchg","inc","dec","neg",
    "section","global","extern","db","dw","dd","dq","resb","resw","resd",
    "resq","times","equ","bits","use64","use32",
    NULL
};

/* -----------------------------------------------------------------------
 * Language detection
 * --------------------------------------------------------------------- */

SyntaxLang syntax_detect(const char *filename) {
    if (!filename) return LANG_NONE;
    const char *ext = strrchr(filename, '.');
    if (!ext) {
        const char *base = strrchr(filename, '/');
        base = base ? base + 1 : filename;
        if (strcmp(base, "Makefile") == 0 ||
            strcmp(base, "makefile") == 0 ||
            strcmp(base, "GNUmakefile") == 0) return LANG_MAKEFILE;
        return LANG_NONE;
    }
    ext++;
    if (strcasecmp(ext, "c")   == 0) return LANG_C;
    if (strcasecmp(ext, "h")   == 0) return LANG_C;
    if (strcasecmp(ext, "cpp") == 0) return LANG_C;
    if (strcasecmp(ext, "cc")  == 0) return LANG_C;
    if (strcasecmp(ext, "cxx") == 0) return LANG_C;
    if (strcasecmp(ext, "py")  == 0) return LANG_PYTHON;
    if (strcasecmp(ext, "js")  == 0) return LANG_JS;
    if (strcasecmp(ext, "ts")  == 0) return LANG_JS;
    if (strcasecmp(ext, "sh")  == 0) return LANG_SHELL;
    if (strcasecmp(ext, "bash")== 0) return LANG_SHELL;
    if (strcasecmp(ext, "asm") == 0) return LANG_ASM;
    if (strcasecmp(ext, "s")   == 0) return LANG_ASM;
    if (strcasecmp(ext, "md")  == 0) return LANG_MARKDOWN;
    if (strcasecmp(ext, "json")== 0) return LANG_JSON;
    if (strcasecmp(ext, "mk")  == 0) return LANG_MAKEFILE;
    return LANG_NONE;
}

/* -----------------------------------------------------------------------
 * SyntaxState
 * --------------------------------------------------------------------- */

struct SyntaxState {
    SyntaxLang  lang;
    SyntaxToken *tokens;
    int          count;
    int          cap;
    int          in_block_comment;
    int          in_string;
    char         string_char;
};

SyntaxState *syntax_state_new(SyntaxLang lang) {
    SyntaxState *s = xmalloc(sizeof(SyntaxState));
    if (!s) return NULL;
    s->lang             = lang;
    s->tokens           = NULL;
    s->count            = 0;
    s->cap              = 0;
    s->in_block_comment = 0;
    s->in_string        = 0;
    s->string_char      = 0;
    return s;
}

void syntax_state_free(SyntaxState *s) {
    if (!s) return;
    free(s->tokens);
    free(s);
}

static void token_push(SyntaxState *s, int start, int end, SyntaxTokenType type) {
    if (end <= start) return;
    if (s->count >= s->cap) {
        int nc = s->cap == 0 ? 64 : s->cap * 2;
        SyntaxToken *nt = xrealloc(s->tokens, sizeof(SyntaxToken) * nc);
        if (!nt) return;
        s->tokens = nt;
        s->cap    = nc;
    }
    s->tokens[s->count].start = start;
    s->tokens[s->count].end   = end;
    s->tokens[s->count].type  = type;
    s->count++;
}

/* -----------------------------------------------------------------------
 * Keyword matching
 * --------------------------------------------------------------------- */

static int kw_match(const char *text, int start, int end,
                    const char **table) {
    int len = end - start;
    for (int i = 0; table[i]; i++) {
        int klen = (int)strlen(table[i]);
        if (klen == len && memcmp(text + start, table[i], len) == 0)
            return 1;
    }
    return 0;
}

static int is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* -----------------------------------------------------------------------
 * C / C++ highlighter
 * --------------------------------------------------------------------- */

static void highlight_c(SyntaxState *s, const char *line, int len) {
    int i = 0;
    while (i < len) {

        if (s->in_block_comment) {
            int start = i;
            while (i < len) {
                if (i + 1 < len && line[i] == '*' && line[i+1] == '/') {
                    i += 2;
                    s->in_block_comment = 0;
                    break;
                }
                i++;
            }
            token_push(s, start, i, TOK_COMMENT);
            continue;
        }

        if (s->in_string) {
            int start = i;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == s->string_char) { i++; s->in_string = 0; break; }
                i++;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (i + 1 < len && line[i] == '/' && line[i+1] == '/') {
            token_push(s, i, len, TOK_COMMENT);
            break;
        }

        if (i + 1 < len && line[i] == '/' && line[i+1] == '*') {
            int start = i;
            i += 2;
            s->in_block_comment = 1;
            while (i < len) {
                if (i + 1 < len && line[i] == '*' && line[i+1] == '/') {
                    i += 2;
                    s->in_block_comment = 0;
                    break;
                }
                i++;
            }
            token_push(s, start, i, TOK_COMMENT);
            continue;
        }

        if (line[i] == '#') {
            int start = i;
            while (i < len && line[i] != '\n') i++;
            token_push(s, start, i, TOK_PREPROCESSOR);
            continue;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char q    = line[i];
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == q)    { i++; break; }
                i++;
            }
            if (i >= len && line[len-1] != q) {
                s->in_string   = 1;
                s->string_char = q;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (isdigit((unsigned char)line[i]) ||
            (line[i] == '0' && i + 1 < len &&
             (line[i+1] == 'x' || line[i+1] == 'X'))) {
            int start = i;
            if (line[i] == '0' && i + 1 < len &&
                (line[i+1] == 'x' || line[i+1] == 'X')) {
                i += 2;
                while (i < len && isxdigit((unsigned char)line[i])) i++;
            } else {
                while (i < len && (isdigit((unsigned char)line[i]) ||
                       line[i] == '.' || line[i] == 'f' ||
                       line[i] == 'u' || line[i] == 'l' ||
                       line[i] == 'U' || line[i] == 'L')) i++;
            }
            token_push(s, start, i, TOK_NUMBER);
            continue;
        }

        if (is_word_char(line[i])) {
            int start = i;
            while (i < len && is_word_char(line[i])) i++;
            if (kw_match(line, start, i, C_KEYWORDS))
                token_push(s, start, i, TOK_KEYWORD);
            else if (kw_match(line, start, i, C_TYPES))
                token_push(s, start, i, TOK_TYPE);
            else if (i < len && line[i] == '(')
                token_push(s, start, i, TOK_FUNCTION);
            else
                token_push(s, start, i, TOK_NORMAL);
            continue;
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * Python highlighter
 * --------------------------------------------------------------------- */

static void highlight_python(SyntaxState *s, const char *line, int len) {
    int i = 0;
    while (i < len) {
        if (s->in_string) {
            int start = i;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == s->string_char) { i++; s->in_string = 0; break; }
                i++;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (line[i] == '#') {
            token_push(s, i, len, TOK_COMMENT);
            break;
        }

        if (i + 2 < len &&
            ((line[i] == '"' && line[i+1] == '"' && line[i+2] == '"') ||
             (line[i] == '\'' && line[i+1] == '\'' && line[i+2] == '\''))) {
            char q    = line[i];
            int start = i;
            i += 3;
            while (i + 2 < len) {
                if (line[i] == q && line[i+1] == q && line[i+2] == q) {
                    i += 3; break;
                }
                i++;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char q    = line[i];
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == q)    { i++; break; }
                i++;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (isdigit((unsigned char)line[i])) {
            int start = i;
            while (i < len && (isdigit((unsigned char)line[i]) ||
                   line[i] == '.' || line[i] == '_' ||
                   line[i] == 'e' || line[i] == 'E')) i++;
            token_push(s, start, i, TOK_NUMBER);
            continue;
        }

        if (line[i] == '@') {
            int start = i++;
            while (i < len && is_word_char(line[i])) i++;
            token_push(s, start, i, TOK_DECORATOR);
            continue;
        }

        if (is_word_char(line[i])) {
            int start = i;
            while (i < len && is_word_char(line[i])) i++;
            if (kw_match(line, start, i, PY_KEYWORDS))
                token_push(s, start, i, TOK_KEYWORD);
            else if (i < len && line[i] == '(')
                token_push(s, start, i, TOK_FUNCTION);
            else
                token_push(s, start, i, TOK_NORMAL);
            continue;
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * JS highlighter
 * --------------------------------------------------------------------- */

static void highlight_js(SyntaxState *s, const char *line, int len) {
    int i = 0;
    while (i < len) {
        if (s->in_block_comment) {
            int start = i;
            while (i < len) {
                if (i + 1 < len && line[i] == '*' && line[i+1] == '/') {
                    i += 2; s->in_block_comment = 0; break;
                }
                i++;
            }
            token_push(s, start, i, TOK_COMMENT);
            continue;
        }

        if (i + 1 < len && line[i] == '/' && line[i+1] == '/') {
            token_push(s, i, len, TOK_COMMENT);
            break;
        }

        if (i + 1 < len && line[i] == '/' && line[i+1] == '*') {
            int start = i; i += 2; s->in_block_comment = 1;
            while (i < len) {
                if (i + 1 < len && line[i] == '*' && line[i+1] == '/') {
                    i += 2; s->in_block_comment = 0; break;
                }
                i++;
            }
            token_push(s, start, i, TOK_COMMENT);
            continue;
        }

        if (line[i] == '`') {
            int start = i++;
            while (i < len && line[i] != '`') {
                if (line[i] == '\\') i++;
                i++;
            }
            if (i < len) i++;
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i]; int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == q)    { i++; break; }
                i++;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (isdigit((unsigned char)line[i])) {
            int start = i;
            while (i < len && (isdigit((unsigned char)line[i]) ||
                   line[i] == '.' || line[i] == 'x' || line[i] == 'X' ||
                   isxdigit((unsigned char)line[i]))) i++;
            token_push(s, start, i, TOK_NUMBER);
            continue;
        }

        if (is_word_char(line[i])) {
            int start = i;
            while (i < len && is_word_char(line[i])) i++;
            if (kw_match(line, start, i, JS_KEYWORDS))
                token_push(s, start, i, TOK_KEYWORD);
            else if (i < len && line[i] == '(')
                token_push(s, start, i, TOK_FUNCTION);
            else
                token_push(s, start, i, TOK_NORMAL);
            continue;
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * Shell highlighter
 * --------------------------------------------------------------------- */

static void highlight_shell(SyntaxState *s, const char *line, int len) {
    int i = 0;
    while (i < len) {
        if (line[i] == '#') {
            token_push(s, i, len, TOK_COMMENT);
            break;
        }

        if (line[i] == '$') {
            int start = i++;
            if (i < len && (line[i] == '{' || line[i] == '(')) {
                char close = line[i] == '{' ? '}' : ')';
                i++;
                while (i < len && line[i] != close) i++;
                if (i < len) i++;
            } else {
                while (i < len && is_word_char(line[i])) i++;
            }
            token_push(s, start, i, TOK_VARIABLE);
            continue;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i]; int start = i++;
            while (i < len) {
                if (q == '\'' && line[i] == '\'') { i++; break; }
                if (q == '"' && line[i] == '\\')  { i += 2; continue; }
                if (line[i] == q)                 { i++; break; }
                i++;
            }
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (isdigit((unsigned char)line[i])) {
            int start = i;
            while (i < len && isdigit((unsigned char)line[i])) i++;
            token_push(s, start, i, TOK_NUMBER);
            continue;
        }

        if (is_word_char(line[i])) {
            int start = i;
            while (i < len && is_word_char(line[i])) i++;
            if (kw_match(line, start, i, SHELL_KEYWORDS))
                token_push(s, start, i, TOK_KEYWORD);
            else
                token_push(s, start, i, TOK_NORMAL);
            continue;
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * ASM highlighter
 * --------------------------------------------------------------------- */

static void highlight_asm(SyntaxState *s, const char *line, int len) {
    int i = 0;
    while (i < len) {
        if (line[i] == ';') {
            token_push(s, i, len, TOK_COMMENT);
            break;
        }

        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i]; int start = i++;
            while (i < len && line[i] != q) i++;
            if (i < len) i++;
            token_push(s, start, i, TOK_STRING);
            continue;
        }

        if (isdigit((unsigned char)line[i]) ||
            (line[i] == '0' && i + 1 < len &&
             (line[i+1] == 'x' || line[i+1] == 'X'))) {
            int start = i;
            if (line[i] == '0' && i + 1 < len &&
                (line[i+1] == 'x' || line[i+1] == 'X')) {
                i += 2;
                while (i < len && isxdigit((unsigned char)line[i])) i++;
            } else {
                while (i < len && (isdigit((unsigned char)line[i]) ||
                       line[i] == 'h' || line[i] == 'H')) i++;
            }
            token_push(s, start, i, TOK_NUMBER);
            continue;
        }

        if (line[i] == '%') {
            int start = i++;
            while (i < len && is_word_char(line[i])) i++;
            token_push(s, start, i, TOK_VARIABLE);
            continue;
        }

        if (is_word_char(line[i])) {
            int start = i;
            while (i < len && is_word_char(line[i])) i++;
            if (i < len && line[i] == ':')
                token_push(s, start, i, TOK_LABEL);
            else if (kw_match(line, start, i, ASM_KEYWORDS))
                token_push(s, start, i, TOK_KEYWORD);
            else
                token_push(s, start, i, TOK_NORMAL);
            continue;
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * Markdown highlighter
 * --------------------------------------------------------------------- */

static void highlight_markdown(SyntaxState *s, const char *line, int len) {
    if (len == 0) return;

    if (line[0] == '#') {
        int level = 0;
        while (level < len && line[level] == '#') level++;
        token_push(s, 0, len, level == 1 ? TOK_HEADING1 :
                              level == 2 ? TOK_HEADING2 : TOK_HEADING3);
        return;
    }

    if (len >= 3 && line[0] == '`' && line[1] == '`' && line[2] == '`') {
        token_push(s, 0, len, TOK_COMMENT);
        return;
    }

    if ((line[0] == '-' || line[0] == '*' || line[0] == '+') &&
        len > 1 && line[1] == ' ') {
        token_push(s, 0, 1, TOK_KEYWORD);
        token_push(s, 1, len, TOK_NORMAL);
        return;
    }

    if (isdigit((unsigned char)line[0])) {
        int i = 0;
        while (i < len && isdigit((unsigned char)line[i])) i++;
        if (i < len && line[i] == '.') {
            token_push(s, 0, i + 1, TOK_NUMBER);
            token_push(s, i + 1, len, TOK_NORMAL);
            return;
        }
    }

    int i = 0;
    while (i < len) {
        if (line[i] == '*' || line[i] == '_') {
            char delim = line[i];
            int bold = (i + 1 < len && line[i+1] == delim);
            int start = i;
            i += bold ? 2 : 1;
            while (i < len) {
                if (bold && i + 1 < len && line[i] == delim && line[i+1] == delim) {
                    i += 2; break;
                }
                if (!bold && line[i] == delim) { i++; break; }
                i++;
            }
            token_push(s, start, i, bold ? TOK_BOLD : TOK_ITALIC);
            continue;
        }
        if (line[i] == '`') {
            int start = i++;
            while (i < len && line[i] != '`') i++;
            if (i < len) i++;
            token_push(s, start, i, TOK_STRING);
            continue;
        }
        if (line[i] == '[') {
            int start = i++;
            while (i < len && line[i] != ']') i++;
            if (i < len) i++;
            token_push(s, start, i, TOK_FUNCTION);
            continue;
        }
        i++;
    }
}

/* -----------------------------------------------------------------------
 * JSON highlighter
 * --------------------------------------------------------------------- */

static void highlight_json(SyntaxState *s, const char *line, int len) {
    int i = 0;
    while (i < len) {
        if (line[i] == '"') {
            int start = i++;
            while (i < len) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == '"')  { i++; break; }
                i++;
            }
            int after = i;
            while (after < len && (line[after] == ' ' || line[after] == '\t')) after++;
            SyntaxTokenType tt = (after < len && line[after] == ':')
                                 ? TOK_FUNCTION : TOK_STRING;
            token_push(s, start, i, tt);
            continue;
        }

        if (isdigit((unsigned char)line[i]) || line[i] == '-') {
            int start = i;
            if (line[i] == '-') i++;
            while (i < len && (isdigit((unsigned char)line[i]) ||
                   line[i] == '.' || line[i] == 'e' ||
                   line[i] == 'E' || line[i] == '+' || line[i] == '-')) i++;
            token_push(s, start, i, TOK_NUMBER);
            continue;
        }

        if (is_word_char(line[i])) {
            int start = i;
            while (i < len && is_word_char(line[i])) i++;
            int len2 = i - start;
            if ((len2 == 4 && memcmp(line + start, "true",  4) == 0) ||
                (len2 == 5 && memcmp(line + start, "false", 5) == 0) ||
                (len2 == 4 && memcmp(line + start, "null",  4) == 0))
                token_push(s, start, i, TOK_KEYWORD);
            else
                token_push(s, start, i, TOK_NORMAL);
            continue;
        }

        i++;
    }
}

/* -----------------------------------------------------------------------
 * Makefile highlighter
 * --------------------------------------------------------------------- */

static void highlight_makefile(SyntaxState *s, const char *line, int len) {
    if (len == 0) return;

    if (line[0] == '#') {
        token_push(s, 0, len, TOK_COMMENT);
        return;
    }

    if (line[0] == '\t') {
        token_push(s, 0, len, TOK_PREPROCESSOR);
        return;
    }

    int i = 0;
    while (i < len) {
        if (line[i] == '$') {
            int start = i++;
            if (i < len && line[i] == '(') {
                i++;
                while (i < len && line[i] != ')') i++;
                if (i < len) i++;
            } else {
                while (i < len && is_word_char(line[i])) i++;
            }
            token_push(s, start, i, TOK_VARIABLE);
            continue;
        }

        if (line[i] == ':') {
            token_push(s, 0, i, TOK_LABEL);
            token_push(s, i, len, TOK_NORMAL);
            return;
        }

        if (line[i] == '=') {
            token_push(s, 0, i, TOK_VARIABLE);
            token_push(s, i, len, TOK_NORMAL);
            return;
        }

        i++;
    }
    token_push(s, 0, len, TOK_NORMAL);
}

/* -----------------------------------------------------------------------
 * Public: highlight a line
 * --------------------------------------------------------------------- */

void syntax_highlight_line(SyntaxState *s, const char *line, int len) {
    if (!s || !line) return;
    s->count = 0;

    switch (s->lang) {
        case LANG_C:        highlight_c(s, line, len);        break;
        case LANG_PYTHON:   highlight_python(s, line, len);   break;
        case LANG_JS:       highlight_js(s, line, len);       break;
        case LANG_SHELL:    highlight_shell(s, line, len);    break;
        case LANG_ASM:      highlight_asm(s, line, len);      break;
        case LANG_MARKDOWN: highlight_markdown(s, line, len); break;
        case LANG_JSON:     highlight_json(s, line, len);     break;
        case LANG_MAKEFILE: highlight_makefile(s, line, len); break;
        default:            break;
    }
}

const SyntaxToken *syntax_tokens(SyntaxState *s, int *count) {
    if (!s) { if (count) *count = 0; return NULL; }
    if (count) *count = s->count;
    return s->tokens;
}

void syntax_reset(SyntaxState *s) {
    if (!s) return;
    s->in_block_comment = 0;
    s->in_string        = 0;
    s->string_char      = 0;
}

SyntaxTokenType syntax_token_at(SyntaxState *s, int col) {
    if (!s) return TOK_NORMAL;
    for (int i = 0; i < s->count; i++) {
        if (col >= s->tokens[i].start && col < s->tokens[i].end)
            return s->tokens[i].type;
    }
    return TOK_NORMAL;
}