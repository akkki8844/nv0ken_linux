#include "http_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HTTP_DEFAULT_PORT     80
#define HTTPS_DEFAULT_PORT    443
#define RECV_CHUNK_SIZE       4096
#define MAX_HEADER_SIZE       8192
#define MAX_URL_LEN           2048
#define MAX_HOST_LEN          256
#define MAX_PATH_LEN          1024
#define MAX_HEADER_NAME_LEN   128
#define MAX_HEADER_VALUE_LEN  512
#define MAX_RESPONSE_HEADERS  64
#define HTTP_VERSION          "HTTP/1.1"

static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) return NULL;
    return p;
}

static void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) return NULL;
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = xmalloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static char *xstrndup(const char *s, size_t n) {
    char *out = xmalloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static void str_tolower(char *s) {
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') *s += 32;
    }
}

static char *str_trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    *(end + 1) = '\0';
    return s;
}

/* -----------------------------------------------------------------------
 * URL parsing
 * --------------------------------------------------------------------- */

HttpUrl *http_url_parse(const char *raw_url) {
    if (!raw_url) return NULL;

    HttpUrl *url = xmalloc(sizeof(HttpUrl));
    if (!url) return NULL;
    memset(url, 0, sizeof(HttpUrl));

    char buf[MAX_URL_LEN];
    strncpy(buf, raw_url, MAX_URL_LEN - 1);
    buf[MAX_URL_LEN - 1] = '\0';

    char *p = buf;

    if (strncmp(p, "https://", 8) == 0) {
        url->scheme = xstrdup("https");
        url->port   = HTTPS_DEFAULT_PORT;
        url->tls    = 1;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        url->scheme = xstrdup("http");
        url->port   = HTTP_DEFAULT_PORT;
        url->tls    = 0;
        p += 7;
    } else {
        url->scheme = xstrdup("http");
        url->port   = HTTP_DEFAULT_PORT;
        url->tls    = 0;
    }

    char *path_start = strchr(p, '/');
    char *host_end   = path_start ? path_start : p + strlen(p);

    char host_buf[MAX_HOST_LEN];
    size_t host_len = host_end - p;
    if (host_len >= MAX_HOST_LEN) host_len = MAX_HOST_LEN - 1;
    strncpy(host_buf, p, host_len);
    host_buf[host_len] = '\0';

    char *port_sep = strchr(host_buf, ':');
    if (port_sep) {
        *port_sep = '\0';
        url->port = atoi(port_sep + 1);
    }

    url->host = xstrdup(host_buf);
    url->path = xstrdup(path_start ? path_start : "/");

    char *query = strchr(url->path, '?');
    if (query) {
        url->query    = xstrdup(query + 1);
        url->fragment = NULL;
        char *frag = strchr(url->query, '#');
        if (frag) {
            url->fragment = xstrdup(frag + 1);
            *frag = '\0';
        }
        *query = '\0';
    } else {
        char *frag = strchr(url->path, '#');
        if (frag) {
            url->fragment = xstrdup(frag + 1);
            *frag = '\0';
        }
        url->query = NULL;
    }

    if (!url->host || !url->path || !url->scheme) {
        http_url_free(url);
        return NULL;
    }

    return url;
}

void http_url_free(HttpUrl *url) {
    if (!url) return;
    free(url->scheme);
    free(url->host);
    free(url->path);
    free(url->query);
    free(url->fragment);
    free(url);
}

char *http_url_to_string(HttpUrl *url) {
    if (!url) return NULL;
    char buf[MAX_URL_LEN];
    int n = snprintf(buf, MAX_URL_LEN, "%s://%s", url->scheme, url->host);
    if (url->port != HTTP_DEFAULT_PORT && url->port != HTTPS_DEFAULT_PORT)
        n += snprintf(buf + n, MAX_URL_LEN - n, ":%d", url->port);
    n += snprintf(buf + n, MAX_URL_LEN - n, "%s", url->path ? url->path : "/");
    if (url->query)
        n += snprintf(buf + n, MAX_URL_LEN - n, "?%s", url->query);
    if (url->fragment)
        snprintf(buf + n, MAX_URL_LEN - n, "#%s", url->fragment);
    return xstrdup(buf);
}

/* -----------------------------------------------------------------------
 * HttpHeaders
 * --------------------------------------------------------------------- */

HttpHeaders *http_headers_new(void) {
    HttpHeaders *h = xmalloc(sizeof(HttpHeaders));
    if (!h) return NULL;
    h->entries = NULL;
    h->count   = 0;
    h->cap     = 0;
    return h;
}

