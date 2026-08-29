#ifndef MMAY_RUNTIME_GENERATED_SLOTS_H
#define MMAY_RUNTIME_GENERATED_SLOTS_H

#include "mmay/runtime/enum.h"
#include "mmay/runtime/generated_record.h"
#include "mmay/runtime/interface.h"
#include "mmay/runtime/generated_map.h"
#include "mmay/runtime/string.h"

/* Exact managed storage layouts reserved for compiler-generated C. */
typedef struct MarimayManagedString {
    size_t length;
    unsigned char *bytes;
} MarimayManagedString;

typedef struct MarimayManagedEnum {
    const MarimayManagedEnumDescriptor *descriptor;
    size_t variant;
    MarimayManagedCell *payload;
} MarimayManagedEnum;

typedef struct MarimayManagedInterface {
    const MarimayManagedInterfaceDescriptor *descriptor;
    MarimayManagedReference payload;
    const void *method_table;
} MarimayManagedInterface;

typedef struct MarimayManagedSlot {
    MarimayManagedObjectKind kind;
    union {
        MarimayManagedArray *array;
        MarimayManagedString *string;
        MarimayManagedRecord *record;
        MarimayManagedEnum *enumeration;
        MarimayManagedInterface *iface;
        MarimayManagedMap *map;
    } as;
    uint32_t generation;
    uint32_t next_free;
    uint32_t next_gray;
    bool marked;
    bool retired;
} MarimayManagedSlot;

static inline MarimayManagedArray *mmay_managed_array_generated_child_unchecked(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    return heap->slots[reference.slot].as.array;
}

static inline MarimayManagedRecord *mmay_managed_record_generated_child_unchecked(
    MarimayManagedHeap *heap, MarimayManagedReference reference) {
    return heap->slots[reference.slot].as.record;
}

static inline MarimayManagedReference mmay_managed_invalid_reference_generated(
    void
) {
    MarimayManagedReference reference;
    reference.heap_provenance = UINT64_MAX;
    reference.slot = UINT32_MAX;
    reference.generation = UINT32_MAX;
    return reference;
}

static inline MarimayManagedSlot *mmay_managed_generated_resolve(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    MarimayManagedSlot *slot = NULL;
    if (heap == NULL || !heap->initialized || heap->provenance == 0
        || heap->provenance == UINT64_MAX
        || reference.heap_provenance != heap->provenance
        || reference.slot == UINT32_MAX
        || (size_t)reference.slot >= heap->slot_count) {
        return NULL;
    }
    slot = &heap->slots[reference.slot];
    return slot->kind != MMAY_MANAGED_OBJECT_INVALID
            && slot->generation == reference.generation
        ? slot : NULL;
}

static inline MarimayManagedStatus mmay_managed_array_generated_bind(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    MarimayManagedArrayGeneratedView *view
) {
    MarimayManagedSlot *slot = NULL;
    if (view == NULL || expected == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    view->object = NULL;
    if (heap != NULL && heap->cached_array_descriptor == expected
        && heap->cached_array_object != NULL
        && mmay_managed_generated_reference_equal(
            heap->cached_array_reference, reference
        )) {
        view->object = (MarimayManagedArray *)heap->cached_array_object;
        return MMAY_MANAGED_OK;
    }
    if (heap != NULL && heap->cached_array_second_descriptor == expected
        && heap->cached_array_second_object != NULL
        && mmay_managed_generated_reference_equal(
            heap->cached_array_second_reference, reference
        )) {
        view->object = (MarimayManagedArray *)heap->cached_array_second_object;
        return MMAY_MANAGED_OK;
    }
    slot = mmay_managed_generated_resolve(heap, reference);
    if (slot == NULL) return MMAY_MANAGED_INVALID_REFERENCE;
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL
        || slot->as.array->descriptor != expected) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    view->object = slot->as.array;
    heap->cached_array_second_reference = heap->cached_array_reference;
    heap->cached_array_second_descriptor = heap->cached_array_descriptor;
    heap->cached_array_second_object = heap->cached_array_object;
    heap->cached_array_reference = reference;
    heap->cached_array_descriptor = expected;
    heap->cached_array_object = view->object;
    return MMAY_MANAGED_OK;
}

static inline MarimayManagedStatus mmay_managed_record_generated_bind(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedRecordDescriptor *expected,
    MarimayManagedRecordGeneratedView *view
) {
    MarimayManagedSlot *slot = NULL;
    if (view == NULL || expected == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    view->reference = reference;
    view->object = NULL;
    if (heap != NULL && heap->cached_record_descriptor == expected
        && heap->cached_record_object != NULL
        && mmay_managed_generated_reference_equal(
            heap->cached_record_reference, reference
        )) {
        view->object = (MarimayManagedRecord *)heap->cached_record_object;
        return MMAY_MANAGED_OK;
    }
    slot = mmay_managed_generated_resolve(heap, reference);
    if (slot == NULL) return MMAY_MANAGED_INVALID_REFERENCE;
    if (slot->kind != MMAY_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor != expected) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    view->object = slot->as.record;
    heap->cached_record_reference = reference;
    heap->cached_record_descriptor = expected;
    heap->cached_record_object = view->object;
    return MMAY_MANAGED_OK;
}
#endif
