#include "vm_hot_strings.h"
#include "mmay/runtime/string.h"

static bool bind(MarimayBytecodeHotStrings *cache,
    MarimayBytecodeExecution *execution, MarimayManagedReference reference,
    MarimayBytecodeHotStringView *result
) {
    for (size_t i = 0; i < cache->count; i += 1) {
        if (mmay_managed_reference_equal(cache->entries[i].reference, reference)) {
            *result = cache->entries[i];
            cache->next = i ^ 1;
            return true;
        }
    }
    MarimayBytecodeHotStringView view = {.reference = reference};
    if (execution == NULL || !execution->initialized
        || mmay_managed_string_bytes(&execution->managed_heap, reference,
            &view.bytes, &view.length) != MMAY_MANAGED_OK) return false;
    size_t i = cache->count < 2 ? cache->count++ : cache->next;
    cache->entries[i] = view;
    cache->next = i ^ 1;
    *result = view;
    return true;
}

bool mmay_bytecode_hot_string_read(MarimayBytecodeHotStrings *cache,
    MarimayBytecodeExecution *execution, const MarimayManagedArrayView *array,
    int64_t index, MarimayManagedArrayAccessMode mode,
    MarimayBytecodeValue *target
) {
    const MarimayManagedArrayDescriptor *d = array->descriptor;
    if (target->type != MMAY_BYTECODE_TYPE_STRING
        || d->storage_kind != MMAY_MANAGED_ARRAY_STORAGE_CELL
        || d->storage_stride != sizeof(MarimayManagedCell)
        || d->cell_kind != MMAY_MANAGED_CELL_REFERENCE
        || d->reference_kind != MMAY_MANAGED_OBJECT_STRING
        || d->reference_descriptor != NULL) return false;
    size_t resolved = 0, length = 0;
    if (mmay_managed_array_view_index_fast(array, index, mode, &resolved, &length)
            != MMAY_MANAGED_OK) return false;
    MarimayManagedCell cell;
    memcpy(&cell, *array->storage + resolved * sizeof(cell), sizeof(cell));
    MarimayBytecodeHotStringView view;
    if (!bind(cache, execution, cell.reference, &view)) return false;
    target->as.managed_reference = cell.reference;
    return true;
}

bool mmay_bytecode_hot_string_equal(MarimayBytecodeHotStrings *cache,
    MarimayBytecodeExecution *execution, MarimayBytecodeValue left,
    MarimayBytecodeValue right, MarimayBytecodeValue *target
) {
    MarimayBytecodeHotStringView a, b;
    if (left.type != MMAY_BYTECODE_TYPE_STRING
        || right.type != MMAY_BYTECODE_TYPE_STRING
        || target->type != MMAY_BYTECODE_TYPE_BOOL
        || !bind(cache, execution, left.as.managed_reference, &a)
        || !bind(cache, execution, right.as.managed_reference, &b)) return false;
    /* Copy each view before the next bind may evict its cache entry. Strings
     * are immutable and no allocation or safepoint can invalidate their bytes. */
    target->as.boolean = a.length == b.length
        && (a.length == 0 || memcmp(a.bytes, b.bytes, a.length) == 0);
    return true;
}
