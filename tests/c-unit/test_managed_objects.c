#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

static RudaManagedCell ruda_test_managed_object_f64(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static bool ruda_test_managed_object_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    bool collect_before_allocation
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 32;
    options.collect_before_allocation = collect_before_allocation;
    return ruda_managed_heap_init(heap, allocator, options);
}

static void ruda_test_managed_records_enums_and_identity(void) {
    static const RudaManagedFieldDescriptor point_fields[] = {
        {RUDA_MANAGED_CELL_RAW64, true, RUDA_MANAGED_OBJECT_INVALID, NULL},
        {RUDA_MANAGED_CELL_RAW64, false, RUDA_MANAGED_OBJECT_INVALID, NULL}
    };
    static const RudaManagedRecordDescriptor point_descriptor = {
        2,
        point_fields
    };
    static const RudaManagedFieldDescriptor payload_fields[] = {
        {RUDA_MANAGED_CELL_REFERENCE, false, RUDA_MANAGED_OBJECT_RECORD,
            &point_descriptor}
    };
    static const RudaManagedEnumVariantDescriptor variants[] = {
        {0, NULL},
        {1, payload_fields}
    };
    static const RudaManagedEnumDescriptor choice_descriptor = {
        2,
        variants
    };
    static const RudaManagedInterfaceDescriptor iface_descriptor = {1};
    RudaManagedHeap heap;
    RudaManagedReference point = ruda_managed_invalid_reference();
    RudaManagedReference alias = ruda_managed_invalid_reference();
    RudaManagedReference other = ruda_managed_invalid_reference();
    RudaManagedReference unit = ruda_managed_invalid_reference();
    RudaManagedReference wrapped = ruda_managed_invalid_reference();
    RudaManagedReference boxed = ruda_managed_invalid_reference();
    RudaManagedReference payload = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&point, &alias, &other, &unit, &wrapped, &boxed};
    RudaManagedRootFrame frame;
    RudaManagedCell fields[2];
    RudaManagedCell observed;
    RudaManagedObjectKind kind = RUDA_MANAGED_OBJECT_INVALID;
    size_t variant = 99;
    bool same_variant = false;
    const RudaManagedRecordDescriptor *record_desc = NULL;
    const RudaManagedEnumDescriptor *enum_desc = NULL;
    const RudaManagedInterfaceDescriptor *iface_desc = NULL;
    const void *table = NULL;
    memset(&frame, 0, sizeof(frame));
    memset(fields, 0, sizeof(fields));
    fields[0].f64_bits = 7;
    fields[1].f64_bits = 11;
    RUDA_EXPECT(ruda_test_managed_object_init(&heap, ruda_system_allocator(), true));
    RUDA_EXPECT(ruda_managed_record_new(
        &heap, &point_descriptor, fields, 2, &point
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(
        &heap, &frame, locations, 6
    ) == RUDA_MANAGED_OK);
    alias = point;
    RUDA_EXPECT(ruda_managed_reference_equal(point, alias));
    fields[0].f64_bits = 8;
    RUDA_EXPECT(ruda_managed_record_new(
        &heap, &point_descriptor, fields, 2, &other
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(!ruda_managed_reference_equal(point, other));
    RUDA_EXPECT(ruda_managed_record_set(
        &heap, point, 0, ruda_test_managed_object_f64(9)
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_record_get(&heap, alias, 0, &observed)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(observed.f64_bits == 9);
    RUDA_EXPECT(ruda_managed_record_set(
        &heap, point, 1, ruda_test_managed_object_f64(12)
    ) == RUDA_MANAGED_TYPE_MISMATCH);
    RUDA_EXPECT(ruda_managed_enum_new(
        &heap, &choice_descriptor, 0, NULL, 0, &unit
    ) == RUDA_MANAGED_OK);
    observed.reference = point;
    RUDA_EXPECT(ruda_managed_enum_new(
        &heap, &choice_descriptor, 1, &observed, 1, &wrapped
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_enum_is_variant(
        &heap, unit, 0, &same_variant
    ) == RUDA_MANAGED_OK && same_variant);
    RUDA_EXPECT(ruda_managed_enum_describe(
        &heap, wrapped, &enum_desc, &variant
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(enum_desc == &choice_descriptor && variant == 1);
    RUDA_EXPECT(ruda_managed_enum_get_payload(
        &heap, wrapped, 1, 0, &observed
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_reference_equal(observed.reference, point));
    RUDA_EXPECT(ruda_managed_interface_new(
        &heap, &iface_descriptor, point, &choice_descriptor, &boxed
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_interface_describe(
        &heap, boxed, &iface_desc, &payload, &table
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(iface_desc == &iface_descriptor);
    RUDA_EXPECT(ruda_managed_reference_equal(payload, point));
    RUDA_EXPECT(table == &choice_descriptor);
    RUDA_EXPECT(ruda_managed_object_kind(&heap, point, &kind) == RUDA_MANAGED_OK
        && kind == RUDA_MANAGED_OBJECT_RECORD);
    RUDA_EXPECT(ruda_managed_record_describe(&heap, point, &record_desc)
        == RUDA_MANAGED_OK && record_desc == &point_descriptor);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_record_get(&heap, alias, 0, &observed)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 0);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_recursive_descriptors(void) {
    RudaManagedFieldDescriptor option_payloads[1];
    RudaManagedEnumVariantDescriptor option_variants[2];
    RudaManagedEnumDescriptor option_descriptor;
    RudaManagedFieldDescriptor node_fields[1];
    RudaManagedRecordDescriptor node_descriptor;
    RudaManagedHeap heap;
    RudaManagedReference none = ruda_managed_invalid_reference();
    RudaManagedReference node = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&node};
    RudaManagedRootFrame frame;
    RudaManagedCell field;
    RudaManagedObjectKind kind = RUDA_MANAGED_OBJECT_INVALID;
    memset(&frame, 0, sizeof(frame));
    memset(&field, 0, sizeof(field));
    memset(option_payloads, 0, sizeof(option_payloads));
    memset(option_variants, 0, sizeof(option_variants));
    memset(&option_descriptor, 0, sizeof(option_descriptor));
    memset(node_fields, 0, sizeof(node_fields));
    memset(&node_descriptor, 0, sizeof(node_descriptor));
    option_payloads[0].cell_kind = RUDA_MANAGED_CELL_REFERENCE;
    option_payloads[0].reference_kind = RUDA_MANAGED_OBJECT_RECORD;
    option_payloads[0].reference_descriptor = &node_descriptor;
    option_variants[0].payload_count = 0;
    option_variants[1].payload_count = 1;
    option_variants[1].payloads = option_payloads;
    option_descriptor.variant_count = 2;
    option_descriptor.variants = option_variants;
    node_fields[0].cell_kind = RUDA_MANAGED_CELL_REFERENCE;
    node_fields[0].reference_kind = RUDA_MANAGED_OBJECT_ENUM;
    node_fields[0].reference_descriptor = &option_descriptor;
    node_descriptor.field_count = 1;
    node_descriptor.fields = node_fields;
    RUDA_EXPECT(ruda_test_managed_object_init(
        &heap, ruda_system_allocator(), true
    ));
    RUDA_EXPECT(ruda_managed_enum_new(
        &heap, &option_descriptor, 0, NULL, 0, &none
    ) == RUDA_MANAGED_OK);
    field.reference = none;
    RUDA_EXPECT(ruda_managed_record_new(
        &heap, &node_descriptor, &field, 1, &node
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_object_kind(&heap, none, &kind) == RUDA_MANAGED_OK
        && kind == RUDA_MANAGED_OBJECT_ENUM);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 0);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_string_and_fail_allocation(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    const unsigned char bytes[] = {'a', 'b'};
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference value = ruda_managed_invalid_reference();
        const unsigned char *observed = NULL;
        size_t length = 0;
        bool initialized = false;
        RudaManagedStatus status = RUDA_MANAGED_ALLOCATION_FAILED;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        initialized = ruda_test_managed_object_init(&heap, allocator, true);
        if (initialized) {
            status = ruda_managed_string_new(&heap, bytes, 2, &value);
            if (status == RUDA_MANAGED_OK) {
                RUDA_EXPECT(ruda_managed_string_bytes(
                    &heap, value, &observed, &length
                ) == RUDA_MANAGED_OK);
                RUDA_EXPECT(length == 2 && observed != NULL && observed[0] == 'a');
            } else {
                RUDA_EXPECT(status == RUDA_MANAGED_ALLOCATION_FAILED);
                RUDA_EXPECT(ruda_managed_reference_is_invalid(value));
            }
            ruda_managed_heap_dispose(&heap);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (initialized && status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_managed_record_fail_allocation(void) {
    static const RudaManagedFieldDescriptor fields[] = {
        {RUDA_MANAGED_CELL_REFERENCE, false, RUDA_MANAGED_OBJECT_ARRAY,
            &ruda_managed_array_f64_descriptor}
    };
    static const RudaManagedRecordDescriptor descriptor = {1, fields};
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference child = ruda_managed_invalid_reference();
        RudaManagedReference record = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&child};
        RudaManagedRootFrame frame;
        RudaManagedCell field;
        RudaManagedStatus status;
        memset(&frame, 0, sizeof(frame));
        memset(&field, 0, sizeof(field));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_test_managed_object_init(&heap, allocator, true));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &child
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
            == RUDA_MANAGED_OK);
        field.reference = child;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_managed_record_new(
            &heap, &descriptor, &field, 1, &record
        );
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        if (status == RUDA_MANAGED_ALLOCATION_FAILED) {
            RUDA_EXPECT(ruda_managed_reference_is_invalid(record));
        }
        RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_managed_enum_fail_allocation(void) {
    static const RudaManagedFieldDescriptor payloads[] = {
        {RUDA_MANAGED_CELL_REFERENCE, false, RUDA_MANAGED_OBJECT_ARRAY,
            &ruda_managed_array_f64_descriptor}
    };
    static const RudaManagedEnumVariantDescriptor variants[] = {
        {1, payloads}
    };
    static const RudaManagedEnumDescriptor descriptor = {1, variants};
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference child = ruda_managed_invalid_reference();
        RudaManagedReference value = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&child};
        RudaManagedRootFrame frame;
        RudaManagedCell payload;
        RudaManagedStatus status;
        memset(&frame, 0, sizeof(frame));
        memset(&payload, 0, sizeof(payload));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_test_managed_object_init(&heap, allocator, true));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &child
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
            == RUDA_MANAGED_OK);
        payload.reference = child;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_managed_enum_new(
            &heap, &descriptor, 0, &payload, 1, &value
        );
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        if (status == RUDA_MANAGED_ALLOCATION_FAILED) {
            RUDA_EXPECT(ruda_managed_reference_is_invalid(value));
        }
        RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_managed_interface_fail_allocation(void) {
    static const RudaManagedInterfaceDescriptor descriptor = {0};
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference child = ruda_managed_invalid_reference();
        RudaManagedReference value = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&child};
        RudaManagedRootFrame frame;
        RudaManagedStatus status;
        memset(&frame, 0, sizeof(frame));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_test_managed_object_init(&heap, allocator, true));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &child
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
            == RUDA_MANAGED_OK);
        state.failure_index = state.attempts + failure_offset;
        status = ruda_managed_interface_new(
            &heap, &descriptor, child, &descriptor, &value
        );
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        if (status == RUDA_MANAGED_ALLOCATION_FAILED) {
            RUDA_EXPECT(ruda_managed_reference_is_invalid(value));
        }
        RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_managed_objects(void) {
    ruda_test_managed_records_enums_and_identity();
    ruda_test_managed_recursive_descriptors();
    ruda_test_managed_string_and_fail_allocation();
    ruda_test_managed_record_fail_allocation();
    ruda_test_managed_enum_fail_allocation();
    ruda_test_managed_interface_fail_allocation();
}
