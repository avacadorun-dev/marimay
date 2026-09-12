#include "managed_publication.h"
#include "mmay/runtime/managed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "string publication: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

static const unsigned char input[] = {'a', 0, 0xce, 0xbb};

static void init(MarimayManagedHeap *heap, MarimayFailAllocator *allocator,
    size_t budget, size_t objects, bool collect) {
    mmay_fail_allocator_init(allocator, mmay_system_allocator(), SIZE_MAX);
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_live_bytes = budget;
    options.max_objects = objects;
    options.collect_before_allocation = collect;
    EXPECT(mmay_managed_heap_init(heap, mmay_fail_allocator(allocator), options));
}

static void dispose(MarimayManagedHeap *heap, MarimayFailAllocator *allocator) {
    EXPECT(!heap->collecting);
    mmay_managed_heap_dispose(heap);
    EXPECT(allocator->live_allocations == 0 && allocator->live_bytes == 0);
}

static void contents(MarimayManagedHeap *heap, MarimayManagedReference value,
    const unsigned char *expected, size_t length) {
    const unsigned char *bytes = NULL;
    size_t actual = SIZE_MAX;
    EXPECT(mmay_managed_string_bytes(heap, value, &bytes, &actual) == MMAY_MANAGED_OK);
    EXPECT(actual == length);
    EXPECT(length == 0 ? bytes == NULL : memcmp(bytes, expected, length) == 0);
}

static size_t shape_length(size_t shape) {
    return shape == 0 ? 0 : shape == 4 ? 2 * sizeof(input) : sizeof(input);
}

static MarimayManagedStatus construct(MarimayManagedHeap *heap,
    size_t shape, MarimayManagedReference *out) {
    switch (shape) {
        case 0: return mmay_managed_string_new(heap, NULL, 0, out);
        case 1: return mmay_managed_string_new(heap, input, sizeof(input), out);
        case 2: return mmay_managed_string_concat(heap, input, sizeof(input), NULL, 0, out);
        case 3: return mmay_managed_string_concat(heap, NULL, 0, input, sizeof(input), out);
        default: return mmay_managed_string_concat(heap, input, sizeof(input), input, sizeof(input), out);
    }
}

static void budget_and_failures(void) {
    for (size_t shape = 0; shape < 5; ++shape) {
        size_t bytes = 8 * sizeof(MarimayManagedSlot)
            + sizeof(MarimayManagedString) + shape_length(shape);
        for (size_t collect = 0; collect < 2; ++collect) {
            for (size_t shortfall = 1; shortfall <= 2; ++shortfall) {
                MarimayManagedHeap heap;
                MarimayFailAllocator allocator;
                init(&heap, &allocator, shortfall == 1 ? bytes - 1 : 1, 8, collect != 0);
                allocator.failure_index = 0;
                MarimayManagedReference out;
                EXPECT(construct(&heap, shape, &out) == MMAY_MANAGED_LIMIT_REACHED);
                EXPECT(mmay_managed_reference_is_invalid(out));
                EXPECT(allocator.attempts == 0 && heap.live_byte_count == 0);
                MarimayManagedResourceFailure failure;
                EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
                EXPECT(failure.classification == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET
                    && failure.live_bytes == 0 && failure.requested_bytes == bytes);
                dispose(&heap, &allocator);
            }
            size_t attempts = 0;
            for (size_t failure_index = 0; failure_index <= 2; ++failure_index) {
                MarimayManagedHeap heap;
                MarimayFailAllocator allocator;
                init(&heap, &allocator, bytes, 8, collect != 0);
                allocator.failure_index = failure_index;
                MarimayManagedReference out;
                MarimayManagedStatus status = construct(&heap, shape, &out);
                attempts = allocator.attempts;
                if (failure_index < 2) {
                    EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED);
                    EXPECT(mmay_managed_reference_is_invalid(out));
                    EXPECT(heap.live_object_count == 0 && heap.live_byte_count == 0);
                    MarimayManagedResourceFailure failure;
                    EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
                    EXPECT(failure.classification == MMAY_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION);
                } else {
                    EXPECT(status == MMAY_MANAGED_OK && attempts == 2);
                    EXPECT(allocator.live_allocations == 2 && heap.live_byte_count == bytes);
                    unsigned char expected[2 * sizeof(input)];
                    memcpy(expected, input, sizeof(input));
                    memcpy(expected + sizeof(input), input, sizeof(input));
                    contents(&heap, out, expected, shape_length(shape));
                }
                dispose(&heap, &allocator);
            }
            EXPECT(attempts == 2);
        }
    }
}

