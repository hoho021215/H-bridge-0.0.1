/* Freestanding C runtime support. No heap, stdio or C++ constructors are used. */
#include <stddef.h>
void __libc_init_array(void) { }
void *memset(void *dest, int value, size_t count)
{
    unsigned char *out = dest;
    while (count-- != 0U) *out++ = (unsigned char)value;
    return dest;
}
void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *out = dest;
    const unsigned char *in = src;
    while (count-- != 0U) *out++ = *in++;
    return dest;
}
void *memmove(void *dest, const void *src, size_t count)
{
    unsigned char *out = dest;
    const unsigned char *in = src;
    if (out < in) {
        while (count-- != 0U) *out++ = *in++;
    } else {
        while (count != 0U) { --count; out[count] = in[count]; }
    }
    return dest;
}
