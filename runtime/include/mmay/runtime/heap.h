#ifndef MMAY_RUNTIME_HEAP_H
#define MMAY_RUNTIME_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/core.h"

typedef struct MarimayManagedHeapOptions {
    size_t max_objects;
    /* Maximum live managed object, reserved cell, and slot-table bytes. */
    size_t max_live_bytes;
    /* Configured array-length ceiling; reaching it reports LIMIT_REACHED. */
    size_t max_array_length;
    /* Collect before every operation that may allocate or grow storage. */
    bool collect_before_allocation;
} MarimayManagedHeapOptions;

typedef enum MarimayManagedResourceFailureClass {
    MMAY_MANAGED_RESOURCE_FAILURE_NONE,
    MMAY_MANAGED_RESOURCE_FAILURE_BUDGET,
    MMAY_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION
} MarimayManagedResourceFailureClass;

typedef struct MarimayManagedResourceFailure {
    MarimayManagedResourceFailureClass classification;
    size_t live_bytes;
    size_t requested_bytes;
    size_t limit_bytes;
} MarimayManagedResourceFailure;

struct MarimayManagedSlot;
struct MarimayManagedRootFrame;
struct MarimayManagedEnumDescriptor;

/*
 * Caller-owned exact root frame. Each entry points to one live reference
 * location; scalar locations must not be included. Frames must be popped in
 * strict reverse push order and their locations must outlive the linked frame.
 * A frame is zero-initialized before its first push.
 */
typedef struct MarimayManagedRootFrame {
    MarimayManagedReference *const *locations;
    size_t location_count;
    struct MarimayManagedRootFrame *previous;
    uint64_t heap_provenance;
    bool linked;
} MarimayManagedRootFrame;

/*
 * Stack-allocatable heap owner. Slot contents are private to the runtime;
 * users must not inspect or modify these bookkeeping fields directly.
 */
typedef struct MarimayManagedHeap {
    MarimayAllocator allocator;
    struct MarimayManagedSlot *slots;
    size_t slot_count;
    size_t slot_capacity;
    size_t live_object_count;
    size_t live_byte_count;
    uint32_t free_slot;
    uint64_t provenance;
    MarimayManagedRootFrame *roots;
    const struct MarimayManagedEnumDescriptor **immediate_enum_descriptors;
    size_t immediate_enum_descriptor_count;
    size_t immediate_enum_descriptor_capacity;
    const struct MarimayManagedEnumDescriptor *immediate_enum_last_descriptor;
    uint32_t immediate_enum_last_id;
    MarimayManagedReference cached_record_reference;
    const void *cached_record_descriptor;
    void *cached_record_object;
    MarimayManagedCell *cached_record_fields;
    MarimayManagedReference cached_array_reference;
    const void *cached_array_descriptor;
    void *cached_array_object;
    size_t *cached_array_length;
    size_t *cached_array_capacity;
    uint64_t *cached_array_mutation_generation;
    unsigned char **cached_array_storage;
    MarimayManagedReference cached_array_second_reference;
    const void *cached_array_second_descriptor;
    void *cached_array_second_object;
    MarimayManagedHeapOptions options;
    MarimayManagedResourceFailure last_resource_failure;
    bool collecting;
    bool initialized;
} MarimayManagedHeap;

MarimayManagedHeapOptions mmay_managed_heap_default_options(void);
bool mmay_managed_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    MarimayManagedHeapOptions options
);
/* Invalidates all references and releases every heap-owned allocation. */
void mmay_managed_heap_dispose(MarimayManagedHeap *heap);
MarimayManagedStatus mmay_managed_root_push(
    MarimayManagedHeap *heap,
    MarimayManagedRootFrame *frame,
    MarimayManagedReference *const *locations,
    size_t location_count
);
MarimayManagedStatus mmay_managed_root_pop(
    MarimayManagedHeap *heap,
    MarimayManagedRootFrame *frame
);
/* Traces exact roots and managed object edges, then sweeps unreachable slots. */
MarimayManagedStatus mmay_managed_heap_collect(MarimayManagedHeap *heap);
/* Read-only deterministic inspection for limits, diagnostics, and tests. */
size_t mmay_managed_heap_live_objects(const MarimayManagedHeap *heap);
size_t mmay_managed_heap_live_bytes(const MarimayManagedHeap *heap);
bool mmay_managed_heap_last_resource_failure(
    const MarimayManagedHeap *heap,
    MarimayManagedResourceFailure *failure
);
MarimayManagedStatus mmay_managed_object_kind(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedObjectKind *kind
);

#endif
