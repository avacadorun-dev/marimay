#include "../test_support.h"

#include "../../../runtime/common/managed_internal.h"

static const MarimayManagedMapDescriptor mmay_cache_raw_map = {
    {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL}, false
};

static MarimayManagedCell mmay_cache_cell(uint64_t bits) {
    MarimayManagedCell cell = {0};
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedMap *mmay_cache_object(
    MarimayManagedHeap *heap, MarimayManagedReference reference
) {
    MarimayManagedSlot *slot = mmay_managed_resolve(heap, reference);
    MMAY_EXPECT(slot != NULL && slot->kind == MMAY_MANAGED_OBJECT_MAP);
    return slot != NULL ? slot->as.map : NULL;
}

static void mmay_test_map_cache_differential(void) {
    MarimayFailAllocator states[2];
    MarimayManagedHeap heaps[2];
    MarimayManagedReference maps[2];
    MarimayManagedRootFrame frames[2] = {{0}, {0}};
    MarimayManagedReference *roots[2][1] = {{&maps[0]}, {&maps[1]}};
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    uint64_t random = UINT64_C(0x20260907cace);
    size_t heap_index, step;
    options.max_objects = 16;
    options.max_live_bytes = 65536;
    options.max_collection_length = 32;
    for (heap_index = 0; heap_index < 2; heap_index += 1) {
        mmay_fail_allocator_init(&states[heap_index], mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_managed_heap_init(&heaps[heap_index],
            mmay_fail_allocator(&states[heap_index]), options));
        MMAY_EXPECT(mmay_managed_map_new(&heaps[heap_index], &mmay_cache_raw_map,
            &maps[heap_index]) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heaps[heap_index], &frames[heap_index],
            roots[heap_index], 1) == MMAY_MANAGED_OK);
    }
    for (step = 0; step < 8192; step += 1) {
        MarimayManagedStatus status[2];
        MarimayManagedCell before[2], after[2];
        bool present[2], changed[2];
        size_t lengths[2];
        uint64_t generations[2];
        uint64_t key, value;
        unsigned int action;
        random = random * UINT64_C(6364136223846793005) + 1;
        key = (random >> 24) % 64;
        value = random;
        action = (unsigned int)((random >> 40) % 8);
        for (heap_index = 0; heap_index < 2; heap_index += 1) {
            MarimayManagedHeap *heap = &heaps[heap_index];
            const MarimayManagedReference alias = maps[heap_index];
            MarimayManagedMap *object = mmay_cache_object(heap, alias);
            const size_t attempts = states[heap_index].attempts;
            if (heap_index == 1) object->lookup.valid = false;
            MMAY_EXPECT(mmay_managed_map_get(heap, alias, mmay_cache_cell(key),
                &before[heap_index], &present[heap_index]) == MMAY_MANAGED_OK);
            MMAY_EXPECT(object->lookup.valid && object->lookup.key_bits == key);
            MMAY_EXPECT(object->lookup.generation == object->mutation_generation);
            MMAY_EXPECT(states[heap_index].attempts == attempts);
            if (heap_index == 1) object->lookup.valid = false;
            after[heap_index] = mmay_cache_cell(0);
            changed[heap_index] = false;
            if (step % 127 == 0) {
                status[heap_index] = mmay_managed_map_clear(heap, alias);
            } else if (action < 5) {
                status[heap_index] = mmay_managed_map_put(heap, alias,
                    mmay_cache_cell(key), mmay_cache_cell(value), &changed[heap_index]);
            } else if (action == 5) {
                status[heap_index] = mmay_managed_map_remove(heap, alias,
                    mmay_cache_cell(key), &after[heap_index], &changed[heap_index]);
            } else if (action == 6) {
                status[heap_index] = mmay_managed_map_put(heap, alias,
                    mmay_cache_cell(key ^ UINT64_C(1)), mmay_cache_cell(value),
                    &changed[heap_index]);
            } else {
                status[heap_index] = mmay_managed_map_get(heap, alias,
                    mmay_cache_cell(key), &after[heap_index], &changed[heap_index]);
            }
            MMAY_EXPECT(mmay_managed_map_length(heap, alias, &lengths[heap_index])
                == MMAY_MANAGED_OK);
            MMAY_EXPECT(mmay_managed_map_mutation_generation(heap, alias,
                &generations[heap_index]) == MMAY_MANAGED_OK);
            if (step % 29 == 0) MMAY_EXPECT(mmay_managed_heap_collect(heap) == MMAY_MANAGED_OK);
        }
        MMAY_EXPECT(status[0] == status[1] && present[0] == present[1]);
        MMAY_EXPECT(before[0].f64_bits == before[1].f64_bits);
        MMAY_EXPECT(after[0].f64_bits == after[1].f64_bits && changed[0] == changed[1]);
        MMAY_EXPECT(lengths[0] == lengths[1] && generations[0] == generations[1]);
        if (step % 97 == 0) {
            MarimayManagedCell rejected;
            bool found;
            MMAY_EXPECT(mmay_managed_map_get(&heaps[0], maps[1], mmay_cache_cell(key),
                &rejected, &found) == MMAY_MANAGED_INVALID_REFERENCE);
        }
        if (step % 31 == 0) {
            size_t ordinal;
            for (ordinal = 0; ordinal < lengths[0]; ordinal += 1) {
                MarimayManagedCell keys[2], values[2];
                for (heap_index = 0; heap_index < 2; heap_index += 1) {
                    MMAY_EXPECT(mmay_managed_map_entry(&heaps[heap_index], maps[heap_index],
                        ordinal, &keys[heap_index], &values[heap_index]) == MMAY_MANAGED_OK);
                }
                MMAY_EXPECT(keys[0].f64_bits == keys[1].f64_bits);
                MMAY_EXPECT(values[0].f64_bits == values[1].f64_bits);
            }
        }
    }
    for (heap_index = 0; heap_index < 2; heap_index += 1) {
        MMAY_EXPECT(mmay_managed_root_pop(&heaps[heap_index], &frames[heap_index]) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_heap_collect(&heaps[heap_index]) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_heap_live_objects(&heaps[heap_index]) == 0);
        mmay_managed_heap_dispose(&heaps[heap_index]);
        MMAY_EXPECT(states[heap_index].live_allocations == 0 && states[heap_index].live_bytes == 0);
    }
}

