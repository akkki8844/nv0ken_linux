#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS    128
#define MAX_REDIRS  16
#define MAX_CMDS    64

typedef enum {
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND,
    REDIR_ERR,
    REDIR_ERR_APPEND,
    REDIR_ERR_OUT,
} RedirType;

typedef struct {
    RedirType type;
    char     *file;
} Redirect;

typedef struct {
    char     *argv[MAX_ARGS];
    int       argc;
    Redirect  redirs[MAX_REDIRS];
    int       redir_count;
} SimpleCmd;

typedef struct {
    SimpleCmd *cmds;
    int        cmd_count;
    int        background;
} Pipeline;

typedef enum {
    NODE_PIPELINE,
    NODE_AND,
    NODE_OR,
    NODE_SEQ,
    NODE_SUBSHELL,
} NodeType;

typedef struct ASTNode {
    NodeType       type;
    Pipeline       pipeline;
    struct ASTNode *left;
    struct ASTNode *right;
} ASTNode;

typedef struct {
    Pipeline *items;
    int       count;
} CmdList;

ASTNode *parse(const char *input);
void     ast_free(ASTNode *node);

#endif