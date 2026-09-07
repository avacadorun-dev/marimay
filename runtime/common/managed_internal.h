#ifndef MMAY_MANAGED_INTERNAL_H
#define MMAY_MANAGED_INTERNAL_H

#include "mmay/runtime/managed.h"
#include "managed_reference_internal.h"

void mmay_managed_view_cache_clear(MarimayManagedHeap *heap);
bool mmay_managed_immediate_enum_decode(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedEnumDescriptor **descriptor,
    size_t *variant,
    uint64_t *payload_bits
);
bool mmay_managed_reference_valid(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference
);
size_t mmay_managed_language_max_array_length(void);
bool mmay_managed_array_descriptor_valid(
    const MarimayManagedArrayDescriptor *descriptor
);
size_t mmay_managed_array_storage_alignment(
    const MarimayManagedArrayDescriptor *descriptor
);
bool mmay_managed_array_storage_bytes_for_capacity(
    const MarimayManagedArrayDescriptor *descriptor,
    size_t capacity,
    size_t *bytes
);
size_t mmay_managed_array_storage_bytes(const MarimayManagedArray *array);
size_t mmay_managed_array_child_cache_bytes(const MarimayManagedArray *array);
void mmay_managed_array_dispose(MarimayManagedHeap *heap, MarimayManagedArray *array);
MarimayManagedStatus mmay_managed_array_storage_load(
    const MarimayManagedArray *array,
    size_t index,
    MarimayManagedCell *result
);
MarimayManagedStatus mmay_managed_array_storage_store(
    MarimayManagedArray *array,
    size_t index,
    MarimayManagedCell value
);
MarimayManagedStatus mmay_managed_array_storage_clear(
    MarimayManagedArray *array,
    size_t start,
    size_t count
);
MarimayManagedStatus mmay_managed_array_index(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    int64_t index,
    const MarimayManagedArray **array,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_resolve_mutable(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedArray **array
);
MarimayManagedStatus mmay_managed_array_validate_value(
    const MarimayManagedHeap *heap,
    const MarimayManagedArray *array,
    MarimayManagedCell value
);
MarimayManagedStatus mmay_managed_array_check_mutation_commit(
    const MarimayManagedArray *array
);
void mmay_managed_array_commit_mutation(MarimayManagedArray *array);
bool mmay_managed_array_plan_capacity(
    size_t current_capacity,
    size_t minimum_capacity,
    size_t maximum_capacity,
    size_t *planned_capacity
);
MarimayManagedStatus mmay_managed_array_ensure_capacity(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    size_t minimum_capacity,
    const MarimayManagedCell *temporary_value,
    MarimayManagedArray **result
);
MarimayManagedStatus mmay_managed_reference_status(const MarimayManagedHeap *heap, MarimayManagedReference reference, const MarimayManagedArrayDescriptor *expected);
size_t mmay_managed_slot_object_bytes(const MarimayManagedSlot *slot);
void mmay_managed_slot_dispose_object(MarimayManagedHeap *heap, MarimayManagedSlot *slot);
MarimayManagedStatus mmay_managed_mark_reference(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    uint32_t *gray_head
);
MarimayManagedStatus mmay_managed_trace_slot(MarimayManagedHeap *heap, MarimayManagedSlot *slot, uint32_t *gray_head);
MarimayManagedStatus mmay_managed_collect_internal(MarimayManagedHeap *heap, const MarimayManagedReference *temporary_roots, size_t temporary_root_count, const MarimayManagedCell *temporary_cells, size_t temporary_cell_count);
bool mmay_managed_add_within_limit(size_t current, size_t amount, size_t limit, size_t *result);
void mmay_managed_resource_failure_clear(MarimayManagedHeap *heap);
MarimayManagedStatus mmay_managed_resource_budget_failure(
    MarimayManagedHeap *heap, size_t requested_bytes);
MarimayManagedStatus mmay_managed_resource_host_failure(
    MarimayManagedHeap *heap, size_t requested_bytes);
bool mmay_managed_new_slot_capacity(const MarimayManagedHeap *heap, size_t *capacity, size_t *added_bytes);
MarimayManagedStatus mmay_managed_publish_object(MarimayManagedHeap *heap, MarimayManagedObjectKind kind, void *object, size_t object_bytes, const MarimayManagedReference *temporary_roots, size_t temporary_root_count, const MarimayManagedCell *temporary_cells, size_t temporary_cell_count, MarimayManagedReference *result);
size_t mmay_managed_count_reference_fields(const MarimayManagedFieldDescriptor *fields, size_t count);
bool mmay_managed_collect_reference_roots(const MarimayManagedFieldDescriptor *fields, const MarimayManagedCell *cells, size_t count, MarimayManagedReference *roots, size_t root_capacity, size_t *root_count);
MarimayManagedStatus mmay_managed_validate_field(const MarimayManagedHeap *heap, const MarimayManagedFieldDescriptor *field, MarimayManagedCell value);
void mmay_managed_string_dispose(MarimayManagedHeap *heap, MarimayManagedString *value);
void mmay_managed_record_dispose(MarimayManagedHeap *heap, MarimayManagedRecord *record);
void mmay_managed_enum_dispose(MarimayManagedHeap *heap, MarimayManagedEnum *value);
void mmay_managed_interface_dispose(MarimayManagedHeap *heap, MarimayManagedInterface *value);
size_t mmay_managed_map_storage_bytes(const MarimayManagedMap *map);
bool mmay_managed_map_descriptor_valid(
    const MarimayManagedMapDescriptor *descriptor
);
void mmay_managed_map_dispose(MarimayManagedHeap *heap, MarimayManagedMap *map);
MarimayManagedStatus mmay_managed_map_trace(
    MarimayManagedHeap *heap,
    const MarimayManagedMap *map,
    uint32_t *gray_head
);

#endif
