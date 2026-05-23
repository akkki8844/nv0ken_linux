#include "../include/nv0/dialog.h"

#include <stdio.h>
#include <string.h>

void nv_dialog_info(const char *title, const char *message)
{
    fprintf(stderr, "[%s] %s\n", title ? title : "Info", message ? message : "");
}

int nv_dialog_confirm(const char *title, const char *message)
{
    fprintf(stderr, "[%s] %s\n", title ? title : "Confirm", message ? message : "");
    return 1;
}

int nv_dialog_input(const char *title, const char *prompt,
                    char *buffer, size_t buffer_size)
{
    (void)title;
    fprintf(stderr, "%s ", prompt ? prompt : "Input:");
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    if (!fgets(buffer, (int)buffer_size, stdin)) {
        return 0;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    return 1;
}

int nv_dialog_open_file(const char *title, char *buffer,
                        size_t buffer_size, const char *filter)
{
    (void)filter;
    return nv_dialog_input(title ? title : "Open File", "Path:", buffer, buffer_size);
}

int nv_dialog_save_file(const char *title, char *buffer, size_t buffer_size)
{
    return nv_dialog_input(title ? title : "Save File", "Path:", buffer, buffer_size);
}
