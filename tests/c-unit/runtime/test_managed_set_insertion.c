#include "../test_support.h"

#include "../../../runtime/common/managed_internal.h"

static const MarimayManagedMapDescriptor mmay_insertion_set = {
    {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_INVALID, false, MMAY_MANAGED_OBJECT_INVALID, NULL}, true
};
static const MarimayManagedMapDescriptor mmay_insertion_strings = {
    {MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_STRING, NULL},
    {MMAY_MANAGED_CELL_INVALID, false, MMAY_MANAGED_OBJECT_INVALID, NULL}, true
};
static const MarimayManagedMapDescriptor mmay_insertion_map = {
    {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL}, false
};

void mmay_test_managed_set_insertion(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap, other;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference set, strings, map, text, equal_text, foreign, stale;
    MarimayManagedCell key = {0};
    MarimayManagedRootFrame frame = {0};
    MarimayManagedReference *roots[] = {&set, &strings, &map, &text, &equal_text};
    MarimayManagedSlot *slot;
    bool inserted;
    uint64_t generation;
    size_t length, attempts;
    options.max_collection_length = 2;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_managed_heap_init(&heap, mmay_fail_allocator(&state), options));
    MMAY_EXPECT(mmay_managed_heap_init(&other, mmay_fail_allocator(&state), options));
    MMAY_EXPECT(mmay_managed_set_new(&heap, &mmay_insertion_set, &set) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_set_new(&heap, &mmay_insertion_strings, &strings) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_new(&heap, &mmay_insertion_map, &map) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(&heap, (const unsigned char *)"same", 4,
        &text) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(&heap, (const unsigned char *)"same", 4,
        &equal_text) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_set_new(&other, &mmay_insertion_set, &foreign) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, roots, 5) == MMAY_MANAGED_OK);
    attempts = state.attempts;
    key.f64_bits = UINT64_MAX;
    inserted = true;
    MMAY_EXPECT(mmay_managed_set_add(NULL, set, key, &inserted)
        == MMAY_MANAGED_INVALID_REFERENCE && inserted);
    MMAY_EXPECT(mmay_managed_set_add(NULL, set, key, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_set_add(&heap, map, key, &inserted)
        == MMAY_MANAGED_TYPE_MISMATCH && inserted);
    MMAY_EXPECT(mmay_managed_set_add(&heap, map, key, NULL) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_managed_set_add(&heap, text, key, &inserted)
        == MMAY_MANAGED_TYPE_MISMATCH && inserted);
    MMAY_EXPECT(mmay_managed_set_add(&heap, foreign, key, &inserted)
        == MMAY_MANAGED_INVALID_REFERENCE && inserted);
    stale = set;
    stale.generation += 1U;
    MMAY_EXPECT(mmay_managed_set_add(&heap, stale, key, &inserted)
        == MMAY_MANAGED_INVALID_REFERENCE && inserted);
    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(state.attempts == attempts);

    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, &inserted)
        == MMAY_MANAGED_OK && inserted);
    MMAY_EXPECT(mmay_managed_set_mutation_generation(&heap, set, &generation)
        == MMAY_MANAGED_OK && generation == 1);
    attempts = state.attempts;
    state.failure_index = attempts;
    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, &inserted)
        == MMAY_MANAGED_OK && !inserted);
    MMAY_EXPECT(state.attempts == attempts);
    state.failure_index = SIZE_MAX;
    MMAY_EXPECT(mmay_managed_set_check_mutation_generation(&heap, set, generation)
        == MMAY_MANAGED_OK);
    key.f64_bits = 0;
    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, &inserted)
        == MMAY_MANAGED_OK && inserted);
    key.f64_bits = 1;
    inserted = true;
    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, &inserted)
        == MMAY_MANAGED_LIMIT_REACHED && !inserted);
    MMAY_EXPECT(mmay_managed_set_length(&heap, set, &length) == MMAY_MANAGED_OK && length == 2);
    MMAY_EXPECT(mmay_managed_set_mutation_generation(&heap, set, &generation)
        == MMAY_MANAGED_OK && generation == 2);
    heap.options.max_collection_length = 3; /* Isolate generation exhaustion. */
    slot = mmay_managed_resolve(&heap, set);
    MMAY_EXPECT(slot != NULL);
    if (slot != NULL) slot->as.map->mutation_generation = UINT64_MAX;
    key.f64_bits = 0;
    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, &inserted)
        == MMAY_MANAGED_OK && !inserted);
    key.f64_bits = 1;
    MMAY_EXPECT(mmay_managed_set_add(&heap, set, key, &inserted)
        == MMAY_MANAGED_LIMIT_REACHED && !inserted);

    key.reference = map;
    inserted = true;
    MMAY_EXPECT(mmay_managed_set_add(&heap, strings, key, &inserted)
        == MMAY_MANAGED_TYPE_MISMATCH && !inserted);
    key.reference = foreign;
    inserted = true;
    MMAY_EXPECT(mmay_managed_set_add(&heap, strings, key, &inserted)
        == MMAY_MANAGED_INVALID_REFERENCE && !inserted);
    key.reference = text;
    MMAY_EXPECT(mmay_managed_set_add(&heap, strings, key, &inserted)
        == MMAY_MANAGED_OK && inserted);
    key.reference = equal_text;
    MMAY_EXPECT(mmay_managed_set_add(&heap, strings, key, &inserted)
        == MMAY_MANAGED_OK && !inserted);
    MMAY_EXPECT(mmay_managed_set_length(&heap, strings, &length)
        == MMAY_MANAGED_OK && length == 1);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_set_add(&heap, strings, key, &inserted)
        == MMAY_MANAGED_OK && !inserted);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    inserted = true;
    MMAY_EXPECT(mmay_managed_set_add(&heap, strings, key, &inserted)
        == MMAY_MANAGED_INVALID_REFERENCE && inserted);
    mmay_managed_heap_dispose(&other);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}