void http_headers_free(HttpHeaders *h) {
    if (!h) return;
    for (int i = 0; i < h->count; i++) {
        free(h->entries[i].name);
        free(h->entries[i].value);
    }
    free(h->entries);
    free(h);
}

int http_headers_set(HttpHeaders *h, const char *name, const char *value) {
    if (!h || !name || !value) return -1;

    char lower_name[MAX_HEADER_NAME_LEN];
    strncpy(lower_name, name, MAX_HEADER_NAME_LEN - 1);
    lower_name[MAX_HEADER_NAME_LEN - 1] = '\0';
    str_tolower(lower_name);

    for (int i = 0; i < h->count; i++) {
        if (strcmp(h->entries[i].name, lower_name) == 0) {
            free(h->entries[i].value);
            h->entries[i].value = xstrdup(value);
            return h->entries[i].value ? 0 : -1;
        }
    }

    if (h->count >= h->cap) {
        int nc = h->cap == 0 ? 8 : h->cap * 2;
        HttpHeaderEntry *ne = xrealloc(h->entries, sizeof(HttpHeaderEntry) * nc);
        if (!ne) return -1;
        h->entries = ne;
        h->cap     = nc;
    }

    h->entries[h->count].name  = xstrdup(lower_name);
    h->entries[h->count].value = xstrdup(value);
    if (!h->entries[h->count].name || !h->entries[h->count].value) {
        free(h->entries[h->count].name);
        free(h->entries[h->count].value);
        return -1;
    }
    h->count++;
    return 0;
}

const char *http_headers_get(HttpHeaders *h, const char *name) {
    if (!h || !name) return NULL;
    char lower_name[MAX_HEADER_NAME_LEN];
    strncpy(lower_name, name, MAX_HEADER_NAME_LEN - 1);
    lower_name[MAX_HEADER_NAME_LEN - 1] = '\0';
    str_tolower(lower_name);
    for (int i = 0; i < h->count; i++)
        if (strcmp(h->entries[i].name, lower_name) == 0)
            return h->entries[i].value;
    return NULL;
}

int http_headers_has(HttpHeaders *h, const char *name) {
    return http_headers_get(h, name) != NULL;
}

void http_headers_remove(HttpHeaders *h, const char *name) {
    if (!h || !name) return;
    char lower_name[MAX_HEADER_NAME_LEN];
    strncpy(lower_name, name, MAX_HEADER_NAME_LEN - 1);
    lower_name[MAX_HEADER_NAME_LEN - 1] = '\0';
    str_tolower(lower_name);
    for (int i = 0; i < h->count; i++) {
        if (strcmp(h->entries[i].name, lower_name) == 0) {
            free(h->entries[i].name);
            free(h->entries[i].value);
            for (int j = i; j < h->count - 1; j++)
                h->entries[j] = h->entries[j + 1];
            h->count--;
            return;
        }
    }
}

/* -----------------------------------------------------------------------
 * HttpRequest
 * --------------------------------------------------------------------- */

HttpRequest *http_request_new(const char *method, const char *url_str) {
    if (!method || !url_str) return NULL;

    HttpRequest *req = xmalloc(sizeof(HttpRequest));
    if (!req) return NULL;
    memset(req, 0, sizeof(HttpRequest));

    req->method  = xstrdup(method);
    req->url     = http_url_parse(url_str);
    req->headers = http_headers_new();
    req->body    = NULL;
    req->body_len = 0;
    req->follow_redirects = 1;
    req->max_redirects    = 5;
    req->timeout_ms       = 30000;

    if (!req->method || !req->url || !req->headers) {
        http_request_free(req);
        return NULL;
    }

    http_headers_set(req->headers, "Host", req->url->host);
    http_headers_set(req->headers, "User-Agent", "nv0ken/1.0");
    http_headers_set(req->headers, "Accept", "*/*");
    http_headers_set(req->headers, "Connection", "close");

    return req;
}

void http_request_free(HttpRequest *req) {
    if (!req) return;
    free(req->method);
    http_url_free(req->url);
    http_headers_free(req->headers);
    free(req->body);
    free(req);
}

void http_request_set_body(HttpRequest *req, const char *body, size_t len) {
    if (!req) return;
    free(req->body);
    req->body = xmalloc(len);
    if (!req->body) { req->body_len = 0; return; }
    memcpy(req->body, body, len);
    req->body_len = len;

    char len_str[32];
    snprintf(len_str, sizeof(len_str), "%zu", len);
    http_headers_set(req->headers, "Content-Length", len_str);
}

