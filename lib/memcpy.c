#include <stddef.h>

void *memcpy(
    void *destination,
    const void *source,
    size_t count
)
{
    volatile unsigned char *dst =
        (volatile unsigned char *)destination;

    const volatile unsigned char *src =
        (const volatile unsigned char *)source;

    for (size_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }

    return destination;
}