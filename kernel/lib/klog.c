#include "klog.h"

#define KLOG_CAPACITY (16 * 1024)

static char storage[KLOG_CAPACITY];
static size_t start;
static size_t length;

void klog_clear(void)
{
    start = 0;
    length = 0;
}

void klog_putc(char ch)
{
    if (length < KLOG_CAPACITY) {
        storage[(start + length) % KLOG_CAPACITY] = ch;
        ++length;
        return;
    }

    storage[start] = ch;
    start = (start + 1) % KLOG_CAPACITY;
}

size_t klog_read(size_t offset, void *buffer, size_t count)
{
    char *out = buffer;
    if (!out || offset >= length) {
        return 0;
    }
    if (count > length - offset) {
        count = length - offset;
    }
    for (size_t index = 0; index < count; ++index) {
        out[index] = storage[(start + offset + index) % KLOG_CAPACITY];
    }
    return count;
}

size_t klog_size(void)
{
    return length;
}
