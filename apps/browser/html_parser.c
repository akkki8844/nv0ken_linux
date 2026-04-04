#include "html_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_CHILDREN_CAP 8
#define INITIAL_ATTR_CAP     4
#define MAX_TAG_NAME_LEN     64

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) return NULL;
    return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) return NULL;
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *out = xmalloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *xstrdup(const char *s) {
    return xstrndup(s, strlen(s));
}

/* -----------------------------------------------------------------------
 * HtmlString — growable string used during parsing
 * --------------------------------------------------------------------- */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} HtmlString;

static HtmlString *htmlstr_new(void) {
    HtmlString *s = xmalloc(sizeof(HtmlString));
    if (!s) return NULL;
    s->cap = 64;
    s->len = 0;
    s->buf = xmalloc(s->cap);
    if (!s->buf) { free(s); return NULL; }
    s->buf[0] = '\0';
    return s;
}

static int htmlstr_append_char(HtmlString *s, char c) {
    if (s->len + 1 >= s->cap) {
        size_t new_cap = s->cap * 2;
        char *nb = xrealloc(s->buf, new_cap);
        if (!nb) return -1;
        s->buf = nb;
        s->cap = new_cap;
    }
    s->buf[s->len++] = c;
    s->buf[s->len]   = '\0';
    return 0;
}

static int htmlstr_append(HtmlString *s, const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (htmlstr_append_char(s, str[i]) < 0) return -1;
    }
    return 0;
}

static char *htmlstr_take(HtmlString *s) {
    char *out = s->buf;
    s->buf = NULL;
    free(s);
    return out;
}

static void htmlstr_free(HtmlString *s) {
    if (s) { free(s->buf); free(s); }
}

/* -----------------------------------------------------------------------
 * HtmlAttr
 * --------------------------------------------------------------------- */

static HtmlAttr *attr_new(const char *name, const char *value) {
    HtmlAttr *a = xmalloc(sizeof(HtmlAttr));
    if (!a) return NULL;
    a->name  = xstrdup(name);
    a->value = xstrdup(value ? value : "");
    if (!a->name || !a->value) {
        free(a->name); free(a->value); free(a);
        return NULL;
    }
    return a;
}

static void attr_free(HtmlAttr *a) {
    if (!a) return;
    free(a->name);
    free(a->value);
    free(a);
}

/* -----------------------------------------------------------------------
 * HtmlNode
 * --------------------------------------------------------------------- */

HtmlNode *html_node_new(HtmlNodeType type) {
    HtmlNode *node = xmalloc(sizeof(HtmlNode));
    if (!node) return NULL;
    node->type        = type;
    node->tag         = NULL;
    node->text        = NULL;
    node->attrs       = NULL;
    node->attr_count  = 0;
    node->attr_cap    = 0;
    node->children    = NULL;
    node->child_count = 0;
    node->child_cap   = 0;
    node->parent      = NULL;
    return node;
}

void html_node_free(HtmlNode *node) {
    if (!node) return;
    free(node->tag);
    free(node->text);
    for (int i = 0; i < node->attr_count; i++) attr_free(node->attrs[i]);
    free(node->attrs);
    for (int i = 0; i < node->child_count; i++) html_node_free(node->children[i]);
    free(node->children);
    free(node);
}

static int node_add_attr(HtmlNode *node, const char *name, const char *value) {
    if (node->attr_count >= node->attr_cap) {
        int nc = node->attr_cap == 0 ? INITIAL_ATTR_CAP : node->attr_cap * 2;
        HtmlAttr **na = xrealloc(node->attrs, sizeof(HtmlAttr *) * nc);
        if (!na) return -1;
        node->attrs    = na;
        node->attr_cap = nc;
    }
    HtmlAttr *a = attr_new(name, value);
    if (!a) return -1;
    node->attrs[node->attr_count++] = a;
    return 0;
}