static void mmay_test_map_cache_boundaries(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference map;
    MarimayManagedMap *object;
    MarimayManagedCell value;
    bool changed, found;
    size_t attempts;
    uint64_t seed0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_managed_heap_init(&heap, mmay_fail_allocator(&state), options));
    MMAY_EXPECT(mmay_managed_map_new(&heap, &mmay_cache_raw_map, &map) == MMAY_MANAGED_OK);
    object = mmay_cache_object(&heap, map);
    MMAY_EXPECT(!object->lookup.valid);
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_OK && !found);
    MMAY_EXPECT(mmay_managed_map_put(&heap, map, mmay_cache_cell(UINT64_MAX),
        mmay_cache_cell(UINT64_MAX), &changed) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(!object->lookup.valid); /* First storage publication invalidates absence. */
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_OK && found && value.f64_bits == UINT64_MAX);
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        NULL, &found) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    attempts = state.attempts;
    state.failure_index = attempts;
    MMAY_EXPECT(mmay_managed_map_put(&heap, map, mmay_cache_cell(UINT64_MAX),
        mmay_cache_cell(42), &changed) == MMAY_MANAGED_OK && !changed);
    MMAY_EXPECT(state.attempts == attempts);
    state.failure_index = SIZE_MAX;
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_OK && found && value.f64_bits == 42);
    seed0 = heap.options.collection_hash_seed0;
    heap.options.collection_hash_seed0 ^= UINT64_C(1);
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_OK && !found);
    heap.options.collection_hash_seed0 = seed0;
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_OK && found && value.f64_bits == 42);
    object->mutation_generation = UINT64_MAX;
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_OK && found);
    MMAY_EXPECT(mmay_managed_map_put(&heap, map, mmay_cache_cell(UINT64_MAX),
        mmay_cache_cell(7), &changed) == MMAY_MANAGED_LIMIT_REACHED && !changed);
    MMAY_EXPECT(mmay_managed_map_remove(&heap, map, mmay_cache_cell(UINT64_MAX),
        &value, &found) == MMAY_MANAGED_LIMIT_REACHED && !found);
    MMAY_EXPECT(mmay_managed_map_clear(&heap, map) == MMAY_MANAGED_LIMIT_REACHED);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_test_map_cache_reference_lifetime(void) {
    static const MarimayManagedMapDescriptor strings = {
        {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_STRING, NULL}, false
    };
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference map, text, fresh;
    MarimayManagedRootFrame frame = {0};
    MarimayManagedReference *roots[] = {&map};
    MarimayManagedCell value = {0};
    bool found, changed;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_managed_heap_init(&heap, mmay_fail_allocator(&state),
        mmay_managed_heap_default_options()));
    MMAY_EXPECT(mmay_managed_map_new(&heap, &strings, &map) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, roots, 1) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(&heap, (const unsigned char *)"cached", 6,
        &text) == MMAY_MANAGED_OK);
    value.reference = text;
    MMAY_EXPECT(mmay_managed_map_put(&heap, map, mmay_cache_cell(1), value,
        &changed) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(1), &value,
        &found) == MMAY_MANAGED_OK && found);
    MMAY_EXPECT(mmay_cache_object(&heap, map)->lookup.valid);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 2);
    value.reference = map; /* A cached key does not bypass reference-value typing. */
    MMAY_EXPECT(mmay_managed_map_put(&heap, map, mmay_cache_cell(1), value,
        &changed) == MMAY_MANAGED_TYPE_MISMATCH && !changed);
    MMAY_EXPECT(mmay_managed_map_remove(&heap, map, mmay_cache_cell(1), &value,
        &found) == MMAY_MANAGED_OK && found);
    MMAY_EXPECT(mmay_managed_reference_equal(value.reference, text));
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 1);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_new(&heap, &strings, &fresh) == MMAY_MANAGED_OK);
    MMAY_EXPECT(!mmay_cache_object(&heap, fresh)->lookup.valid);
    MMAY_EXPECT(mmay_managed_map_get(&heap, map, mmay_cache_cell(1), &value,
        &found) == MMAY_MANAGED_INVALID_REFERENCE);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_managed_map_cache(void) {
    mmay_test_map_cache_differential();
    mmay_test_map_cache_boundaries();
    mmay_test_map_cache_reference_lifetime();
}