static void aliased_borrows(void) {
    for (size_t failure_index = 0; failure_index <= 1; ++failure_index) {
        MarimayManagedHeap heap;
        MarimayFailAllocator allocator;
        size_t slots = 8 * sizeof(MarimayManagedSlot);
        init(&heap, &allocator, slots + sizeof(MarimayManagedString) + 6, 8, true);
        MarimayManagedReference out = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&out};
        MarimayManagedRootFrame root = {0};
        EXPECT(mmay_managed_root_push(&heap, &root, locations, 1) == MMAY_MANAGED_OK);
        EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &out) == MMAY_MANAGED_OK);
        MarimayManagedReference old = out;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        EXPECT(mmay_managed_string_bytes(&heap, out, &bytes, &length) == MMAY_MANAGED_OK);
        allocator.attempts = 0;
        allocator.failure_index = failure_index;
        MarimayManagedStatus status = mmay_managed_string_concat(
            &heap, bytes + 1, length - 1, bytes + 1, length - 1, &out);
        EXPECT(allocator.attempts == 1 && !heap.collecting);
        EXPECT(mmay_managed_string_bytes(&heap, old, &bytes, &length) == MMAY_MANAGED_INVALID_REFERENCE);
        if (failure_index == 0) {
            EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED);
            EXPECT(mmay_managed_reference_is_invalid(out));
            EXPECT(heap.live_object_count == 0 && heap.live_byte_count == slots);
        } else {
            static const unsigned char expected[] = {0, 0xce, 0xbb, 0, 0xce, 0xbb};
            EXPECT(status == MMAY_MANAGED_OK);
            EXPECT(out.slot == old.slot && out.generation == old.generation + 1);
            contents(&heap, out, expected, sizeof(expected));
            EXPECT(heap.live_object_count == 1);
            EXPECT(heap.live_byte_count == slots + sizeof(MarimayManagedString) + sizeof(expected));
        }
        EXPECT(mmay_managed_root_pop(&heap, &root) == MMAY_MANAGED_OK);
        dispose(&heap, &allocator);
    }
}

static void unrooted_borrows(void) {
    MarimayManagedHeap heap;
    MarimayFailAllocator allocator;
    init(&heap, &allocator, 4096, 8, true);
    MarimayManagedReference sources[] = {mmay_managed_invalid_reference(), mmay_managed_invalid_reference()};
    MarimayManagedReference *locations[] = {&sources[0], &sources[1]};
    MarimayManagedRootFrame root = {0};
    EXPECT(mmay_managed_root_push(&heap, &root, locations, 2) == MMAY_MANAGED_OK);
    for (size_t i = 0; i < 2; ++i)
        EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &sources[i]) == MMAY_MANAGED_OK);
    const unsigned char *a, *b;
    size_t na, nb;
    EXPECT(mmay_managed_string_bytes(&heap, sources[0], &a, &na) == MMAY_MANAGED_OK);
    EXPECT(mmay_managed_string_bytes(&heap, sources[1], &b, &nb) == MMAY_MANAGED_OK);
    EXPECT(mmay_managed_root_pop(&heap, &root) == MMAY_MANAGED_OK);
    heap.options.max_live_bytes = 8 * sizeof(MarimayManagedSlot)
        + sizeof(MarimayManagedString) + 2 * sizeof(input);
    MarimayManagedReference out;
    EXPECT(mmay_managed_string_concat(&heap, a, na, b, nb, &out) == MMAY_MANAGED_OK);
    unsigned char expected[2 * sizeof(input)];
    memcpy(expected, input, sizeof(input));
    memcpy(expected + sizeof(input), input, sizeof(input));
    contents(&heap, out, expected, sizeof(expected));
    EXPECT(heap.live_object_count == 1);
    for (size_t i = 0; i < 2; ++i)
        EXPECT(mmay_managed_string_bytes(&heap, sources[i], &a, &na) == MMAY_MANAGED_INVALID_REFERENCE);
    dispose(&heap, &allocator);
}

