#ifndef HTML_PARSER_H
#define HTML_PARSER_H

#include <stddef.h>

typedef enum {
    HTML_NODE_DOCUMENT,
    HTML_NODE_ELEMENT,
    HTML_NODE_TEXT,
    HTML_NODE_COMMENT,
} HtmlNodeType;

typedef struct {
    char *name;
    char *value;
} HtmlAttr;

typedef struct HtmlNode {
    HtmlNodeType     type;
    char            *tag;
    char            *text;

    HtmlAttr       **attrs;
    int              attr_count;
    int              attr_cap;

    struct HtmlNode **children;
    int              child_count;
    int              child_cap;

    struct HtmlNode *parent;
} HtmlNode;

typedef struct {
    HtmlNode *root;
    HtmlNode *head;
    HtmlNode *body;
} HtmlDocument;

typedef struct {
    HtmlNode **nodes;
    int        count;
} HtmlNodeList;

HtmlDocument *html_parse(const char *src, size_t len);
void          html_document_free(HtmlDocument *doc);

HtmlNode     *html_node_new(HtmlNodeType type);
void          html_node_free(HtmlNode *node);

const char   *html_attr(HtmlNode *node, const char *name);
int           html_has_attr(HtmlNode *node, const char *name);

HtmlNode     *html_query_selector(HtmlNode *root, const char *selector);
HtmlNodeList *html_query_selector_all(HtmlNode *root, const char *selector);
void          html_node_list_free(HtmlNodeList *list);

char         *html_text_content(HtmlNode *node);
char         *html_dump_tree(HtmlNode *root);

#endif