void http_request_set_header(HttpRequest *req, const char *name, const char *value) {
    if (!req) return;
    http_headers_set(req->headers, name, value);
}

/* -----------------------------------------------------------------------
 * Request serialization
 * --------------------------------------------------------------------- */

static char *build_request_line(HttpRequest *req) {
    char buf[MAX_URL_LEN + 64];
    char path[MAX_URL_LEN];

    if (req->url->query)
        snprintf(path, MAX_URL_LEN, "%s?%s", req->url->path, req->url->query);
    else
        strncpy(path, req->url->path, MAX_URL_LEN - 1);

    snprintf(buf, sizeof(buf), "%s %s %s\r\n", req->method, path, HTTP_VERSION);
    return xstrdup(buf);
}

char *http_request_serialize(HttpRequest *req, size_t *out_len) {
    if (!req || !out_len) return NULL;

    char *req_line = build_request_line(req);
    if (!req_line) return NULL;

    size_t total = strlen(req_line);
    for (int i = 0; i < req->headers->count; i++) {
        total += strlen(req->headers->entries[i].name) + 2 +
                 strlen(req->headers->entries[i].value) + 2;
    }
    total += 2;
    if (req->body) total += req->body_len;

    char *out = xmalloc(total + 1);
    if (!out) { free(req_line); return NULL; }

    size_t pos = 0;
    size_t rl  = strlen(req_line);
    memcpy(out + pos, req_line, rl); pos += rl;
    free(req_line);

    for (int i = 0; i < req->headers->count; i++) {
        const char *n = req->headers->entries[i].name;
        const char *v = req->headers->entries[i].value;
        size_t nlen = strlen(n), vlen = strlen(v);
        memcpy(out + pos, n, nlen); pos += nlen;
        memcpy(out + pos, ": ", 2); pos += 2;
        memcpy(out + pos, v, vlen); pos += vlen;
        memcpy(out + pos, "\r\n", 2); pos += 2;
    }
    memcpy(out + pos, "\r\n", 2); pos += 2;

    if (req->body && req->body_len > 0) {
        memcpy(out + pos, req->body, req->body_len);
        pos += req->body_len;
    }

    out[pos] = '\0';
    *out_len = pos;
    return out;
}

/* -----------------------------------------------------------------------
 * HttpResponse
 * --------------------------------------------------------------------- */

HttpResponse *http_response_new(void) {
    HttpResponse *res = xmalloc(sizeof(HttpResponse));
    if (!res) return NULL;
    res->status_code   = 0;
    res->status_text   = NULL;
    res->headers       = http_headers_new();
    res->body          = NULL;
    res->body_len      = 0;
    res->final_url     = NULL;
    res->error         = HTTP_OK;
    return res;
}

void http_response_free(HttpResponse *res) {
    if (!res) return;
    free(res->status_text);
    http_headers_free(res->headers);
    free(res->body);
    free(res->final_url);
    free(res);
}

const char *http_response_body_str(HttpResponse *res) {
    return res ? res->body : NULL;
}

/* -----------------------------------------------------------------------
 * Response parsing
 * --------------------------------------------------------------------- */

static int parse_status_line(const char *line, int *code, char **text) {
    const char *p = line;

    if (strncmp(p, "HTTP/", 5) != 0) return -1;
    p += 5;
    while (*p && *p != ' ') p++;
    if (!*p) return -1;
    p++;

    *code = atoi(p);
    if (*code < 100 || *code > 599) return -1;

    while (*p && *p != ' ') p++;
    if (*p == ' ') p++;

    size_t tlen = strlen(p);
    while (tlen > 0 && (p[tlen-1] == '\r' || p[tlen-1] == '\n')) tlen--;
    *text = xstrndup(p, tlen);
    return 0;
}

static int parse_header_line(const char *line, char **name, char **value) {
    const char *colon = strchr(line, ':');
    if (!colon) return -1;

    size_t nlen = colon - line;
    char *n = xstrndup(line, nlen);
    if (!n) return -1;
    str_tolower(n);

    const char *vstart = colon + 1;
    while (*vstart == ' ' || *vstart == '\t') vstart++;
    size_t vlen = strlen(vstart);
    while (vlen > 0 && (vstart[vlen-1] == '\r' || vstart[vlen-1] == '\n')) vlen--;

    char *v = xstrndup(vstart, vlen);
    if (!v) { free(n); return -1; }

    *name  = n;
    *value = v;
    return 0;
}

