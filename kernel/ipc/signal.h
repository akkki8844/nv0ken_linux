#ifndef NV0KEN_IPC_SIGNAL_H
#define NV0KEN_IPC_SIGNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "signal_table.h"

typedef void (*signal_handler_t)(int signal);

typedef struct {
    uint32_t pending_mask;
    uint32_t blocked_mask;
    signal_handler_t handlers[SIG_MAX];
} signal_state_t;

void signal_state_init(signal_state_t *state);
bool signal_send(signal_state_t *state, int signal);
bool signal_block(signal_state_t *state, int signal);
bool signal_unblock(signal_state_t *state, int signal);
int signal_next(signal_state_t *state);
void signal_set_handler(signal_state_t *state, int signal, signal_handler_t handler);
void signal_dispatch(signal_state_t *state);

#endif
