#ifndef MMAY_RUNTIME_GENERATED_STRING_H
#define MMAY_RUNTIME_GENERATED_STRING_H

#include "mmay/runtime/generated_slots.h"

/* Compiler-only projection of a proven live string: either a child of an exact,
 * descriptor-checked rooted container, or the just-published successful result
 * of the string constructor. The owning runtime operation already validated
 * the complete reference and string kind. No safepoint may intervene before
 * publishing its complete reference into the target root and reading its bytes.
 * String objects and bytes are immutable and nonmoving. Host values must use
 * mmay_managed_string_bytes instead of this trusted child projection. */
static inline void mmay_managed_string_generated_bytes(
    const MarimayManagedHeap *heap, MarimayManagedReference reference,
    const unsigned char **bytes, size_t *length
) {
    const MarimayManagedString *string = heap->slots[reference.slot].as.string;
    *bytes = string->bytes;
    *length = string->length;
}

#endif
