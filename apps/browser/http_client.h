#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stddef.h>

typedef enum {
    HTTP_OK                     = 0,
    HTTP_ERR_INVALID_URL        = 1,
    HTTP_ERR_CONNECT            = 2,
    HTTP_ERR_SEND               = 3,
    HTTP_ERR_RECV               = 4,
    HTTP_ERR_TIMEOUT            = 5,
    HTTP_ERR_TOO_MANY_REDIRECTS = 6,
    HTTP_ERR_INVALID_RESPONSE   = 7,
    HTTP_ERR_TLS_NOT_SUPPORTED  = 8,
    HTTP_ERR_OUT_OF_MEMORY      = 9,
} HttpError;

typedef struct {
    char *scheme;
    char *host;
    char *path;
    char *query;
    char *fragment;
    int   port;
    int   tls;
} HttpUrl;

typedef struct {
    char *name;
    char *value;
} HttpHeaderEntry;

typedef struct {
    HttpHeaderEntry *entries;
    int              count;
    int              cap;
} HttpHeaders;

typedef struct {
    char        *method;
    HttpUrl     *url;
    HttpHeaders *headers;
    char        *body;
    size_t       body_len;
    int          follow_redirects;
    int          max_redirects;
    int          timeout_ms;
} HttpRequest;

typedef struct {
    int          status_code;
    char        *status_text;
    HttpHeaders *headers;
    char        *body;
    size_t       body_len;
    char        *final_url;
    HttpError    error;
} HttpResponse;

HttpUrl      *http_url_parse(const char *raw_url);
void          http_url_free(HttpUrl *url);
char         *http_url_to_string(HttpUrl *url);

HttpHeaders  *http_headers_new(void);
void          http_headers_free(HttpHeaders *h);
int           http_headers_set(HttpHeaders *h, const char *name, const char *value);
const char   *http_headers_get(HttpHeaders *h, const char *name);
int           http_headers_has(HttpHeaders *h, const char *name);
void          http_headers_remove(HttpHeaders *h, const char *name);

HttpRequest  *http_request_new(const char *method, const char *url);
void          http_request_free(HttpRequest *req);
void          http_request_set_body(HttpRequest *req, const char *body, size_t len);
void          http_request_set_header(HttpRequest *req, const char *name, const char *value);
char         *http_request_serialize(HttpRequest *req, size_t *out_len);

HttpResponse *http_response_new(void);
void          http_response_free(HttpResponse *res);
const char   *http_response_body_str(HttpResponse *res);

HttpResponse *http_get(const char *url);
HttpResponse *http_post(const char *url, const char *content_type,
                        const char *body, size_t body_len);
HttpResponse *http_send(HttpRequest *req);

const char   *http_error_str(HttpError err);

#endif