static int node_add_child(HtmlNode *parent, HtmlNode *child) {
    if (parent->child_count >= parent->child_cap) {
        int nc = parent->child_cap == 0 ? INITIAL_CHILDREN_CAP : parent->child_cap * 2;
        HtmlNode **nc_arr = xrealloc(parent->children, sizeof(HtmlNode *) * nc);
        if (!nc_arr) return -1;
        parent->children  = nc_arr;
        parent->child_cap = nc;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    return 0;
}

/* -----------------------------------------------------------------------
 * HTML entity decoding
 * --------------------------------------------------------------------- */

static int decode_entity(const char *s, size_t len, char *out) {
    static const struct { const char *name; char ch; } named[] = {
        {"amp",'&'},{"lt",'<'},{"gt",'>'},{"quot",'"'},
        {"apos","'"[0]},{"nbsp",' '},{"copy",'c'},{"reg",'r'},{NULL,0}
    };

    if (!len) return 0;

    if (s[0] == '#') {
        long val = 0;
        if (len > 1 && (s[1] == 'x' || s[1] == 'X')) {
            char hex[16] = {0};
            if (len - 2 < sizeof(hex)) {
                memcpy(hex, s + 2, len - 2);
                val = strtol(hex, NULL, 16);
            }
        } else {
            char dec[16] = {0};
            if (len - 1 < sizeof(dec)) {
                memcpy(dec, s + 1, len - 1);
                val = strtol(dec, NULL, 10);
            }
        }
        if (val > 0 && val < 128) { *out = (char)val; return 1; }
        return 0;
    }

    char name[32] = {0};
    if (len < sizeof(name)) {
        memcpy(name, s, len);
        for (int i = 0; named[i].name; i++) {
            if (strcmp(name, named[i].name) == 0) { *out = named[i].ch; return 1; }
        }
    }
    return 0;
}

static char *decode_text(const char *raw, size_t raw_len) {
    HtmlString *out = htmlstr_new();
    if (!out) return NULL;
    size_t i = 0;
    while (i < raw_len) {
        if (raw[i] == '&') {
            size_t j = i + 1;
            while (j < raw_len && raw[j] != ';' && raw[j] != ' ' && (j - i) < 16) j++;
            if (j < raw_len && raw[j] == ';') {
                char decoded;
                if (decode_entity(raw + i + 1, j - i - 1, &decoded)) {
                    htmlstr_append_char(out, decoded);
                    i = j + 1;
                    continue;
                }
            }
            htmlstr_append_char(out, raw[i++]);
        } else {
            htmlstr_append_char(out, raw[i++]);
        }
    }
    return htmlstr_take(out);
}

/* -----------------------------------------------------------------------
 * Tokenizer
 * --------------------------------------------------------------------- */

typedef enum {
    TOK_DOCTYPE,
    TOK_OPEN_TAG,
    TOK_CLOSE_TAG,
    TOK_SELF_CLOSE_TAG,
    TOK_TEXT,
    TOK_COMMENT,
    TOK_CDATA,
    TOK_EOF,
} TokenType;

typedef struct {
    TokenType  type;
    char      *tag_name;
    HtmlAttr **attrs;
    int        attr_count;
    int        attr_cap;
    char      *text;
} Token;

typedef struct {
    const char *src;
    size_t      len;
    size_t      pos;
} Lexer;

static void lexer_init(Lexer *lex, const char *src, size_t len) {
    lex->src = src;
    lex->len = len;
    lex->pos = 0;
}

static char lexer_peek(Lexer *lex) {
    return lex->pos < lex->len ? lex->src[lex->pos] : '\0';
}

static char lexer_peek2(Lexer *lex) {
    return lex->pos + 1 < lex->len ? lex->src[lex->pos + 1] : '\0';
}

static char lexer_advance(Lexer *lex) {
    return lex->pos < lex->len ? lex->src[lex->pos++] : '\0';
}

static void lexer_skip_ws(Lexer *lex) {
    while (lex->pos < lex->len && isspace((unsigned char)lex->src[lex->pos]))
        lex->pos++;
}

static Token *token_new(TokenType type) {
    Token *t = xmalloc(sizeof(Token));
    if (!t) return NULL;
    t->type       = type;
    t->tag_name   = NULL;
    t->attrs      = NULL;
    t->attr_count = 0;
    t->attr_cap   = 0;
    t->text       = NULL;
    return t;
}

static void token_free(Token *t) {
    if (!t) return;
    free(t->tag_name);
    free(t->text);
    for (int i = 0; i < t->attr_count; i++) attr_free(t->attrs[i]);
    free(t->attrs);
    free(t);
}

static int token_add_attr(Token *t, const char *name, const char *value) {
    if (t->attr_count >= t->attr_cap) {
        int nc = t->attr_cap == 0 ? INITIAL_ATTR_CAP : t->attr_cap * 2;
        HtmlAttr **na = xrealloc(t->attrs, sizeof(HtmlAttr *) * nc);
        if (!na) return -1;
        t->attrs    = na;
        t->attr_cap = nc;
    }
    HtmlAttr *a = attr_new(name, value);
    if (!a) return -1;
    t->attrs[t->attr_count++] = a;
    return 0;
}

static char *lex_tag_name(Lexer *lex) {
    HtmlString *s = htmlstr_new();
    if (!s) return NULL;
    while (lex->pos < lex->len) {
        char c = lexer_peek(lex);
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == ':' || c == '.') {
            htmlstr_append_char(s, tolower((unsigned char)c));
            lex->pos++;
        } else {
            break;
        }
    }
    return htmlstr_take(s);
}

