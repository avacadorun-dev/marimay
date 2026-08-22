#ifndef RUDA_RUNTIME_GENERATED_SLOTS_H
#define RUDA_RUNTIME_GENERATED_SLOTS_H

#include "ruda/runtime/enum.h"
#include "ruda/runtime/generated_record.h"
#include "ruda/runtime/interface.h"
#include "ruda/runtime/string.h"

/* Exact managed storage layouts reserved for compiler-generated C. */
typedef struct RudaManagedString {
    size_t length;
    unsigned char *bytes;
} RudaManagedString;

typedef struct RudaManagedEnum {
    const RudaManagedEnumDescriptor *descriptor;
    size_t variant;
    RudaManagedCell *payload;
} RudaManagedEnum;

typedef struct RudaManagedInterface {
    const RudaManagedInterfaceDescriptor *descriptor;
    RudaManagedReference payload;
    const void *method_table;
} RudaManagedInterface;

typedef struct RudaManagedSlot {
    RudaManagedObjectKind kind;
    union {
        RudaManagedArray *array;
        RudaManagedString *string;
        RudaManagedRecord *record;
        RudaManagedEnum *enumeration;
        RudaManagedInterface *iface;
    } as;
    uint32_t generation;
    uint32_t next_free;
    uint32_t next_gray;
    bool marked;
    bool retired;
} RudaManagedSlot;

static inline RudaManagedArray *ruda_managed_array_generated_child_unchecked(
    RudaManagedHeap *heap,
    RudaManagedReference reference
) {
    return heap->slots[reference.slot].as.array;
}

static inline RudaManagedRecord *ruda_managed_record_generated_child_unchecked(
    RudaManagedHeap *heap, RudaManagedReference reference) {
    return heap->slots[reference.slot].as.record;
}

static inline RudaManagedReference ruda_managed_invalid_reference_generated(
    void
) {
    RudaManagedReference reference;
    reference.heap_provenance = UINT64_MAX;
    reference.slot = UINT32_MAX;
    reference.generation = UINT32_MAX;
    return reference;
}

static inline RudaManagedSlot *ruda_managed_generated_resolve(
    RudaManagedHeap *heap,
    RudaManagedReference reference
) {
    RudaManagedSlot *slot = NULL;
    if (heap == NULL || !heap->initialized || heap->provenance == 0
        || heap->provenance == UINT64_MAX
        || reference.heap_provenance != heap->provenance
        || reference.slot == UINT32_MAX
        || (size_t)reference.slot >= heap->slot_count) {
        return NULL;
    }
    slot = &heap->slots[reference.slot];
    return slot->kind != RUDA_MANAGED_OBJECT_INVALID
            && slot->generation == reference.generation
        ? slot : NULL;
}

static inline RudaManagedStatus ruda_managed_array_generated_bind(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    RudaManagedArrayGeneratedView *view
) {
    RudaManagedSlot *slot = NULL;
    if (view == NULL || expected == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    view->object = NULL;
    if (heap != NULL && heap->cached_array_descriptor == expected
        && heap->cached_array_object != NULL
        && ruda_managed_generated_reference_equal(
            heap->cached_array_reference, reference
        )) {
        view->object = (RudaManagedArray *)heap->cached_array_object;
        return RUDA_MANAGED_OK;
    }
    if (heap != NULL && heap->cached_array_second_descriptor == expected
        && heap->cached_array_second_object != NULL
        && ruda_managed_generated_reference_equal(
            heap->cached_array_second_reference, reference
        )) {
        view->object = (RudaManagedArray *)heap->cached_array_second_object;
        return RUDA_MANAGED_OK;
    }
    slot = ruda_managed_generated_resolve(heap, reference);
    if (slot == NULL) return RUDA_MANAGED_INVALID_REFERENCE;
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL
        || slot->as.array->descriptor != expected) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    view->object = slot->as.array;
    heap->cached_array_second_reference = heap->cached_array_reference;
    heap->cached_array_second_descriptor = heap->cached_array_descriptor;
    heap->cached_array_second_object = heap->cached_array_object;
    heap->cached_array_reference = reference;
    heap->cached_array_descriptor = expected;
    heap->cached_array_object = view->object;
    return RUDA_MANAGED_OK;
}

static inline RudaManagedStatus ruda_managed_record_generated_bind(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedRecordDescriptor *expected,
    RudaManagedRecordGeneratedView *view
) {
    RudaManagedSlot *slot = NULL;
    if (view == NULL || expected == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    view->reference = reference;
    view->object = NULL;
    if (heap != NULL && heap->cached_record_descriptor == expected
        && heap->cached_record_object != NULL
        && ruda_managed_generated_reference_equal(
            heap->cached_record_reference, reference
        )) {
        view->object = (RudaManagedRecord *)heap->cached_record_object;
        return RUDA_MANAGED_OK;
    }
    slot = ruda_managed_generated_resolve(heap, reference);
    if (slot == NULL) return RUDA_MANAGED_INVALID_REFERENCE;
    if (slot->kind != RUDA_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor != expected) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    view->object = slot->as.record;
    heap->cached_record_reference = reference;
    heap->cached_record_descriptor = expected;
    heap->cached_record_object = view->object;
    return RUDA_MANAGED_OK;
}

#endif