static HttpResponse *parse_response(const char *raw, size_t raw_len) {
    HttpResponse *res = http_response_new();
    if (!res) return NULL;

    const char *p   = raw;
    const char *end = raw + raw_len;

    const char *line_end = strstr(p, "\r\n");
    if (!line_end) { res->error = HTTP_ERR_INVALID_RESPONSE; return res; }

    char status_line[512];
    size_t sl_len = line_end - p;
    if (sl_len >= sizeof(status_line)) sl_len = sizeof(status_line) - 1;
    strncpy(status_line, p, sl_len);
    status_line[sl_len] = '\0';

    if (parse_status_line(status_line, &res->status_code, &res->status_text) < 0) {
        res->error = HTTP_ERR_INVALID_RESPONSE;
        return res;
    }
    p = line_end + 2;

    while (p < end) {
        line_end = strstr(p, "\r\n");
        if (!line_end) break;
        if (line_end == p) { p += 2; break; }

        size_t hlen = line_end - p;
        char hbuf[MAX_HEADER_NAME_LEN + MAX_HEADER_VALUE_LEN + 4];
        if (hlen >= sizeof(hbuf)) hlen = sizeof(hbuf) - 1;
        strncpy(hbuf, p, hlen);
        hbuf[hlen] = '\0';

        char *hname = NULL, *hvalue = NULL;
        if (parse_header_line(hbuf, &hname, &hvalue) == 0) {
            http_headers_set(res->headers, hname, hvalue);
            free(hname);
            free(hvalue);
        }
        p = line_end + 2;
    }

    /* Body */
    size_t body_len = end - p;
    if (body_len > 0) {
        res->body = xmalloc(body_len + 1);
        if (res->body) {
            memcpy(res->body, p, body_len);
            res->body[body_len] = '\0';
            res->body_len = body_len;
        }
    }

    return res;
}

/* -----------------------------------------------------------------------
 * Chunked transfer encoding decode
 * --------------------------------------------------------------------- */

static char *decode_chunked(const char *body, size_t body_len, size_t *out_len) {
    char   *out  = xmalloc(body_len + 1);
    size_t  pos  = 0;
    size_t  wpos = 0;

    if (!out) return NULL;

    while (pos < body_len) {
        const char *crlf = strstr(body + pos, "\r\n");
        if (!crlf) break;

        size_t chunk_size = strtoul(body + pos, NULL, 16);
        pos = (crlf - body) + 2;

        if (chunk_size == 0) break;
        if (pos + chunk_size > body_len) break;

        memcpy(out + wpos, body + pos, chunk_size);
        wpos += chunk_size;
        pos  += chunk_size + 2;
    }

    out[wpos] = '\0';
    *out_len  = wpos;
    return out;
}

/* -----------------------------------------------------------------------
 * TCP socket — platform backend
 * These call into the nv0ken kernel socket syscalls.
 * On a hosted build they fall through to POSIX.
 * --------------------------------------------------------------------- */

