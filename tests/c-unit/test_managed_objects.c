#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

static MarimayManagedCell mmay_test_managed_object_f64(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static bool mmay_test_managed_object_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    bool collect_before_allocation
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 32;
    options.collect_before_allocation = collect_before_allocation;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_managed_records_enums_and_identity(void) {
    static const MarimayManagedFieldDescriptor point_fields[] = {
        {MMAY_MANAGED_CELL_RAW64, true, MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL}
    };
    static const MarimayManagedRecordDescriptor point_descriptor = {
        2,
        point_fields
    };
    static const MarimayManagedFieldDescriptor payload_fields[] = {
        {MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_RECORD,
            &point_descriptor}
    };
    static const MarimayManagedEnumVariantDescriptor variants[] = {
        {0, NULL},
        {1, payload_fields}
    };
    static const MarimayManagedEnumDescriptor choice_descriptor = {
        2,
        variants
    };
    static const MarimayManagedInterfaceDescriptor iface_descriptor = {1};
    MarimayManagedHeap heap;
    MarimayManagedReference point = mmay_managed_invalid_reference();
    MarimayManagedReference alias = mmay_managed_invalid_reference();
    MarimayManagedReference other = mmay_managed_invalid_reference();
    MarimayManagedReference unit = mmay_managed_invalid_reference();
    MarimayManagedReference wrapped = mmay_managed_invalid_reference();
    MarimayManagedReference boxed = mmay_managed_invalid_reference();
    MarimayManagedReference payload = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&point, &alias, &other, &unit, &wrapped, &boxed};
    MarimayManagedRootFrame frame;
    MarimayManagedCell fields[2];
    MarimayManagedCell observed;
    MarimayManagedObjectKind kind = MMAY_MANAGED_OBJECT_INVALID;
    size_t variant = 99;
    bool same_variant = false;
    const MarimayManagedRecordDescriptor *record_desc = NULL;
    const MarimayManagedEnumDescriptor *enum_desc = NULL;
    const MarimayManagedInterfaceDescriptor *iface_desc = NULL;
    const void *table = NULL;
    memset(&frame, 0, sizeof(frame));
    memset(fields, 0, sizeof(fields));
    fields[0].f64_bits = 7;
    fields[1].f64_bits = 11;
    MMAY_EXPECT(mmay_test_managed_object_init(&heap, mmay_system_allocator(), true));
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &point_descriptor, fields, 2, &point
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &frame, locations, 6
    ) == MMAY_MANAGED_OK);
    alias = point;
    MMAY_EXPECT(mmay_managed_reference_equal(point, alias));
    fields[0].f64_bits = 8;
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &point_descriptor, fields, 2, &other
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(!mmay_managed_reference_equal(point, other));
    MMAY_EXPECT(mmay_managed_record_set(
        &heap, point, 0, mmay_test_managed_object_f64(9)
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_get(&heap, alias, 0, &observed)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(observed.f64_bits == 9);
    MMAY_EXPECT(mmay_managed_record_set(
        &heap, point, 1, mmay_test_managed_object_f64(12)
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_managed_enum_new(
        &heap, &choice_descriptor, 0, NULL, 0, &unit
    ) == MMAY_MANAGED_OK);
    observed.reference = point;
    MMAY_EXPECT(mmay_managed_enum_new(
        &heap, &choice_descriptor, 1, &observed, 1, &wrapped
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_enum_is_variant(
        &heap, unit, 0, &same_variant
    ) == MMAY_MANAGED_OK && same_variant);
    MMAY_EXPECT(mmay_managed_enum_describe(
        &heap, wrapped, &enum_desc, &variant
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(enum_desc == &choice_descriptor && variant == 1);
    MMAY_EXPECT(mmay_managed_enum_get_payload(
        &heap, wrapped, 1, 0, &observed
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_reference_equal(observed.reference, point));
    MMAY_EXPECT(mmay_managed_interface_new(
        &heap, &iface_descriptor, point, &choice_descriptor, &boxed
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_interface_describe(
        &heap, boxed, &iface_desc, &payload, &table
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(iface_desc == &iface_descriptor);
    MMAY_EXPECT(mmay_managed_reference_equal(payload, point));
    MMAY_EXPECT(table == &choice_descriptor);
    MMAY_EXPECT(mmay_managed_object_kind(&heap, point, &kind) == MMAY_MANAGED_OK
        && kind == MMAY_MANAGED_OBJECT_RECORD);
    MMAY_EXPECT(mmay_managed_record_describe(&heap, point, &record_desc)
        == MMAY_MANAGED_OK && record_desc == &point_descriptor);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_get(&heap, alias, 0, &observed)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_recursive_descriptors(void) {
    MarimayManagedFieldDescriptor option_payloads[1];
    MarimayManagedEnumVariantDescriptor option_variants[2];
    MarimayManagedEnumDescriptor option_descriptor;
    MarimayManagedFieldDescriptor node_fields[1];
    MarimayManagedRecordDescriptor node_descriptor;
    MarimayManagedHeap heap;
    MarimayManagedReference none = mmay_managed_invalid_reference();
    MarimayManagedReference node = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&node};
    MarimayManagedRootFrame frame;
    MarimayManagedCell field;
    MarimayManagedObjectKind kind = MMAY_MANAGED_OBJECT_INVALID;
    memset(&frame, 0, sizeof(frame));
    memset(&field, 0, sizeof(field));
    memset(option_payloads, 0, sizeof(option_payloads));
    memset(option_variants, 0, sizeof(option_variants));
    memset(&option_descriptor, 0, sizeof(option_descriptor));
    memset(node_fields, 0, sizeof(node_fields));
    memset(&node_descriptor, 0, sizeof(node_descriptor));
    option_payloads[0].cell_kind = MMAY_MANAGED_CELL_REFERENCE;
    option_payloads[0].reference_kind = MMAY_MANAGED_OBJECT_RECORD;
    option_payloads[0].reference_descriptor = &node_descriptor;
    option_variants[0].payload_count = 0;
    option_variants[1].payload_count = 1;
    option_variants[1].payloads = option_payloads;
    option_descriptor.variant_count = 2;
    option_descriptor.variants = option_variants;
    node_fields[0].cell_kind = MMAY_MANAGED_CELL_REFERENCE;
    node_fields[0].reference_kind = MMAY_MANAGED_OBJECT_ENUM;
    node_fields[0].reference_descriptor = &option_descriptor;
    node_descriptor.field_count = 1;
    node_descriptor.fields = node_fields;
    MMAY_EXPECT(mmay_test_managed_object_init(
        &heap, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_managed_enum_new(
        &heap, &option_descriptor, 0, NULL, 0, &none
    ) == MMAY_MANAGED_OK);
    field.reference = none;
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &node_descriptor, &field, 1, &node
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_object_kind(&heap, none, &kind) == MMAY_MANAGED_OK
        && kind == MMAY_MANAGED_OBJECT_ENUM);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_string_and_fail_allocation(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    const unsigned char bytes[] = {'a', 'b'};
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference value = mmay_managed_invalid_reference();
        const unsigned char *observed = NULL;
        size_t length = 0;
        bool initialized = false;
        MarimayManagedStatus status = MMAY_MANAGED_ALLOCATION_FAILED;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        initialized = mmay_test_managed_object_init(&heap, allocator, true);
        if (initialized) {
            status = mmay_managed_string_new(&heap, bytes, 2, &value);
            if (status == MMAY_MANAGED_OK) {
                MMAY_EXPECT(mmay_managed_string_bytes(
                    &heap, value, &observed, &length
                ) == MMAY_MANAGED_OK);
                MMAY_EXPECT(length == 2 && observed != NULL && observed[0] == 'a');
            } else {
                MMAY_EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED);
                MMAY_EXPECT(mmay_managed_reference_is_invalid(value));
            }
            mmay_managed_heap_dispose(&heap);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (initialized && status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_managed_record_fail_allocation(void) {
    static const MarimayManagedFieldDescriptor fields[] = {
        {MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_ARRAY,
            &mmay_managed_array_f64_descriptor}
    };
    static const MarimayManagedRecordDescriptor descriptor = {1, fields};
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference child = mmay_managed_invalid_reference();
        MarimayManagedReference record = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&child};
        MarimayManagedRootFrame frame;
        MarimayManagedCell field;
        MarimayManagedStatus status;
        memset(&frame, 0, sizeof(frame));
        memset(&field, 0, sizeof(field));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_managed_object_init(&heap, allocator, true));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &child
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
            == MMAY_MANAGED_OK);
        field.reference = child;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_record_new(
            &heap, &descriptor, &field, 1, &record
        );
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        if (status == MMAY_MANAGED_ALLOCATION_FAILED) {
            MMAY_EXPECT(mmay_managed_reference_is_invalid(record));
        }
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_managed_enum_fail_allocation(void) {
    static const MarimayManagedFieldDescriptor payloads[] = {
        {MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_ARRAY,
            &mmay_managed_array_f64_descriptor}
    };
    static const MarimayManagedEnumVariantDescriptor variants[] = {
        {1, payloads}
    };
    static const MarimayManagedEnumDescriptor descriptor = {1, variants};
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference child = mmay_managed_invalid_reference();
        MarimayManagedReference value = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&child};
        MarimayManagedRootFrame frame;
        MarimayManagedCell payload;
        MarimayManagedStatus status;
        memset(&frame, 0, sizeof(frame));
        memset(&payload, 0, sizeof(payload));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_managed_object_init(&heap, allocator, true));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &child
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
            == MMAY_MANAGED_OK);
        payload.reference = child;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_enum_new(
            &heap, &descriptor, 0, &payload, 1, &value
        );
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        if (status == MMAY_MANAGED_ALLOCATION_FAILED) {
            MMAY_EXPECT(mmay_managed_reference_is_invalid(value));
        }
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_managed_interface_fail_allocation(void) {
    static const MarimayManagedInterfaceDescriptor descriptor = {0};
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 16; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference child = mmay_managed_invalid_reference();
        MarimayManagedReference value = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&child};
        MarimayManagedRootFrame frame;
        MarimayManagedStatus status;
        memset(&frame, 0, sizeof(frame));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_managed_object_init(&heap, allocator, true));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &child
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
            == MMAY_MANAGED_OK);
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_interface_new(
            &heap, &descriptor, child, &descriptor, &value
        );
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        if (status == MMAY_MANAGED_ALLOCATION_FAILED) {
            MMAY_EXPECT(mmay_managed_reference_is_invalid(value));
        }
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_managed_objects(void) {
    mmay_test_managed_records_enums_and_identity();
    mmay_test_managed_recursive_descriptors();
    mmay_test_managed_string_and_fail_allocation();
    mmay_test_managed_record_fail_allocation();
    mmay_test_managed_enum_fail_allocation();
    mmay_test_managed_interface_fail_allocation();
}
