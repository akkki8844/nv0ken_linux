#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

static char *xstrdup(const char *s) {
    size_t l = strlen(s);
    char *out = xmalloc(l + 1);
    if (out) memcpy(out, s, l + 1);
    return out;
}

/* -----------------------------------------------------------------------
 * Tokenizer
 * --------------------------------------------------------------------- */

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_AND,
    TOK_OR,
    TOK_SEMI,
    TOK_AMP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APPEND,
    TOK_REDIR_ERR,
    TOK_REDIR_ERR_APPEND,
    TOK_REDIR_ERR_OUT,
    TOK_NEWLINE,
    TOK_EOF,
} TokType;

typedef struct {
    TokType type;
    char   *str;
} Token;

typedef struct {
    const char *src;
    int         pos;
    Token       cur;
} Lexer;

static void tok_free(Token *t) { free(t->str); t->str = NULL; }

static Token lex_next(Lexer *lex) {
    const char *s = lex->src;
    int i = lex->pos;

    while (s[i] == ' ' || s[i] == '\t') i++;

    Token t = {0};

    if (!s[i]) { t.type = TOK_EOF; lex->pos = i; return t; }

    if (s[i] == '\n') { t.type = TOK_NEWLINE; lex->pos = i + 1; return t; }
    if (s[i] == ';')  { t.type = TOK_SEMI;    lex->pos = i + 1; return t; }
    if (s[i] == '(')  { t.type = TOK_LPAREN;  lex->pos = i + 1; return t; }
    if (s[i] == ')')  { t.type = TOK_RPAREN;  lex->pos = i + 1; return t; }

    if (s[i] == '|') {
        if (s[i+1] == '|') { t.type = TOK_OR;   lex->pos = i + 2; return t; }
        t.type = TOK_PIPE; lex->pos = i + 1; return t;
    }
    if (s[i] == '&') {
        if (s[i+1] == '&') { t.type = TOK_AND; lex->pos = i + 2; return t; }
        t.type = TOK_AMP; lex->pos = i + 1; return t;
    }
    if (s[i] == '2' && s[i+1] == '>') {
        if (s[i+2] == '>') { t.type = TOK_REDIR_ERR_APPEND; lex->pos = i + 3; return t; }
        if (s[i+2] == '&' && s[i+3] == '1') { t.type = TOK_REDIR_ERR_OUT; lex->pos = i + 4; return t; }
        t.type = TOK_REDIR_ERR; lex->pos = i + 2; return t;
    }
    if (s[i] == '>') {
        if (s[i+1] == '>') { t.type = TOK_REDIR_APPEND; lex->pos = i + 2; return t; }
        t.type = TOK_REDIR_OUT; lex->pos = i + 1; return t;
    }
    if (s[i] == '<') { t.type = TOK_REDIR_IN; lex->pos = i + 1; return t; }

    char buf[4096];
    int  len = 0;

    while (s[i] && s[i] != ' ' && s[i] != '\t' && s[i] != '\n' &&
           s[i] != '|' && s[i] != '&' && s[i] != ';' &&
           s[i] != '(' && s[i] != ')' && len < 4095) {

        if (s[i] == '\\' && s[i+1]) {
            buf[len++] = s[i+1]; i += 2; continue;
        }
        if (s[i] == '"') {
            i++;
            while (s[i] && s[i] != '"' && len < 4095) {
                if (s[i] == '\\' && s[i+1]) { buf[len++] = s[++i]; i++; continue; }
                buf[len++] = s[i++];
            }
            if (s[i] == '"') i++;
            continue;
        }
        if (s[i] == '\'') {
            i++;
            while (s[i] && s[i] != '\'' && len < 4095) buf[len++] = s[i++];
            if (s[i] == '\'') i++;
            continue;
        }
        buf[len++] = s[i++];
    }
    buf[len] = '\0';
    t.type = TOK_WORD;
    t.str  = xstrdup(buf);
    lex->pos = i;
    return t;
}

static void lex_init(Lexer *lex, const char *src) {
    lex->src = src;
    lex->pos = 0;
    lex->cur = lex_next(lex);
}

static Token lex_peek(Lexer *lex) { return lex->cur; }

static Token lex_consume(Lexer *lex) {
    Token t = lex->cur;
    lex->cur = lex_next(lex);
    return t;
}

/* -----------------------------------------------------------------------
 * Parser
 * --------------------------------------------------------------------- */

static ASTNode *node_new(NodeType type) {
    ASTNode *n = xmalloc(sizeof(ASTNode));
    if (!n) return NULL;
    memset(n, 0, sizeof(ASTNode));
    n->type = type;
    return n;
}

