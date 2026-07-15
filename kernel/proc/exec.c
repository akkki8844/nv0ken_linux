#include "exec.h"

#include "../lib/string.h"
#include "elf_loader.h"
#include "scheduler.h"

int exec_image(process_t *process, const char *name, const void *image, size_t size)
{
    elf_load_result_t loaded;
    if (elf_load_process(process, image, size, &loaded) != 0) {
        return -1;
    }

    if (name) {
        size_t length = strlen(name);
        if (length >= PROCESS_NAME_MAX) {
            length = PROCESS_NAME_MAX - 1;
        }
        memcpy(process->name, name, length);
        process->name[length] = '\0';
    }

    if (!process->main_thread) {
        process->main_thread = thread_create(process->pid, loaded.entry, 0);
        if (!process->main_thread) {
            return -1;
        }
        scheduler_add(process->main_thread);
    } else {
        process->main_thread->instruction_pointer = loaded.entry;
    }
    process->state = PROCESS_READY;
    return 0;
}
