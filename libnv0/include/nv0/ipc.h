#ifndef NV0_IPC_H
#define NV0_IPC_H

#include <stddef.h>
#include <stdint.h>

int nv_ipc_connect(void);
int nv_ipc_fd(void);
int nv_ipc_send(uint32_t type, const void *payload, size_t payload_len);
int nv_ipc_poll(int timeout_ms);
void nv_ipc_disconnect(void);

int nv_clipboard_set(const char *text);
int nv_clipboard_get(char *buffer, size_t buffer_size);

#endif