static void growth_failures(void) {
    for (size_t failure_index = 0; failure_index <= 3; ++failure_index) {
        MarimayManagedHeap heap;
        MarimayFailAllocator allocator;
        size_t exact = 16 * sizeof(MarimayManagedSlot)
            + 9 * (sizeof(MarimayManagedString) + sizeof(input));
        init(&heap, &allocator, exact, 16, false);
        MarimayManagedReference values[9];
        MarimayManagedReference *locations[9];
        for (size_t i = 0; i < 9; ++i) {
            values[i] = mmay_managed_invalid_reference();
            locations[i] = &values[i];
        }
        MarimayManagedRootFrame root = {0};
        EXPECT(mmay_managed_root_push(&heap, &root, locations, 9) == MMAY_MANAGED_OK);
        for (size_t i = 0; i < 8; ++i)
            EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &values[i]) == MMAY_MANAGED_OK);
        EXPECT(heap.slot_capacity == 8);
        size_t before_bytes = heap.live_byte_count;
        allocator.attempts = 0;
        allocator.failure_index = failure_index == 3 ? 0 : failure_index;
        if (failure_index == 3) heap.options.max_live_bytes = exact - 1;
        MarimayManagedStatus status = mmay_managed_string_new(&heap, input, sizeof(input), &values[8]);
        if (failure_index == 2) {
            EXPECT(status == MMAY_MANAGED_OK && allocator.attempts == 2);
            EXPECT(heap.slot_capacity == 16 && heap.live_byte_count == exact);
        } else {
            EXPECT(status == (failure_index == 3 ? MMAY_MANAGED_LIMIT_REACHED : MMAY_MANAGED_ALLOCATION_FAILED));
            EXPECT(heap.slot_capacity == 8 && heap.live_byte_count == before_bytes);
            EXPECT(heap.live_object_count == 8 && mmay_managed_reference_is_invalid(values[8]));
            if (failure_index == 3) EXPECT(allocator.attempts == 0);
        }
        for (size_t i = 0; i < 8; ++i) contents(&heap, values[i], input, sizeof(input));
        EXPECT(mmay_managed_root_pop(&heap, &root) == MMAY_MANAGED_OK);
        dispose(&heap, &allocator);
    }
}

static void errors_and_cancellation(void) {
    MarimayManagedHeap heap;
    MarimayFailAllocator allocator;
    init(&heap, &allocator, 4096, 8, true);
    MarimayManagedReference value = mmay_managed_invalid_reference();
    EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &value) == MMAY_MANAGED_OK);
    MarimayManagedRootFrame root = {0};
    MarimayManagedReference *locations[] = {&value};
    EXPECT(mmay_managed_root_push(&heap, &root, locations, 1) == MMAY_MANAGED_OK);
    MarimayManagedReference saved = value;
    value.heap_provenance ^= 1;
    allocator.attempts = 0;
    allocator.failure_index = 0;
    MarimayManagedReference out;
    EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &out) == MMAY_MANAGED_INVALID_REFERENCE);
    EXPECT(mmay_managed_reference_is_invalid(out) && allocator.attempts == 0 && !heap.collecting);
    value = saved;
    contents(&heap, value, input, sizeof(input));
    EXPECT(mmay_managed_root_pop(&heap, &root) == MMAY_MANAGED_OK);
    const unsigned char *bytes;
    size_t length;
    EXPECT(mmay_managed_string_bytes(&heap, value, &bytes, &length) == MMAY_MANAGED_OK);
    MarimayManagedPublication publication = {0};
    EXPECT(mmay_managed_publication_prepare(&heap, sizeof(MarimayManagedString),
        NULL, 0, NULL, 0, true, &publication) == MMAY_MANAGED_OK);
    EXPECT(heap.collecting && publication.collection.pending);
    EXPECT(memcmp(bytes, input, length) == 0 && allocator.attempts == 0);
    MarimayManagedPublication nested = {0};
    EXPECT(mmay_managed_publication_prepare(&heap, sizeof(MarimayManagedString),
        NULL, 0, NULL, 0, true, &nested) == MMAY_MANAGED_INVALID_ARGUMENT);
    EXPECT(mmay_managed_publication_commit(&publication, MMAY_MANAGED_OBJECT_STRING,
        NULL, &out) == MMAY_MANAGED_INVALID_ARGUMENT);
    EXPECT(!publication.ready && !heap.collecting && heap.live_object_count == 0);
    mmay_managed_publication_cancel(&publication);
    dispose(&heap, &allocator);
}

