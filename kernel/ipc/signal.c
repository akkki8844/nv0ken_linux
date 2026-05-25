#include "signal.h"

#include "../lib/string.h"

static bool valid_signal(int signal)
{
    return signal > SIG_NONE && signal < SIG_MAX;
}

void signal_state_init(signal_state_t *state)
{
    if (state) {
        memset(state, 0, sizeof(*state));
    }
}

bool signal_send(signal_state_t *state, int signal)
{
    if (!state || !valid_signal(signal)) {
        return false;
    }

    state->pending_mask |= (1u << signal);
    return true;
}

bool signal_block(signal_state_t *state, int signal)
{
    if (!state || !valid_signal(signal) || signal == SIGKILL || signal == SIGSTOP) {
        return false;
    }

    state->blocked_mask |= (1u << signal);
    return true;
}

bool signal_unblock(signal_state_t *state, int signal)
{
    if (!state || !valid_signal(signal)) {
        return false;
    }

    state->blocked_mask &= ~(1u << signal);
    return true;
}

int signal_next(signal_state_t *state)
{
    if (!state) {
        return SIG_NONE;
    }

    uint32_t deliverable = state->pending_mask & ~state->blocked_mask;
    for (int signal = 1; signal < SIG_MAX; ++signal) {
        if (deliverable & (1u << signal)) {
            state->pending_mask &= ~(1u << signal);
            return signal;
        }
    }
    return SIG_NONE;
}

void signal_set_handler(signal_state_t *state, int signal, signal_handler_t handler)
{
    if (state && valid_signal(signal)) {
        state->handlers[signal] = handler;
    }
}

void signal_dispatch(signal_state_t *state)
{
    int signal = signal_next(state);
    while (signal != SIG_NONE) {
        signal_handler_t handler = state->handlers[signal];
        if (handler) {
            handler(signal);
        }
        signal = signal_next(state);
    }
}
