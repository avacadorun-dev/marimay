#ifndef MMAY_BASE_ALIGNMENT_H
#define MMAY_BASE_ALIGNMENT_H

#include <stddef.h>
#include <stdint.h>

/* MSVC's C17 CRT does not declare max_align_t. Its fundamental scalar
 * types are covered by these members; no allocator promises extended alignment.
 * Other toolchains retain their standard library's maximum alignment.
 * This is a storage-alignment contract, not an over-aligned allocation API. */
#if defined(_MSC_VER)
typedef union MarimayMaxAlign {
    long double floating;
    intmax_t integer;
    void *object_pointer;
    void (*function_pointer)(void);
} MarimayMaxAlign;
#else
typedef max_align_t MarimayMaxAlign;
#endif

_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(long double),
    "maximum alignment covers floating types");
_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(intmax_t),
    "maximum alignment covers integer types");
_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(long long),
    "maximum alignment covers long long integers");
_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(void *),
    "maximum alignment covers object pointers");
_Static_assert(_Alignof(MarimayMaxAlign) >= _Alignof(void (*)(void)),
    "maximum alignment covers function pointers");

#endif