static char *lex_attr_value(Lexer *lex) {
    lexer_skip_ws(lex);
    if (lexer_peek(lex) != '=') return xstrdup("");
    lex->pos++;
    lexer_skip_ws(lex);

    char quote = lexer_peek(lex);
    HtmlString *s = htmlstr_new();
    if (!s) return NULL;

    if (quote == '"' || quote == '\'') {
        lex->pos++;
        while (lex->pos < lex->len && lexer_peek(lex) != quote)
            htmlstr_append_char(s, lexer_advance(lex));
        if (lex->pos < lex->len) lex->pos++;
    } else {
        while (lex->pos < lex->len) {
            char c = lexer_peek(lex);
            if (isspace((unsigned char)c) || c == '>' || c == '/') break;
            htmlstr_append_char(s, c);
            lex->pos++;
        }
    }
    return htmlstr_take(s);
}

static Token *lex_tag(Lexer *lex) {
    /* '&lt;' already consumed */

    /* Comment: <!-- ... --> */
    if (lexer_peek(lex) == '!' && lexer_peek2(lex) == '-') {
        lex->pos += 2;
        if (lexer_peek(lex) == '-') {
            lex->pos++;
            HtmlString *s = htmlstr_new();
            while (lex->pos + 2 < lex->len) {
                if (lex->src[lex->pos]   == '-' &&
                    lex->src[lex->pos+1] == '-' &&
                    lex->src[lex->pos+2] == '>') {
                    lex->pos += 3;
                    break;
                }
                htmlstr_append_char(s, lex->src[lex->pos++]);
            }
            Token *t = token_new(TOK_COMMENT);
            if (t) t->text = htmlstr_take(s);
            else   htmlstr_free(s);
            return t;
        }
    }

    /* DOCTYPE */
    if (lex->pos < lex->len && lex->src[lex->pos] == '!') {
        lex->pos++;
        while (lex->pos < lex->len && lex->src[lex->pos] != '>') lex->pos++;
        if (lex->pos < lex->len) lex->pos++;
        return token_new(TOK_DOCTYPE);
    }

    /* CDATA */
    if (lex->pos + 7 < lex->len && strncmp(lex->src + lex->pos, "![CDATA[", 8) == 0) {
        lex->pos += 8;
        HtmlString *s = htmlstr_new();
        while (lex->pos + 2 < lex->len) {
            if (lex->src[lex->pos]   == ']' &&
                lex->src[lex->pos+1] == ']' &&
                lex->src[lex->pos+2] == '>') {
                lex->pos += 3;
                break;
            }
            htmlstr_append_char(s, lex->src[lex->pos++]);
        }
        Token *t = token_new(TOK_CDATA);
        if (t) t->text = htmlstr_take(s);
        else   htmlstr_free(s);
        return t;
    }

    TokenType type = TOK_OPEN_TAG;

    /* Closing tag */
    if (lex->pos < lex->len && lex->src[lex->pos] == '/') {
        type = TOK_CLOSE_TAG;
        lex->pos++;
        lexer_skip_ws(lex);
    }

    char *tag_name = lex_tag_name(lex);
    if (!tag_name || strlen(tag_name) == 0) {
        free(tag_name);
        while (lex->pos < lex->len && lex->src[lex->pos] != '>') lex->pos++;
        if (lex->pos < lex->len) lex->pos++;
        return NULL;
    }

    Token *tok = token_new(type);
    if (!tok) { free(tag_name); return NULL; }
    tok->tag_name = tag_name;

    while (lex->pos < lex->len) {
        lexer_skip_ws(lex);
        char c = lexer_peek(lex);
        if (c == '>' || c == '\0') break;
        if (c == '/') {
            lex->pos++;
            lexer_skip_ws(lex);
            if (lexer_peek(lex) == '>') tok->type = TOK_SELF_CLOSE_TAG;
            break;
        }

        HtmlString *aname = htmlstr_new();
        while (lex->pos < lex->len) {
            char ac = lexer_peek(lex);
            if (isspace((unsigned char)ac) || ac == '=' || ac == '>' || ac == '/' || ac == '\0') break;
            htmlstr_append_char(aname, tolower((unsigned char)ac));
            lex->pos++;
        }

        char *name_str = htmlstr_take(aname);
        if (!name_str || strlen(name_str) == 0) {
            free(name_str);
            if (lex->pos < lex->len) lex->pos++;
            continue;
        }

        char *val_str = lex_attr_value(lex);
        token_add_attr(tok, name_str, val_str ? val_str : "");
        free(name_str);
        free(val_str);
    }

    if (lex->pos < lex->len && lex->src[lex->pos] == '>') lex->pos++;
    return tok;
}

