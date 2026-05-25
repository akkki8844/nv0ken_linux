#ifndef NV0KEN_PROC_EXEC_H
#define NV0KEN_PROC_EXEC_H

#include <stddef.h>

#include "process.h"

int exec_image(process_t *process, const char *name, const void *image, size_t size);

#endif
