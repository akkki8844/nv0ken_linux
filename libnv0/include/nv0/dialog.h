#ifndef NV0_DIALOG_H
#define NV0_DIALOG_H

#include <stddef.h>

void nv_dialog_info(const char *title, const char *message);
int  nv_dialog_confirm(const char *title, const char *message);
int  nv_dialog_input(const char *title, const char *prompt,
                     char *buffer, size_t buffer_size);
int  nv_dialog_open_file(const char *title, char *buffer,
                         size_t buffer_size, const char *filter);
int  nv_dialog_save_file(const char *title, char *buffer, size_t buffer_size);

#endif