static Token *lexer_next(Lexer *lex) {
    if (lex->pos >= lex->len) return token_new(TOK_EOF);

    if (lexer_peek(lex) == '<') {
        lex->pos++;
        return lex_tag(lex);
    }

    HtmlString *s = htmlstr_new();
    if (!s) return NULL;
    while (lex->pos < lex->len && lexer_peek(lex) != '<')
        htmlstr_append_char(s, lex->src[lex->pos++]);

    Token *t = token_new(TOK_TEXT);
    if (!t) { htmlstr_free(s); return NULL; }
    char *raw = htmlstr_take(s);
    t->text = decode_text(raw, strlen(raw));
    free(raw);
    return t;
}

/* -----------------------------------------------------------------------
 * Void elements and raw text elements
 * --------------------------------------------------------------------- */

static int is_void_element(const char *tag) {
    static const char *voids[] = {
        "area","base","br","col","embed","hr","img","input",
        "link","meta","param","source","track","wbr",NULL
    };
    for (int i = 0; voids[i]; i++)
        if (strcasecmp(tag, voids[i]) == 0) return 1;
    return 0;
}

static int is_raw_text_element(const char *tag) {
    return strcasecmp(tag, "script") == 0 || strcasecmp(tag, "style") == 0;
}

/* -----------------------------------------------------------------------
 * Parser
 * --------------------------------------------------------------------- */

typedef struct {
    Lexer    *lex;
    HtmlNode *root;
    HtmlNode **stack;
    int       stack_depth;
    int       stack_cap;
} Parser;

static HtmlNode *parser_current(Parser *p) {
    return p->stack_depth == 0 ? p->root : p->stack[p->stack_depth - 1];
}

