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

    size_t out = 1;
    size_t in = 0;
    output[0] = '/';

    while (input[in]) {
        while (input[in] == '/') {
            ++in;
        }
        size_t component_start = in;
        while (input[in] && input[in] != '/') {
            ++in;
        }
        size_t component_length = in - component_start;
        if (component_length == 0 ||
            (component_length == 1 && input[component_start] == '.')) {
            continue;
        }
        if (component_length == 2 && input[component_start] == '.' &&
            input[component_start + 1] == '.') {
            while (out > 1 && output[out - 1] != '/') {
                --out;
            }
            if (out > 1) {
                --out;
            }
            continue;
        }

        size_t separator = out > 1 ? 1 : 0;
        if (out + separator >= output_size ||
            component_length > output_size - out - separator - 1) {
            return -1;
        }
        if (separator) {
            output[out++] = '/';
        }
        memcpy(output + out, input + component_start, component_length);
        out += component_length;
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
