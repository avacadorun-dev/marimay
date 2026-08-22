#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

static const MarimayManagedArrayDescriptor mmay_test_nested_rows = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_managed_array_f64_descriptor
};

static const MarimayManagedArrayDescriptor mmay_test_nested_planes = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_test_nested_rows
};

static MarimayManagedCell mmay_test_nested_raw(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedCell mmay_test_nested_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static void mmay_test_nested_ragged_gc_and_mutation(void) {
    MarimayManagedHeap heap;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference row0 = mmay_managed_invalid_reference();
    MarimayManagedReference row1 = mmay_managed_invalid_reference();
    MarimayManagedReference replacement = mmay_managed_invalid_reference();
    MarimayManagedReference rows = mmay_managed_invalid_reference();
    MarimayManagedReference plane = mmay_managed_invalid_reference();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&plane};
    MarimayManagedRootFrame roots;
    MarimayManagedCell row0_values[] = {
        mmay_test_nested_raw(10), mmay_test_nested_raw(11)
    };
    MarimayManagedCell replacement_values[] = {
        mmay_test_nested_raw(90), mmay_test_nested_raw(91),
        mmay_test_nested_raw(92)
    };
    MarimayManagedCell rows_values[2];
    MarimayManagedCell plane_value;
    MarimayManagedCell observed;
    size_t length = 0;
    memset(&roots, 0, sizeof(roots));
    memset(&observed, 0, sizeof(observed));
    options.max_objects = 64;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 64;
    options.collect_before_allocation = false;
    MMAY_EXPECT(mmay_managed_heap_init(
        &heap, mmay_system_allocator(), options
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor,
        row0_values, 2, &row0
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &row1
    ) == MMAY_MANAGED_OK);
    rows_values[0] = mmay_test_nested_reference(row0);
    rows_values[1] = mmay_test_nested_reference(row1);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_nested_rows, rows_values, 2, &rows
    ) == MMAY_MANAGED_OK);
    plane_value = mmay_test_nested_reference(rows);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_nested_planes, &plane_value, 1, &plane
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 1)
        == MMAY_MANAGED_OK);
    row0 = mmay_managed_invalid_reference();
    row1 = mmay_managed_invalid_reference();
    rows = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);

    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, plane, &mmay_test_nested_planes, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && length == 1);
    rows = observed.reference;
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, rows, &mmay_test_nested_rows, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && length == 2);
    row0 = observed.reference;
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, row0, &mmay_managed_array_f64_descriptor, 1,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == 11 && length == 2);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, rows, &mmay_test_nested_rows, 1,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(
        &heap, observed.reference, &length
    ) == MMAY_MANAGED_OK && length == 0);

    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor,
        replacement_values, 3, &replacement
    ) == MMAY_MANAGED_OK);
    stale = row0;
    MMAY_EXPECT(mmay_managed_array_access_set(
        &heap, rows, &mmay_test_nested_rows, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        mmay_test_nested_reference(replacement), &length
    ) == MMAY_MANAGED_OK);
    replacement = mmay_managed_invalid_reference();
    row0 = mmay_managed_invalid_reference();
    rows = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, stale, &length)
        == MMAY_MANAGED_INVALID_REFERENCE);

    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, plane, &mmay_test_nested_planes, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK);
    rows = observed.reference;
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, rows, &mmay_test_nested_rows, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, observed.reference, &mmay_managed_array_f64_descriptor, 2,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == 92);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, plane, &mmay_test_nested_planes, 1,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, plane, &mmay_test_nested_rows, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_nested_generated_cache_failure_and_refresh(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 4 && !reached_success;
        failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
        MarimayManagedReference row0 = mmay_managed_invalid_reference();
        MarimayManagedReference row1 = mmay_managed_invalid_reference();
        MarimayManagedReference replacement = mmay_managed_invalid_reference();
        MarimayManagedReference rows = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {
            &row0, &row1, &replacement, &rows
        };
        MarimayManagedRootFrame roots;
        MarimayManagedArrayGeneratedView rows_view;
        MarimayManagedArrayGeneratedView row0_view;
        MarimayManagedArrayGeneratedView replacement_view;
        MarimayManagedCell row0_values[] = {
            mmay_test_nested_raw(10), mmay_test_nested_raw(11)
        };
        MarimayManagedCell row1_value = mmay_test_nested_raw(20);
        MarimayManagedCell replacement_values[] = {
            mmay_test_nested_raw(90), mmay_test_nested_raw(91),
            mmay_test_nested_raw(92)
        };
        MarimayManagedCell rows_values[2];
        MarimayManagedStatus status;
        size_t before_bytes = 0;
        size_t length = 0;
        memset(&roots, 0, sizeof(roots));
        memset(&rows_view, 0, sizeof(rows_view));
        memset(&row0_view, 0, sizeof(row0_view));
        memset(&replacement_view, 0, sizeof(replacement_view));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        options.max_objects = 32;
        options.max_live_bytes = 64U * 1024U;
        options.max_array_length = 32;
        options.collect_before_allocation = false;
        MMAY_EXPECT(mmay_managed_heap_init(
            &heap, mmay_fail_allocator(&state), options
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor,
            row0_values, 2, &row0
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor,
            &row1_value, 1, &row1
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor,
            replacement_values, 3, &replacement
        ) == MMAY_MANAGED_OK);
        rows_values[0] = mmay_test_nested_reference(row0);
        rows_values[1] = mmay_test_nested_reference(row1);
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_test_nested_rows, rows_values, 2, &rows
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(
            &heap, &roots, locations, 4
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_generated_bind(
            &heap, rows, &mmay_test_nested_rows, &rows_view
        ) == MMAY_MANAGED_OK);
        before_bytes = mmay_managed_heap_live_bytes(&heap);
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_array_generated_cache_children(
            &heap, rows, &rows_view
        );
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            MMAY_EXPECT(rows_view.object->generated_children != NULL);
            MMAY_EXPECT(rows_view.object->generated_children->count == 2);
            MMAY_EXPECT(rows_view.object->generated_children->generation
                == rows_view.object->mutation_generation);
            MMAY_EXPECT(mmay_managed_array_generated_bind(
                &heap, row0, &mmay_managed_array_f64_descriptor, &row0_view
            ) == MMAY_MANAGED_OK);
            MMAY_EXPECT(rows_view.object->generated_children->entries[0].object
                == row0_view.object);
            MMAY_EXPECT(rows_view.object->generated_children
                ->entries[0].array_snapshot.length == 2);
            state.failure_index = SIZE_MAX;
            MMAY_EXPECT(mmay_managed_array_access_set(
                &heap, rows, &mmay_test_nested_rows, 0,
                MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
                mmay_test_nested_reference(replacement), &length
            ) == MMAY_MANAGED_OK);
            MMAY_EXPECT(rows_view.object->generated_children->generation
                != rows_view.object->mutation_generation);
            MMAY_EXPECT(mmay_managed_array_generated_cache_children(
                &heap, rows, &rows_view
            ) == MMAY_MANAGED_OK);
            MMAY_EXPECT(mmay_managed_array_generated_bind(
                &heap, replacement, &mmay_managed_array_f64_descriptor,
                &replacement_view
            ) == MMAY_MANAGED_OK);
            MMAY_EXPECT(rows_view.object->generated_children->entries[0].object
                == replacement_view.object);
            MMAY_EXPECT(rows_view.object->generated_children
                ->entries[0].array_snapshot.length == 3);
        } else {
            MMAY_EXPECT(rows_view.object->generated_children == NULL);
            MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == before_bytes);
        }
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_managed_array_nested(void) {
    mmay_test_nested_ragged_gc_and_mutation();
    mmay_test_nested_generated_cache_failure_and_refresh();
}