static int parser_push(Parser *p, HtmlNode *node) {
    if (p->stack_depth >= p->stack_cap) {
        int nc = p->stack_cap == 0 ? 32 : p->stack_cap * 2;
        HtmlNode **ns = xrealloc(p->stack, sizeof(HtmlNode *) * nc);
        if (!ns) return -1;
        p->stack     = ns;
        p->stack_cap = nc;
    }
    p->stack[p->stack_depth++] = node;
    return 0;
}

static void parser_close_tag(Parser *p, const char *tag) {
    for (int i = p->stack_depth - 1; i >= 0; i--) {
        if (p->stack[i]->tag && strcasecmp(p->stack[i]->tag, tag) == 0) {
            p->stack_depth = i;
            return;
        }
    }
}

static HtmlNode *parser_consume_raw(Lexer *lex, const char *tag) {
    HtmlString *s = htmlstr_new();
    if (!s) return NULL;
    size_t taglen = strlen(tag);
    while (lex->pos < lex->len) {
        if (lex->src[lex->pos] == '<' &&
            lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '/' &&
            lex->pos + 2 + taglen < lex->len &&
            strncasecmp(lex->src + lex->pos + 2, tag, taglen) == 0) {
            char after = lex->src[lex->pos + 2 + taglen];
            if (after == '>' || isspace((unsigned char)after)) {
                while (lex->pos < lex->len && lex->src[lex->pos] != '>') lex->pos++;
                if (lex->pos < lex->len) lex->pos++;
                break;
            }
        }
        htmlstr_append_char(s, lex->src[lex->pos++]);
    }
    HtmlNode *node = html_node_new(HTML_NODE_TEXT);
    if (!node) { htmlstr_free(s); return NULL; }
    node->text = htmlstr_take(s);
    return node;
}