#ifdef NV0KEN_KERNEL
#include "../../userland/libc/include/unistd.h"
#include "../../userland/libc/include/sys/types.h"
static int tcp_connect(const char *host, int port) {
    return sys_tcp_connect(host, port);
}
static int tcp_send(int fd, const char *buf, size_t len) {
    return sys_write(fd, buf, len);
}
static int tcp_recv(int fd, char *buf, size_t len) {
    return sys_read(fd, buf, len);
}
static void tcp_close(int fd) {
    sys_close(fd);
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

static int tcp_connect(const char *host, int port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

static int tcp_send(int fd, const char *buf, size_t len) {
    return (int)send(fd, buf, len, 0);
}

static int tcp_recv(int fd, char *buf, size_t len) {
    return (int)recv(fd, buf, len, 0);
}

static void tcp_close(int fd) {
    close(fd);
}
#endif

/* -----------------------------------------------------------------------
 * Receive full response into buffer
 * --------------------------------------------------------------------- */

static char *recv_all(int fd, size_t *out_len) {
    size_t  cap = RECV_CHUNK_SIZE;
    size_t  len = 0;
    char   *buf = xmalloc(cap);
    if (!buf) return NULL;

    while (1) {
        if (len + RECV_CHUNK_SIZE >= cap) {
            cap *= 2;
            char *nb = xrealloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        int n = tcp_recv(fd, buf + len, RECV_CHUNK_SIZE - 1);
        if (n <= 0) break;
        len += n;
    }

    buf[len] = '\0';
    *out_len  = len;
    return buf;
}

/* -----------------------------------------------------------------------
 * Core send/receive
 * --------------------------------------------------------------------- */

static HttpResponse *do_request(HttpRequest *req, int redirect_count) {
    if (!req || !req->url) {
        HttpResponse *err = http_response_new();
        if (err) err->error = HTTP_ERR_INVALID_URL;
        return err;
    }

    if (req->url->tls) {
        HttpResponse *err = http_response_new();
        if (err) err->error = HTTP_ERR_TLS_NOT_SUPPORTED;
        return err;
    }

    int fd = tcp_connect(req->url->host, req->url->port);
    if (fd < 0) {
        HttpResponse *err = http_response_new();
        if (err) err->error = HTTP_ERR_CONNECT;
        return err;
    }

    size_t req_len;
    char *raw_req = http_request_serialize(req, &req_len);
    if (!raw_req) {
        tcp_close(fd);
        HttpResponse *err = http_response_new();
        if (err) err->error = HTTP_ERR_OUT_OF_MEMORY;
        return err;
    }

    size_t sent = 0;
    while (sent < req_len) {
        int n = tcp_send(fd, raw_req + sent, req_len - sent);
        if (n <= 0) {
            free(raw_req);
            tcp_close(fd);
            HttpResponse *err = http_response_new();
            if (err) err->error = HTTP_ERR_SEND;
            return err;
        }
        sent += n;
    }
    free(raw_req);

    size_t raw_len;
    char *raw_res = recv_all(fd, &raw_len);
    tcp_close(fd);

    if (!raw_res) {
        HttpResponse *err = http_response_new();
        if (err) err->error = HTTP_ERR_RECV;
        return err;
    }

    HttpResponse *res = parse_response(raw_res, raw_len);
    free(raw_res);

    if (!res) return NULL;

    const char *te = http_headers_get(res->headers, "transfer-encoding");
    if (te && strstr(te, "chunked")) {
        size_t decoded_len;
        char *decoded = decode_chunked(res->body, res->body_len, &decoded_len);
        if (decoded) {
            free(res->body);
            res->body     = decoded;
            res->body_len = decoded_len;
        }
    }

    if (req->follow_redirects &&
        redirect_count < req->max_redirects &&
        (res->status_code == 301 || res->status_code == 302 ||
         res->status_code == 303 || res->status_code == 307 ||
         res->status_code == 308))
    {
        const char *location = http_headers_get(res->headers, "location");
        if (location) {
            char *new_url_str = xstrdup(location);
            http_response_free(res);

            HttpRequest *new_req = http_request_new(
                (req->method && strcmp(req->method, "POST") == 0 &&
                 res->status_code == 303) ? "GET" : req->method,
                new_url_str
            );
            free(new_url_str);

            if (!new_req) return NULL;

            HttpResponse *redirect_res = do_request(new_req, redirect_count + 1);
            if (redirect_res && !redirect_res->final_url)
                redirect_res->final_url = http_url_to_string(new_req->url);

            http_request_free(new_req);
            return redirect_res;
        }
    }

    if (!res->final_url)
        res->final_url = http_url_to_string(req->url);

    return res;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

HttpResponse *http_get(const char *url) {
    HttpRequest *req = http_request_new("GET", url);
    if (!req) return NULL;
    HttpResponse *res = do_request(req, 0);
    http_request_free(req);
    return res;
}

HttpResponse *http_post(const char *url, const char *content_type,
                        const char *body, size_t body_len) {
    HttpRequest *req = http_request_new("POST", url);
    if (!req) return NULL;
    if (content_type)
        http_request_set_header(req, "Content-Type", content_type);
    if (body && body_len > 0)
        http_request_set_body(req, body, body_len);
    HttpResponse *res = do_request(req, 0);
    http_request_free(req);
    return res;
}

HttpResponse *http_send(HttpRequest *req) {
    return do_request(req, 0);
}

const char *http_error_str(HttpError err) {
    switch (err) {
        case HTTP_OK:                    return "ok";
        case HTTP_ERR_INVALID_URL:       return "invalid url";
        case HTTP_ERR_CONNECT:           return "connection failed";
        case HTTP_ERR_SEND:              return "send failed";
        case HTTP_ERR_RECV:              return "recv failed";
        case HTTP_ERR_TIMEOUT:           return "timed out";
        case HTTP_ERR_TOO_MANY_REDIRECTS:return "too many redirects";
        case HTTP_ERR_INVALID_RESPONSE:  return "invalid response";
        case HTTP_ERR_TLS_NOT_SUPPORTED: return "tls not supported";
        case HTTP_ERR_OUT_OF_MEMORY:     return "out of memory";
        default:                         return "unknown error";
    }
}