static SimpleCmd parse_simple_cmd(Lexer *lex) {
    SimpleCmd cmd;
    memset(&cmd, 0, sizeof(cmd));

    while (1) {
        Token t = lex_peek(lex);
        if (t.type == TOK_EOF || t.type == TOK_NEWLINE ||
            t.type == TOK_PIPE || t.type == TOK_AND ||
            t.type == TOK_OR   || t.type == TOK_SEMI ||
            t.type == TOK_AMP  || t.type == TOK_RPAREN) break;

        if (t.type == TOK_REDIR_IN || t.type == TOK_REDIR_OUT ||
            t.type == TOK_REDIR_APPEND || t.type == TOK_REDIR_ERR ||
            t.type == TOK_REDIR_ERR_APPEND || t.type == TOK_REDIR_ERR_OUT) {
            RedirType rt = (t.type == TOK_REDIR_IN)         ? REDIR_IN :
                           (t.type == TOK_REDIR_OUT)        ? REDIR_OUT :
                           (t.type == TOK_REDIR_APPEND)     ? REDIR_APPEND :
                           (t.type == TOK_REDIR_ERR)        ? REDIR_ERR :
                           (t.type == TOK_REDIR_ERR_APPEND) ? REDIR_ERR_APPEND :
                                                               REDIR_ERR_OUT;
            lex_consume(lex);
            if (rt != REDIR_ERR_OUT && cmd.redir_count < MAX_REDIRS) {
                Token f = lex_consume(lex);
                cmd.redirs[cmd.redir_count].type = rt;
                cmd.redirs[cmd.redir_count].file = f.str ? f.str : xstrdup("");
                cmd.redir_count++;
            } else if (rt == REDIR_ERR_OUT && cmd.redir_count < MAX_REDIRS) {
                cmd.redirs[cmd.redir_count].type = REDIR_ERR_OUT;
                cmd.redirs[cmd.redir_count].file = NULL;
                cmd.redir_count++;
            }
            continue;
        }

        if (t.type == TOK_WORD && cmd.argc < MAX_ARGS - 1) {
            lex_consume(lex);
            cmd.argv[cmd.argc++] = t.str;
            cmd.argv[cmd.argc]   = NULL;
            continue;
        }
        break;
    }
    return cmd;
}

static Pipeline parse_pipeline(Lexer *lex) {
    Pipeline pl;
    memset(&pl, 0, sizeof(pl));
    pl.cmds = xmalloc(sizeof(SimpleCmd) * MAX_CMDS);
    if (!pl.cmds) return pl;

    pl.cmds[pl.cmd_count++] = parse_simple_cmd(lex);

    while (lex_peek(lex).type == TOK_PIPE) {
        lex_consume(lex);
        if (pl.cmd_count < MAX_CMDS)
            pl.cmds[pl.cmd_count++] = parse_simple_cmd(lex);
    }

    if (lex_peek(lex).type == TOK_AMP) {
        pl.background = 1;
        lex_consume(lex);
    }

    return pl;
}

static ASTNode *parse_expr(Lexer *lex) {
    ASTNode *left;

    Token t = lex_peek(lex);
    if (t.type == TOK_LPAREN) {
        lex_consume(lex);
        ASTNode *inner = parse_expr(lex);
        if (lex_peek(lex).type == TOK_RPAREN) lex_consume(lex);
        left = node_new(NODE_SUBSHELL);
        if (left) left->left = inner;
    } else {
        left = node_new(NODE_PIPELINE);
        if (left) left->pipeline = parse_pipeline(lex);
    }

    while (1) {
        t = lex_peek(lex);
        NodeType op;
        if (t.type == TOK_AND)      op = NODE_AND;
        else if (t.type == TOK_OR)  op = NODE_OR;
        else if (t.type == TOK_SEMI || t.type == TOK_NEWLINE) op = NODE_SEQ;
        else break;

        lex_consume(lex);
        if (lex_peek(lex).type == TOK_EOF || lex_peek(lex).type == TOK_RPAREN)
            break;

        ASTNode *right = parse_expr(lex);
        ASTNode *seq   = node_new(op);
        if (seq) { seq->left = left; seq->right = right; }
        left = seq;
    }

    return left;
}

ASTNode *parse(const char *input) {
    if (!input || !input[0]) return NULL;
    Lexer lex;
    lex_init(&lex, input);
    return parse_expr(&lex);
}

void ast_free(ASTNode *node) {
    if (!node) return;
    if (node->type == NODE_PIPELINE) {
        for (int i = 0; i < node->pipeline.cmd_count; i++) {
            SimpleCmd *c = &node->pipeline.cmds[i];
            for (int j = 0; j < c->argc; j++) free(c->argv[j]);
            for (int j = 0; j < c->redir_count; j++) free(c->redirs[j].file);
        }
        free(node->pipeline.cmds);
    } else {
        ast_free(node->left);
        ast_free(node->right);
    }
    free(node);
}