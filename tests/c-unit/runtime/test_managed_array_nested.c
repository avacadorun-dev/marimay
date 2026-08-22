#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/runtime/managed.h"

static const RudaManagedArrayDescriptor ruda_test_nested_rows = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(RudaManagedCell),
    RUDA_MANAGED_OBJECT_ARRAY,
    &ruda_managed_array_f64_descriptor
};

static const RudaManagedArrayDescriptor ruda_test_nested_planes = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(RudaManagedCell),
    RUDA_MANAGED_OBJECT_ARRAY,
    &ruda_test_nested_rows
};

static RudaManagedCell ruda_test_nested_raw(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static RudaManagedCell ruda_test_nested_reference(
    RudaManagedReference reference
) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static void ruda_test_nested_ragged_gc_and_mutation(void) {
    RudaManagedHeap heap;
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    RudaManagedReference row0 = ruda_managed_invalid_reference();
    RudaManagedReference row1 = ruda_managed_invalid_reference();
    RudaManagedReference replacement = ruda_managed_invalid_reference();
    RudaManagedReference rows = ruda_managed_invalid_reference();
    RudaManagedReference plane = ruda_managed_invalid_reference();
    RudaManagedReference stale = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&plane};
    RudaManagedRootFrame roots;
    RudaManagedCell row0_values[] = {
        ruda_test_nested_raw(10), ruda_test_nested_raw(11)
    };
    RudaManagedCell replacement_values[] = {
        ruda_test_nested_raw(90), ruda_test_nested_raw(91),
        ruda_test_nested_raw(92)
    };
    RudaManagedCell rows_values[2];
    RudaManagedCell plane_value;
    RudaManagedCell observed;
    size_t length = 0;
    memset(&roots, 0, sizeof(roots));
    memset(&observed, 0, sizeof(observed));
    options.max_objects = 64;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 64;
    options.collect_before_allocation = false;
    RUDA_EXPECT(ruda_managed_heap_init(
        &heap, ruda_system_allocator(), options
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor,
        row0_values, 2, &row0
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &row1
    ) == RUDA_MANAGED_OK);
    rows_values[0] = ruda_test_nested_reference(row0);
    rows_values[1] = ruda_test_nested_reference(row1);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_nested_rows, rows_values, 2, &rows
    ) == RUDA_MANAGED_OK);
    plane_value = ruda_test_nested_reference(rows);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_nested_planes, &plane_value, 1, &plane
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &roots, locations, 1)
        == RUDA_MANAGED_OK);
    row0 = ruda_managed_invalid_reference();
    row1 = ruda_managed_invalid_reference();
    rows = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);

    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, plane, &ruda_test_nested_planes, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && length == 1);
    rows = observed.reference;
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, rows, &ruda_test_nested_rows, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && length == 2);
    row0 = observed.reference;
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, row0, &ruda_managed_array_f64_descriptor, 1,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == 11 && length == 2);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, rows, &ruda_test_nested_rows, 1,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(
        &heap, observed.reference, &length
    ) == RUDA_MANAGED_OK && length == 0);

    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor,
        replacement_values, 3, &replacement
    ) == RUDA_MANAGED_OK);
    stale = row0;
    RUDA_EXPECT(ruda_managed_array_access_set(
        &heap, rows, &ruda_test_nested_rows, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        ruda_test_nested_reference(replacement), &length
    ) == RUDA_MANAGED_OK);
    replacement = ruda_managed_invalid_reference();
    row0 = ruda_managed_invalid_reference();
    rows = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, stale, &length)
        == RUDA_MANAGED_INVALID_REFERENCE);

    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, plane, &ruda_test_nested_planes, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK);
    rows = observed.reference;
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, rows, &ruda_test_nested_rows, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, observed.reference, &ruda_managed_array_f64_descriptor, 2,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == 92);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, plane, &ruda_test_nested_planes, 1,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_BOUNDS);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, plane, &ruda_test_nested_rows, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_TYPE_MISMATCH);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

void ruda_test_managed_array_nested(void) {
    ruda_test_nested_ragged_gc_and_mutation();
}