static void retired_slot(void) {
    MarimayManagedHeap heap;
    MarimayFailAllocator allocator;
    init(&heap, &allocator, 4096, 1, false);
    MarimayManagedReference value;
    EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &value) == MMAY_MANAGED_OK);
    heap.slots[value.slot].generation = UINT32_MAX;
    allocator.attempts = 0;
    allocator.failure_index = 0;
    EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &value) == MMAY_MANAGED_LIMIT_REACHED);
    EXPECT(allocator.attempts == 0 && !heap.collecting && heap.live_object_count == 0);
    EXPECT(heap.slots[0].retired && heap.free_slot == UINT32_MAX);
    dispose(&heap, &allocator);
}

static void borrowed_array_bytes(void) {
    static const MarimayManagedArrayDescriptor descriptor = {
        MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW8, 1,
        MMAY_MANAGED_OBJECT_INVALID, NULL
    };
    for (size_t failure_index = 0; failure_index <= 1; ++failure_index) {
        MarimayManagedHeap heap;
        MarimayFailAllocator allocator;
        init(&heap, &allocator, 4096, 8, true);
        MarimayManagedCell cells[sizeof(input)] = {0};
        for (size_t i = 0; i < sizeof(input); ++i) cells[i].f64_bits = input[i];
        MarimayManagedReference array, out;
        EXPECT(mmay_managed_array_new(&heap, &descriptor, cells,
            sizeof(input), &array) == MMAY_MANAGED_OK);
        MarimayManagedArrayView view = {0};
        EXPECT(mmay_managed_array_view_bind(&heap, array, &descriptor, &view) == MMAY_MANAGED_OK);
        const unsigned char *bytes = *view.storage;
        heap.options.max_live_bytes = 8 * sizeof(MarimayManagedSlot)
            + sizeof(MarimayManagedString) + sizeof(input) - 1;
        allocator.attempts = 0;
        allocator.failure_index = failure_index;
        MarimayManagedStatus status = mmay_managed_string_new(&heap,
            bytes + 1, sizeof(input) - 1, &out);
        EXPECT(allocator.attempts == 1 && !heap.collecting);
        size_t length;
        EXPECT(mmay_managed_array_length(&heap, array, &length) == MMAY_MANAGED_INVALID_REFERENCE);
        if (failure_index == 0) {
            EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED);
            EXPECT(mmay_managed_reference_is_invalid(out) && heap.live_object_count == 0);
        } else {
            EXPECT(status == MMAY_MANAGED_OK && heap.live_object_count == 1);
            contents(&heap, out, input + 1, sizeof(input) - 1);
        }
        dispose(&heap, &allocator);
    }
}

static void rooted_object_limit(void) {
    MarimayManagedHeap heap;
    MarimayFailAllocator allocator;
    init(&heap, &allocator, 4096, 1, false);
    MarimayManagedReference value, out;
    EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &value) == MMAY_MANAGED_OK);
    MarimayManagedReference *locations[] = {&value};
    MarimayManagedRootFrame root = {0};
    EXPECT(mmay_managed_root_push(&heap, &root, locations, 1) == MMAY_MANAGED_OK);
    allocator.attempts = 0;
    allocator.failure_index = 0;
    EXPECT(mmay_managed_string_new(&heap, input, sizeof(input), &out) == MMAY_MANAGED_LIMIT_REACHED);
    EXPECT(allocator.attempts == 0 && heap.live_object_count == 1);
    EXPECT(mmay_managed_reference_is_invalid(out));
    contents(&heap, value, input, sizeof(input));
    EXPECT(mmay_managed_root_pop(&heap, &root) == MMAY_MANAGED_OK);
    dispose(&heap, &allocator);
}

int main(void) {
    budget_and_failures();
    aliased_borrows();
    unrooted_borrows();
    growth_failures();
    errors_and_cancellation();
    retired_slot();
    borrowed_array_bytes();
    rooted_object_limit();
    puts("String publication: budgets, borrows, all allocation failures and GC teardown passed");
    return 0;
}