static void parser_run(Parser *p) {
    while (1) {
        Token *tok = lexer_next(p->lex);
        if (!tok) continue;
        if (tok->type == TOK_EOF) { token_free(tok); break; }

        if (tok->type == TOK_COMMENT) {
            HtmlNode *c = html_node_new(HTML_NODE_COMMENT);
            if (c) {
                c->text = xstrdup(tok->text ? tok->text : "");
                node_add_child(parser_current(p), c);
            }
            token_free(tok);
            continue;
        }

        if (tok->type == TOK_DOCTYPE || tok->type == TOK_CDATA) {
            token_free(tok);
            continue;
        }

        if (tok->type == TOK_TEXT) {
            int all_ws = 1;
            if (tok->text) {
                for (char *c = tok->text; *c; c++) {
                    if (!isspace((unsigned char)*c)) { all_ws = 0; break; }
                }
            }
            if (!all_ws && tok->text && tok->text[0]) {
                HtmlNode *tn = html_node_new(HTML_NODE_TEXT);
                if (tn) {
                    tn->text = xstrdup(tok->text);
                    node_add_child(parser_current(p), tn);
                }
            }
            token_free(tok);
            continue;
        }

        if (tok->type == TOK_OPEN_TAG || tok->type == TOK_SELF_CLOSE_TAG) {
            HtmlNode *elem = html_node_new(HTML_NODE_ELEMENT);
            if (!elem) { token_free(tok); continue; }
            elem->tag = xstrdup(tok->tag_name);
            for (int i = 0; i < tok->attr_count; i++)
                node_add_attr(elem, tok->attrs[i]->name, tok->attrs[i]->value);
            node_add_child(parser_current(p), elem);

            int self_closing = tok->type == TOK_SELF_CLOSE_TAG || is_void_element(tok->tag_name);

            if (!self_closing) {
                if (is_raw_text_element(tok->tag_name)) {
                    char saved[MAX_TAG_NAME_LEN];
                    strncpy(saved, tok->tag_name, MAX_TAG_NAME_LEN - 1);
                    saved[MAX_TAG_NAME_LEN - 1] = '\0';
                    token_free(tok);
                    HtmlNode *raw = parser_consume_raw(p->lex, saved);
                    if (raw) node_add_child(elem, raw);
                    continue;
                }
                parser_push(p, elem);
            }
            token_free(tok);
            continue;
        }

        if (tok->type == TOK_CLOSE_TAG) {
            parser_close_tag(p, tok->tag_name);
            token_free(tok);
            continue;
        }

        token_free(tok);
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

HtmlDocument *html_parse(const char *src, size_t len) {
    if (!src || len == 0) return NULL;

    HtmlDocument *doc = xmalloc(sizeof(HtmlDocument));
    if (!doc) return NULL;

    doc->root = html_node_new(HTML_NODE_DOCUMENT);
    if (!doc->root) { free(doc); return NULL; }
    doc->root->tag = xstrdup("#document");
    doc->head = NULL;
    doc->body = NULL;

    Lexer lex;
    lexer_init(&lex, src, len);

    Parser parser;
    parser.lex         = &lex;
    parser.root        = doc->root;
    parser.stack       = NULL;
    parser.stack_depth = 0;
    parser.stack_cap   = 0;

    parser_run(&parser);
    free(parser.stack);

    doc->head = html_query_selector(doc->root, "head");
    doc->body = html_query_selector(doc->root, "body");

    return doc;
}

void html_document_free(HtmlDocument *doc) {
    if (!doc) return;
    html_node_free(doc->root);
    free(doc);
}

const char *html_attr(HtmlNode *node, const char *name) {
    if (!node || !name) return NULL;
    for (int i = 0; i < node->attr_count; i++)
        if (strcasecmp(node->attrs[i]->name, name) == 0)
            return node->attrs[i]->value;
    return NULL;
}

int html_has_attr(HtmlNode *node, const char *name) {
    return html_attr(node, name) != NULL;
}

/* -----------------------------------------------------------------------
 * Selector matching — tag, #id, .class, tag#id, tag.class
 * --------------------------------------------------------------------- */

static int node_matches(HtmlNode *node, const char *sel) {
    if (!node || node->type != HTML_NODE_ELEMENT || !node->tag) return 0;

    char buf[256];
    strncpy(buf, sel, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tag_part = NULL, *id_part = NULL, *class_part = NULL;
    char *hash = strchr(buf, '#');
    char *dot  = strchr(buf, '.');

    if (hash && dot) {
        if (hash < dot) {
            *hash = '\0'; *dot = '\0';
            tag_part = buf; id_part = hash + 1; class_part = dot + 1;
        } else {
            *dot = '\0'; *hash = '\0';
            tag_part = buf; class_part = dot + 1; id_part = hash + 1;
        }
    } else if (hash) {
        *hash = '\0'; tag_part = buf; id_part = hash + 1;
    } else if (dot) {
        *dot = '\0'; tag_part = buf; class_part = dot + 1;
    } else {
        tag_part = buf;
    }

    if (tag_part && strlen(tag_part) > 0)
        if (strcasecmp(node->tag, tag_part) != 0) return 0;

    if (id_part && strlen(id_part) > 0) {
        const char *nid = html_attr(node, "id");
        if (!nid || strcmp(nid, id_part) != 0) return 0;
    }

    if (class_part && strlen(class_part) > 0) {
        const char *nc = html_attr(node, "class");
        if (!nc) return 0;
        int found = 0;
        const char *p = nc;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            const char *start = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            size_t clen = p - start;
            if (clen == strlen(class_part) && strncmp(start, class_part, clen) == 0) {
                found = 1; break;
            }
        }
        if (!found) return 0;
    }

    return 1;
}

HtmlNode *html_query_selector(HtmlNode *root, const char *selector) {
    if (!root || !selector) return NULL;
    if (node_matches(root, selector)) return root;
    for (int i = 0; i < root->child_count; i++) {
        HtmlNode *r = html_query_selector(root->children[i], selector);
        if (r) return r;
    }
    return NULL;
}

static int collect_all(HtmlNode *node, const char *sel,
                        HtmlNode ***out, int *count, int *cap) {
    if (!node) return 0;
    if (node_matches(node, sel)) {
        if (*count >= *cap) {
            int nc = *cap == 0 ? 16 : *cap * 2;
            HtmlNode **nb = xrealloc(*out, sizeof(HtmlNode *) * nc);
            if (!nb) return -1;
            *out = nb; *cap = nc;
        }
        (*out)[(*count)++] = node;
    }
    for (int i = 0; i < node->child_count; i++)
        if (collect_all(node->children[i], sel, out, count, cap) < 0) return -1;
    return 0;
}

HtmlNodeList *html_query_selector_all(HtmlNode *root, const char *selector) {
    HtmlNodeList *list = xmalloc(sizeof(HtmlNodeList));
    if (!list) return NULL;
    list->nodes = NULL;
    list->count = 0;
    int cap = 0;
    collect_all(root, selector, &list->nodes, &list->count, &cap);
    return list;
}

void html_node_list_free(HtmlNodeList *list) {
    if (!list) return;
    free(list->nodes);
    free(list);
}

/* -----------------------------------------------------------------------
 * Text content extraction
 * --------------------------------------------------------------------- */

static void collect_text(HtmlNode *node, HtmlString *out) {
    if (!node) return;
    if (node->type == HTML_NODE_TEXT && node->text) {
        htmlstr_append(out, node->text, strlen(node->text));
        return;
    }
    for (int i = 0; i < node->child_count; i++)
        collect_text(node->children[i], out);
}

char *html_text_content(HtmlNode *node) {
    HtmlString *s = htmlstr_new();
    if (!s) return NULL;
    collect_text(node, s);
    return htmlstr_take(s);
}

/* -----------------------------------------------------------------------
 * Debug tree dump
 * --------------------------------------------------------------------- */

static void node_dump_r(HtmlNode *node, int depth, HtmlString *out) {
    if (!node) return;
    for (int i = 0; i < depth; i++) htmlstr_append(out, "  ", 2);

    switch (node->type) {
        case HTML_NODE_DOCUMENT:
            htmlstr_append(out, "#document\n", 10);
            break;
        case HTML_NODE_ELEMENT:
            htmlstr_append(out, "<", 1);
            if (node->tag) htmlstr_append(out, node->tag, strlen(node->tag));
            for (int i = 0; i < node->attr_count; i++) {
                htmlstr_append(out, " ", 1);
                htmlstr_append(out, node->attrs[i]->name, strlen(node->attrs[i]->name));
                htmlstr_append(out, "=\"", 2);
                htmlstr_append(out, node->attrs[i]->value, strlen(node->attrs[i]->value));
                htmlstr_append(out, "\"", 1);
            }
            htmlstr_append(out, ">\n", 2);
            break;
        case HTML_NODE_TEXT:
            htmlstr_append(out, "\"", 1);
            if (node->text) {
                size_t tlen = strlen(node->text);
                size_t show = tlen > 60 ? 60 : tlen;
                htmlstr_append(out, node->text, show);
                if (tlen > 60) htmlstr_append(out, "...", 3);
            }
            htmlstr_append(out, "\"\n", 2);
            break;
        case HTML_NODE_COMMENT:
            htmlstr_append(out, "<!--", 4);
            if (node->text) {
                size_t tlen = strlen(node->text);
                size_t show = tlen > 40 ? 40 : tlen;
                htmlstr_append(out, node->text, show);
            }
            htmlstr_append(out, "-->\n", 4);
            break;
        default:
            break;
    }

    for (int i = 0; i < node->child_count; i++)
        node_dump_r(node->children[i], depth + 1, out);
}

char *html_dump_tree(HtmlNode *root) {
    HtmlString *s = htmlstr_new();
    if (!s) return NULL;
    node_dump_r(root, 0, s);
    return htmlstr_take(s);
}