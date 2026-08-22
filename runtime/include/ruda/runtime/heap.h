#ifndef RUDA_RUNTIME_HEAP_H
#define RUDA_RUNTIME_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/core.h"

typedef struct RudaManagedHeapOptions {
    size_t max_objects;
    /* Maximum live managed object, reserved cell, and slot-table bytes. */
    size_t max_live_bytes;
    /* Configured array-length ceiling; reaching it reports LIMIT_REACHED. */
    size_t max_array_length;
    /* Collect before every operation that may allocate or grow storage. */
    bool collect_before_allocation;
} RudaManagedHeapOptions;

typedef enum RudaManagedResourceFailureClass {
    RUDA_MANAGED_RESOURCE_FAILURE_NONE,
    RUDA_MANAGED_RESOURCE_FAILURE_BUDGET,
    RUDA_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION
} RudaManagedResourceFailureClass;

typedef struct RudaManagedResourceFailure {
    RudaManagedResourceFailureClass classification;
    size_t live_bytes;
    size_t requested_bytes;
    size_t limit_bytes;
} RudaManagedResourceFailure;

struct RudaManagedSlot;
struct RudaManagedRootFrame;
struct RudaManagedEnumDescriptor;

/*
 * Caller-owned exact root frame. Each entry points to one live reference
 * location; scalar locations must not be included. Frames must be popped in
 * strict reverse push order and their locations must outlive the linked frame.
 * A frame is zero-initialized before its first push.
 */
typedef struct RudaManagedRootFrame {
    RudaManagedReference *const *locations;
    size_t location_count;
    struct RudaManagedRootFrame *previous;
    uint64_t heap_provenance;
    bool linked;
} RudaManagedRootFrame;

/*
 * Stack-allocatable heap owner. Slot contents are private to the runtime;
 * users must not inspect or modify these bookkeeping fields directly.
 */
typedef struct RudaManagedHeap {
    RudaAllocator allocator;
    struct RudaManagedSlot *slots;
    size_t slot_count;
    size_t slot_capacity;
    size_t live_object_count;
    size_t live_byte_count;
    uint32_t free_slot;
    uint64_t provenance;
    RudaManagedRootFrame *roots;
    const struct RudaManagedEnumDescriptor **immediate_enum_descriptors;
    size_t immediate_enum_descriptor_count;
    size_t immediate_enum_descriptor_capacity;
    const struct RudaManagedEnumDescriptor *immediate_enum_last_descriptor;
    uint32_t immediate_enum_last_id;
    RudaManagedReference cached_record_reference;
    const void *cached_record_descriptor;
    void *cached_record_object;
    RudaManagedCell *cached_record_fields;
    RudaManagedReference cached_array_reference;
    const void *cached_array_descriptor;
    void *cached_array_object;
    size_t *cached_array_length;
    size_t *cached_array_capacity;
    uint64_t *cached_array_mutation_generation;
    unsigned char **cached_array_storage;
    RudaManagedReference cached_array_second_reference;
    const void *cached_array_second_descriptor;
    void *cached_array_second_object;
    RudaManagedHeapOptions options;
    RudaManagedResourceFailure last_resource_failure;
    bool collecting;
    bool initialized;
} RudaManagedHeap;

RudaManagedHeapOptions ruda_managed_heap_default_options(void);
bool ruda_managed_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    RudaManagedHeapOptions options
);
/* Invalidates all references and releases every heap-owned allocation. */
void ruda_managed_heap_dispose(RudaManagedHeap *heap);
RudaManagedStatus ruda_managed_root_push(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame,
    RudaManagedReference *const *locations,
    size_t location_count
);
RudaManagedStatus ruda_managed_root_pop(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame
);
/* Traces exact roots and managed object edges, then sweeps unreachable slots. */
RudaManagedStatus ruda_managed_heap_collect(RudaManagedHeap *heap);
/* Read-only deterministic inspection for limits, diagnostics, and tests. */
size_t ruda_managed_heap_live_objects(const RudaManagedHeap *heap);
size_t ruda_managed_heap_live_bytes(const RudaManagedHeap *heap);
bool ruda_managed_heap_last_resource_failure(
    const RudaManagedHeap *heap,
    RudaManagedResourceFailure *failure
);
RudaManagedStatus ruda_managed_object_kind(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    RudaManagedObjectKind *kind
);

#endif
