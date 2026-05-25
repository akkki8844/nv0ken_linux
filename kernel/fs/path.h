#ifndef NV0KEN_FS_PATH_H
#define NV0KEN_FS_PATH_H

#include <stdbool.h>
#include <stddef.h>

bool path_is_absolute(const char *path);
int path_normalize(const char *input, char *output, size_t output_size);
const char *path_basename(const char *path);
size_t path_next_component(const char **cursor, char *component, size_t component_size);

#endif
