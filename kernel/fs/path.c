#include "path.h"

#include "../lib/string.h"

bool path_is_absolute(const char *path)
{
    return path && path[0] == '/';
}

int path_normalize(const char *input, char *output, size_t output_size)
{
    if (!input || !output || output_size == 0) {
        return -1;
    }

    size_t out = 0;
    if (!path_is_absolute(input)) {
        output[out++] = '/';
    }

    for (size_t in = 0; input[in] && out + 1 < output_size; ++in) {
        if (input[in] == '/' && out > 0 && output[out - 1] == '/') {
            continue;
        }
        output[out++] = input[in];
    }

    if (out > 1 && output[out - 1] == '/') {
        --out;
    }
    output[out] = '\0';
    return 0;
}

const char *path_basename(const char *path)
{
    if (!path) {
        return "";
    }

    const char *base = path;
    for (const char *cursor = path; *cursor; ++cursor) {
        if (*cursor == '/' && cursor[1]) {
            base = cursor + 1;
        }
    }
    return base;
}

size_t path_next_component(const char **cursor, char *component, size_t component_size)
{
    if (!cursor || !*cursor || !component || component_size == 0) {
        return 0;
    }

    const char *path = *cursor;
    while (*path == '/') {
        ++path;
    }

    size_t length = 0;
    while (path[length] && path[length] != '/') {
        if (length + 1 < component_size) {
            component[length] = path[length];
        }
        ++length;
    }

    size_t copied = length < component_size ? length : component_size - 1;
    component[copied] = '\0';
    *cursor = path + length;
    return length;
}
