#ifndef MMAY_BASE_ALIGNMENT_H
#define MMAY_BASE_ALIGNMENT_H

#include <stddef.h>

/* MSVC's C17 CRT omits max_align_t. Its fundamental scalar alignments are
 * represented by this C union; other toolchains retain their standard type.
 * This is a storage-alignment contract, not an over-aligned allocation API. */
#if defined(_MSC_VER)
typedef union MarimayMaxAlign {
    long double floating;
    long long integer;
    void *object_pointer;
    void (*function_pointer)(void);
} MarimayMaxAlign;
#else
typedef max_align_t MarimayMaxAlign;
#endif

_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(long double),
    "allocator storage must align floating scalars");
_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(void *),
    "allocator storage must align pointers");

#endif
