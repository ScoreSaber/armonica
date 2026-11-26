#include <stddef.h>
#include <stdint.h>

void* memset(void* dst, int value, size_t count) {
    uint8_t* out = dst;
    for (size_t i = 0; i < count; ++i) {
        out[i] = (uint8_t)value;
    }
    return dst;
}

void* memcpy(void* dst, const void* src, size_t count) {
    uint8_t* out = dst;
    const uint8_t* in = src;
    for (size_t i = 0; i < count; ++i) {
        out[i] = in[i];
    }
    return dst;
}
