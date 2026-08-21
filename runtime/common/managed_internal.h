#ifndef RUDA_MANAGED_INTERNAL_H
#define RUDA_MANAGED_INTERNAL_H

#include "ruda/runtime/managed.h"

typedef struct RudaManagedArray {
    const RudaManagedArrayDescriptor *descriptor;
    size_t length;
    size_t capacity;
    uint64_t mutation_generation;
    RudaManagedCell *cells;
} RudaManagedArray;
typedef struct RudaManagedString {
    size_t length;
    unsigned char *bytes;
} RudaManagedString;
typedef struct RudaManagedRecord {
    const RudaManagedRecordDescriptor *descriptor;
    RudaManagedCell *fields;
} RudaManagedRecord;
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

bool ruda_managed_heap_valid(const RudaManagedHeap *heap);
size_t ruda_managed_language_max_array_length(void);
size_t ruda_managed_array_cell_bytes(const RudaManagedArray *array);
void ruda_managed_array_dispose(RudaManagedHeap *heap, RudaManagedArray *array);
RudaManagedStatus ruda_managed_array_index(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    int64_t index,
    const RudaManagedArray **array,
    size_t *current_length
);
RudaManagedSlot *ruda_managed_resolve(RudaManagedHeap *heap, RudaManagedReference reference);
const RudaManagedSlot *ruda_managed_resolve_const(const RudaManagedHeap *heap, RudaManagedReference reference);
RudaManagedStatus ruda_managed_reference_status(const RudaManagedHeap *heap, RudaManagedReference reference, const RudaManagedArrayDescriptor *expected);
bool ruda_managed_slot_occupied(const RudaManagedSlot *slot);
size_t ruda_managed_slot_object_bytes(const RudaManagedSlot *slot);
void ruda_managed_slot_dispose_object(RudaManagedHeap *heap, RudaManagedSlot *slot);
RudaManagedStatus ruda_managed_mark_reference(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    uint32_t *gray_head
);
RudaManagedStatus ruda_managed_trace_slot(RudaManagedHeap *heap, RudaManagedSlot *slot, uint32_t *gray_head);
RudaManagedStatus ruda_managed_collect_internal(RudaManagedHeap *heap, const RudaManagedReference *temporary_roots, size_t temporary_root_count, const RudaManagedCell *temporary_cells, size_t temporary_cell_count);
bool ruda_managed_add_within_limit(size_t current, size_t amount, size_t limit, size_t *result);
void ruda_managed_resource_failure_clear(RudaManagedHeap *heap);
RudaManagedStatus ruda_managed_resource_budget_failure(
    RudaManagedHeap *heap, size_t requested_bytes);
RudaManagedStatus ruda_managed_resource_host_failure(
    RudaManagedHeap *heap, size_t requested_bytes);
bool ruda_managed_new_slot_capacity(const RudaManagedHeap *heap, size_t *capacity, size_t *added_bytes);
RudaManagedStatus ruda_managed_publish_object(RudaManagedHeap *heap, RudaManagedObjectKind kind, void *object, size_t object_bytes, const RudaManagedReference *temporary_roots, size_t temporary_root_count, const RudaManagedCell *temporary_cells, size_t temporary_cell_count, RudaManagedReference *result);
size_t ruda_managed_count_reference_fields(const RudaManagedFieldDescriptor *fields, size_t count);
bool ruda_managed_collect_reference_roots(const RudaManagedFieldDescriptor *fields, const RudaManagedCell *cells, size_t count, RudaManagedReference *roots, size_t root_capacity, size_t *root_count);
RudaManagedStatus ruda_managed_validate_field(const RudaManagedHeap *heap, const RudaManagedFieldDescriptor *field, RudaManagedCell value);
void ruda_managed_string_dispose(RudaManagedHeap *heap, RudaManagedString *value);
void ruda_managed_record_dispose(RudaManagedHeap *heap, RudaManagedRecord *record);
void ruda_managed_enum_dispose(RudaManagedHeap *heap, RudaManagedEnum *value);
void ruda_managed_interface_dispose(RudaManagedHeap *heap, RudaManagedInterface *value);

#endif
