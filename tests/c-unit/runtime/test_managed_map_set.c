#include "../test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

static const MarimayManagedFieldDescriptor mmay_test_map_raw_field = {
    MMAY_MANAGED_CELL_RAW64,
    false,
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

static const MarimayManagedFieldDescriptor mmay_test_map_string_field = {
    MMAY_MANAGED_CELL_REFERENCE,
    false,
    MMAY_MANAGED_OBJECT_STRING,
    NULL
};

static const MarimayManagedMapDescriptor mmay_test_raw_map = {
    {
        MMAY_MANAGED_CELL_RAW64,
        false,
        MMAY_MANAGED_OBJECT_INVALID,
        NULL
    },
    {
        MMAY_MANAGED_CELL_RAW64,
        false,
        MMAY_MANAGED_OBJECT_INVALID,
        NULL
    },
    false
};

static const MarimayManagedMapDescriptor mmay_test_raw_set = {
    {
        MMAY_MANAGED_CELL_RAW64,
        false,
        MMAY_MANAGED_OBJECT_INVALID,
        NULL
    },
    {
        MMAY_MANAGED_CELL_INVALID,
        false,
        MMAY_MANAGED_OBJECT_INVALID,
        NULL
    },
    true
};

static const MarimayManagedMapDescriptor mmay_test_string_map = {
    {
        MMAY_MANAGED_CELL_REFERENCE,
        false,
        MMAY_MANAGED_OBJECT_STRING,
        NULL
    },
    {
        MMAY_MANAGED_CELL_REFERENCE,
        false,
        MMAY_MANAGED_OBJECT_STRING,
        NULL
    },
    false
};

static MarimayManagedCell mmay_test_map_raw(uint64_t bits) {
    MarimayManagedCell value;
    memset(&value, 0, sizeof(value));
    value.f64_bits = bits;
    return value;
}

static MarimayManagedCell mmay_test_map_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell value;
    memset(&value, 0, sizeof(value));
    value.reference = reference;
    return value;
}

static bool mmay_test_map_heap(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    bool collect
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 256;
    options.max_live_bytes = 1024U * 1024U;
    options.max_collection_length = 256;
    options.collect_before_allocation = collect;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_managed_map_order_and_mutation(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference map = mmay_managed_invalid_reference();
    uint64_t generation = 0;
    size_t length = 0;
    size_t index = 0;
    bool changed = false;
    bool found = false;
    MarimayManagedCell key;
    MarimayManagedCell value;
    MMAY_EXPECT(mmay_test_map_heap(
        &heap, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_managed_map_new(&heap, &mmay_test_raw_map, &map)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_mutation_generation(
        &heap, map, &generation
    ) == MMAY_MANAGED_OK && generation == 0);
    for (index = 0; index < 64; index += 1) {
        changed = false;
        MMAY_EXPECT(mmay_managed_map_put(
            &heap,
            map,
            mmay_test_map_raw((uint64_t)index),
            mmay_test_map_raw((uint64_t)(index * 10U)),
            &changed
        ) == MMAY_MANAGED_OK && changed);
    }
    MMAY_EXPECT(mmay_managed_map_length(&heap, map, &length)
        == MMAY_MANAGED_OK && length == 64);
    MMAY_EXPECT(mmay_managed_map_mutation_generation(
        &heap, map, &generation
    ) == MMAY_MANAGED_OK && generation == 64);

    changed = true;
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, map, mmay_test_map_raw(5), mmay_test_map_raw(555), &changed
    ) == MMAY_MANAGED_OK && !changed);
    MMAY_EXPECT(mmay_managed_map_check_mutation_generation(&heap, map, 64)
        == MMAY_MANAGED_ITERATOR_INVALIDATED);
    generation = 65;
    MMAY_EXPECT(mmay_managed_map_get(
        &heap, map, mmay_test_map_raw(5), &value, &found
    ) == MMAY_MANAGED_OK && found && value.f64_bits == 555);

    changed = false;
    MMAY_EXPECT(mmay_managed_map_remove(
        &heap, map, mmay_test_map_raw(2), &value, &changed
    ) == MMAY_MANAGED_OK && changed && value.f64_bits == 20);
    MMAY_EXPECT(mmay_managed_map_entry(&heap, map, 2, &key, &value)
        == MMAY_MANAGED_OK && key.f64_bits == 3 && value.f64_bits == 30);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, map, mmay_test_map_raw(2), mmay_test_map_raw(222), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_map_entry(&heap, map, 63, &key, &value)
        == MMAY_MANAGED_OK && key.f64_bits == 2 && value.f64_bits == 222);

    changed = true;
    MMAY_EXPECT(mmay_managed_map_remove(
        &heap, map, mmay_test_map_raw(999), &value, &changed
    ) == MMAY_MANAGED_OK && !changed);
    MMAY_EXPECT(mmay_managed_map_mutation_generation(
        &heap, map, &generation
    ) == MMAY_MANAGED_OK && generation == 67);
    MMAY_EXPECT(mmay_managed_map_clear(&heap, map) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_clear(&heap, map) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_mutation_generation(
        &heap, map, &generation
    ) == MMAY_MANAGED_OK && generation == 68);
    MMAY_EXPECT(mmay_managed_map_length(&heap, map, &length)
        == MMAY_MANAGED_OK && length == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_map_seed_independent_order(void) {
    MarimayManagedHeap heaps[2];
    MarimayManagedHeapOptions options;
    MarimayManagedReference maps[2] = {
        {0}, {0}
    };
    static const uint64_t seeds[2][2] = {
        {
            UINT64_C(0x736f6d6570736575),
            UINT64_C(0x646f72616e646f6d)
        },
        {
            UINT64_C(0x0706050403020100),
            UINT64_C(0x0f0e0d0c0b0a0908)
        }
    };
    MarimayManagedCell keys[2];
    MarimayManagedCell values[2];
    MarimayManagedCell removed;
    bool changed = false;
    size_t heap_index = 0;
    size_t index = 0;
    uint64_t key_bits = 0;

    for (heap_index = 0; heap_index < 2; heap_index += 1) {
        options = mmay_managed_heap_default_options();
        options.max_objects = 16;
        options.max_live_bytes = 1024U * 1024U;
        options.max_collection_length = 256;
        options.collection_hash_seed0 = seeds[heap_index][0];
        options.collection_hash_seed1 = seeds[heap_index][1];
        MMAY_EXPECT(mmay_managed_heap_init(
            &heaps[heap_index], mmay_system_allocator(), options
        ));
        MMAY_EXPECT(mmay_managed_map_new(
            &heaps[heap_index], &mmay_test_raw_map, &maps[heap_index]
        ) == MMAY_MANAGED_OK);
        for (index = 0; index < 96; index += 1) {
            key_bits = ((uint64_t)index * UINT64_C(37)) % UINT64_C(96);
            MMAY_EXPECT(mmay_managed_map_put(
                &heaps[heap_index], maps[heap_index],
                mmay_test_map_raw(key_bits),
                mmay_test_map_raw(key_bits * UINT64_C(11)), &changed
            ) == MMAY_MANAGED_OK && changed);
        }
        for (index = 0; index < 24; index += 1) {
            key_bits = (uint64_t)index * UINT64_C(4);
            MMAY_EXPECT(mmay_managed_map_remove(
                &heaps[heap_index], maps[heap_index],
                mmay_test_map_raw(key_bits), &removed, &changed
            ) == MMAY_MANAGED_OK && changed);
            MMAY_EXPECT(mmay_managed_map_put(
                &heaps[heap_index], maps[heap_index],
                mmay_test_map_raw(key_bits),
                mmay_test_map_raw(key_bits * UINT64_C(13)), &changed
            ) == MMAY_MANAGED_OK && changed);
        }
    }

    for (index = 0; index < 96; index += 1) {
        MMAY_EXPECT(mmay_managed_map_entry(
            &heaps[0], maps[0], index, &keys[0], &values[0]
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_map_entry(
            &heaps[1], maps[1], index, &keys[1], &values[1]
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(keys[0].f64_bits == keys[1].f64_bits);
        MMAY_EXPECT(values[0].f64_bits == values[1].f64_bits);
    }
    mmay_managed_heap_dispose(&heaps[1]);
    mmay_managed_heap_dispose(&heaps[0]);
}

static void mmay_test_managed_set_order_and_duplicates(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference set = mmay_managed_invalid_reference();
    MarimayManagedCell value;
    bool changed = false;
    bool contains = false;
    uint64_t generation = 0;
    size_t length = 0;
    MMAY_EXPECT(mmay_test_map_heap(
        &heap, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_managed_set_new(&heap, &mmay_test_raw_set, &set)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(7), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(8), &changed
    ) == MMAY_MANAGED_OK && changed);
    changed = true;
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(7), &changed
    ) == MMAY_MANAGED_OK && !changed);
    MMAY_EXPECT(mmay_managed_set_mutation_generation(
        &heap, set, &generation
    ) == MMAY_MANAGED_OK && generation == 2);
    MMAY_EXPECT(mmay_managed_set_contains(
        &heap, set, mmay_test_map_raw(8), &contains
    ) == MMAY_MANAGED_OK && contains);
    MMAY_EXPECT(mmay_managed_set_remove(
        &heap, set, mmay_test_map_raw(7), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(7), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_set_entry(&heap, set, 0, &value)
        == MMAY_MANAGED_OK && value.f64_bits == 8);
    MMAY_EXPECT(mmay_managed_set_entry(&heap, set, 1, &value)
        == MMAY_MANAGED_OK && value.f64_bits == 7);
    MMAY_EXPECT(mmay_managed_set_length(&heap, set, &length)
        == MMAY_MANAGED_OK && length == 2);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_map_string_and_nominal_keys(void) {
    static const MarimayManagedFieldDescriptor point_fields[] = {
        {
            MMAY_MANAGED_CELL_RAW64,
            false,
            MMAY_MANAGED_OBJECT_INVALID,
            NULL
        },
        {
            MMAY_MANAGED_CELL_RAW64,
            false,
            MMAY_MANAGED_OBJECT_INVALID,
            NULL
        }
    };
    static const MarimayManagedRecordDescriptor point_descriptor = {
        2,
        point_fields
    };
    static const MarimayManagedMapDescriptor point_map_descriptor = {
        {
            MMAY_MANAGED_CELL_REFERENCE,
            false,
            MMAY_MANAGED_OBJECT_RECORD,
            &point_descriptor
        },
        {
            MMAY_MANAGED_CELL_RAW64,
            false,
            MMAY_MANAGED_OBJECT_INVALID,
            NULL
        },
        false
    };
    MarimayManagedHeap heap;
    MarimayManagedReference strings = mmay_managed_invalid_reference();
    MarimayManagedReference first = mmay_managed_invalid_reference();
    MarimayManagedReference equal = mmay_managed_invalid_reference();
    MarimayManagedReference different = mmay_managed_invalid_reference();
    MarimayManagedReference text_value = mmay_managed_invalid_reference();
    MarimayManagedReference points = mmay_managed_invalid_reference();
    MarimayManagedReference point_a = mmay_managed_invalid_reference();
    MarimayManagedReference point_b = mmay_managed_invalid_reference();
    MarimayManagedReference point_c = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {
        &strings, &first, &equal, &different, &text_value, &points,
        &point_a, &point_b, &point_c
    };
    MarimayManagedRootFrame frame;
    MarimayManagedCell fields[] = {
        {0}, {0}
    };
    MarimayManagedCell observed;
    bool changed = false;
    bool found = false;
    memset(&frame, 0, sizeof(frame));
    fields[0] = mmay_test_map_raw(4);
    fields[1] = mmay_test_map_raw(9);
    MMAY_EXPECT(mmay_test_map_heap(
        &heap, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_managed_map_new(&heap, &mmay_test_string_map, &strings)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &frame, locations, sizeof(locations) / sizeof(locations[0])
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(
        &heap, (const unsigned char *)"same\0\xce\xbb", 7, &first
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(
        &heap, (const unsigned char *)"same\0\xce\xbb", 7, &equal
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(
        &heap, (const unsigned char *)"same\0\xce\xbc", 7, &different
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_new(
        &heap, (const unsigned char *)"value", 5, &text_value
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, strings, mmay_test_map_reference(first),
        mmay_test_map_reference(text_value), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_map_get(
        &heap, strings, mmay_test_map_reference(equal), &observed, &found
    ) == MMAY_MANAGED_OK && found
        && mmay_managed_reference_equal(observed.reference, text_value));
    found = true;
    MMAY_EXPECT(mmay_managed_map_get(
        &heap, strings, mmay_test_map_reference(different), &observed, &found
    ) == MMAY_MANAGED_OK && !found);

    MMAY_EXPECT(mmay_managed_map_new(
        &heap, &point_map_descriptor, &points
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &point_descriptor, fields, 2, &point_a
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &point_descriptor, fields, 2, &point_b
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &point_descriptor, fields, 2, &point_c
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, points, mmay_test_map_reference(point_a),
        mmay_test_map_raw(49), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_map_get(
        &heap, points, mmay_test_map_reference(point_b), &observed, &found
    ) == MMAY_MANAGED_OK && found && observed.f64_bits == 49);
    MMAY_EXPECT(mmay_managed_map_get(
        &heap, points, mmay_test_map_reference(point_c), &observed, &found
    ) == MMAY_MANAGED_OK && found && observed.f64_bits == 49);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_map_gc_graphs(void) {
    MarimayManagedMapDescriptor graph_descriptor = {
        {
            MMAY_MANAGED_CELL_RAW64,
            false,
            MMAY_MANAGED_OBJECT_INVALID,
            NULL
        },
        {
            MMAY_MANAGED_CELL_REFERENCE,
            false,
            MMAY_MANAGED_OBJECT_MAP,
            NULL
        },
        false
    };
    MarimayManagedHeap heap;
    MarimayManagedReference left = mmay_managed_invalid_reference();
    MarimayManagedReference right = mmay_managed_invalid_reference();
    MarimayManagedReference *both[] = {&left, &right};
    MarimayManagedReference *only_left[] = {&left};
    MarimayManagedRootFrame frame;
    MarimayManagedCell observed;
    bool changed = false;
    bool found = false;
    size_t live = 0;
    memset(&frame, 0, sizeof(frame));
    graph_descriptor.value.reference_descriptor = &graph_descriptor;
    MMAY_EXPECT(mmay_test_map_heap(
        &heap, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_managed_map_new(&heap, &graph_descriptor, &left)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, both, 2)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_new(&heap, &graph_descriptor, &right)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, left, mmay_test_map_raw(1), mmay_test_map_reference(right),
        &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, right, mmay_test_map_raw(2), mmay_test_map_reference(left),
        &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    memset(&frame, 0, sizeof(frame));
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, only_left, 1)
        == MMAY_MANAGED_OK);
    right = mmay_managed_invalid_reference();
    live = mmay_managed_heap_live_objects(&heap);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == live);
    MMAY_EXPECT(mmay_managed_map_get(
        &heap, left, mmay_test_map_raw(1), &observed, &found
    ) == MMAY_MANAGED_OK && found);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    left = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_map_randomized_gc_graph(void) {
    enum {
        MMAY_TEST_GRAPH_COMPONENT_SIZE = 8,
        MMAY_TEST_GRAPH_NODE_COUNT = 16
    };
    MarimayManagedMapDescriptor graph_descriptor = {
        {
            MMAY_MANAGED_CELL_RAW64,
            false,
            MMAY_MANAGED_OBJECT_INVALID,
            NULL
        },
        {
            MMAY_MANAGED_CELL_REFERENCE,
            false,
            MMAY_MANAGED_OBJECT_MAP,
            NULL
        },
        false
    };
    MarimayManagedHeap heap;
    MarimayManagedReference nodes[MMAY_TEST_GRAPH_NODE_COUNT];
    MarimayManagedReference *locations[MMAY_TEST_GRAPH_NODE_COUNT];
    MarimayManagedReference root = mmay_managed_invalid_reference();
    MarimayManagedReference *only_root[] = {&root};
    MarimayManagedRootFrame all_frame;
    MarimayManagedRootFrame root_frame;
    uint64_t random_state = UINT64_C(0xc36c36c36c36c36c);
    size_t component_start = 0;
    size_t index = 0;
    size_t random_target = 0;
    bool changed = false;

    memset(&all_frame, 0, sizeof(all_frame));
    memset(&root_frame, 0, sizeof(root_frame));
    graph_descriptor.value.reference_descriptor = &graph_descriptor;
    MMAY_EXPECT(mmay_test_map_heap(
        &heap, mmay_system_allocator(), false
    ));
    for (index = 0; index < MMAY_TEST_GRAPH_NODE_COUNT; index += 1) {
        nodes[index] = mmay_managed_invalid_reference();
        locations[index] = &nodes[index];
    }
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &all_frame, locations, MMAY_TEST_GRAPH_NODE_COUNT
    ) == MMAY_MANAGED_OK);
    for (index = 0; index < MMAY_TEST_GRAPH_NODE_COUNT; index += 1) {
        MMAY_EXPECT(mmay_managed_map_new(
            &heap, &graph_descriptor, &nodes[index]
        ) == MMAY_MANAGED_OK);
    }
    for (index = 0; index < MMAY_TEST_GRAPH_NODE_COUNT; index += 1) {
        component_start = index < MMAY_TEST_GRAPH_COMPONENT_SIZE
            ? 0 : MMAY_TEST_GRAPH_COMPONENT_SIZE;
        random_state = random_state * UINT64_C(6364136223846793005)
            + UINT64_C(1442695040888963407);
        random_target = component_start
            + (size_t)(random_state % MMAY_TEST_GRAPH_COMPONENT_SIZE);
        MMAY_EXPECT(mmay_managed_map_put(
            &heap, nodes[index], mmay_test_map_raw(1),
            mmay_test_map_reference(nodes[
                component_start
                    + ((index - component_start + 1)
                        % MMAY_TEST_GRAPH_COMPONENT_SIZE)
            ]), &changed
        ) == MMAY_MANAGED_OK && changed);
        MMAY_EXPECT(mmay_managed_map_put(
            &heap, nodes[index], mmay_test_map_raw(2),
            mmay_test_map_reference(nodes[random_target]), &changed
        ) == MMAY_MANAGED_OK && changed);
    }
    root = nodes[0];
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &all_frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &root_frame, only_root, 1
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap)
        == MMAY_TEST_GRAPH_COMPONENT_SIZE);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &root_frame) == MMAY_MANAGED_OK);
    root = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 0);
    mmay_managed_heap_dispose(&heap);
}

static bool mmay_test_managed_collection_failure_run(
    size_t failure_index,
    bool is_set,
    size_t *attempts
) {
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayManagedHeap heap;
    MarimayManagedReference map = mmay_managed_invalid_reference();
    bool complete = false;
    bool changed = false;
    size_t index = 0;
    mmay_fail_allocator_init(
        &state, mmay_system_allocator(), failure_index
    );
    allocator = mmay_fail_allocator(&state);
    if (mmay_test_map_heap(&heap, allocator, true)) {
        MarimayManagedStatus status = is_set
            ? mmay_managed_set_new(&heap, &mmay_test_raw_set, &map)
            : mmay_managed_map_new(&heap, &mmay_test_raw_map, &map);
        if (status == MMAY_MANAGED_OK) {
            complete = true;
            for (index = 0; index < 48; index += 1) {
                size_t length = 0;
                uint64_t generation = 0;
                changed = false;
                status = is_set
                    ? mmay_managed_set_add(
                        &heap, map, mmay_test_map_raw(index), &changed
                    )
                    : mmay_managed_map_put(
                        &heap, map, mmay_test_map_raw(index),
                        mmay_test_map_raw(index * 3U), &changed
                    );
                if (status != MMAY_MANAGED_OK) {
                    MMAY_EXPECT(!changed);
                    MMAY_EXPECT((is_set
                        ? mmay_managed_set_length(&heap, map, &length)
                        : mmay_managed_map_length(&heap, map, &length)
                    ) == MMAY_MANAGED_OK && length == index);
                    MMAY_EXPECT((is_set
                        ? mmay_managed_set_mutation_generation(
                            &heap, map, &generation
                        )
                        : mmay_managed_map_mutation_generation(
                            &heap, map, &generation
                        )
                    ) == MMAY_MANAGED_OK && generation == index);
                    complete = false;
                    break;
                }
            }
        }
        mmay_managed_heap_dispose(&heap);
    }
    *attempts = state.attempts;
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    return complete;
}

static void mmay_test_managed_map_allocation_sweep(void) {
    unsigned int collection = 0;
    size_t failure = 0;
    size_t attempts = 0;
    bool complete = false;
    for (collection = 0; collection < 2; collection += 1) {
        complete = false;
        for (failure = 0; failure < 256; failure += 1) {
            complete = mmay_test_managed_collection_failure_run(
                failure, collection != 0, &attempts
            );
            if (complete) {
                MMAY_EXPECT(failure == attempts);
                break;
            }
        }
        MMAY_EXPECT(complete);
    }
}

static void mmay_test_managed_map_length_limit(void) {
    MarimayManagedHeap heap;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference map = mmay_managed_invalid_reference();
    MarimayManagedReference set = mmay_managed_invalid_reference();
    uint64_t generation = 0;
    bool changed = false;
    options.max_objects = 16;
    options.max_live_bytes = 64U * 1024U;
    options.max_collection_length = 2;
    MMAY_EXPECT(mmay_managed_heap_init(
        &heap, mmay_system_allocator(), options
    ));
    MMAY_EXPECT(mmay_managed_map_new(&heap, &mmay_test_raw_map, &map)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, map, mmay_test_map_raw(1), mmay_test_map_raw(1), &changed
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, map, mmay_test_map_raw(2), mmay_test_map_raw(2), &changed
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, map, mmay_test_map_raw(3), mmay_test_map_raw(3), &changed
    ) == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(mmay_managed_map_mutation_generation(
        &heap, map, &generation
    ) == MMAY_MANAGED_OK && generation == 2);
    changed = true;
    MMAY_EXPECT(mmay_managed_map_put(
        &heap, map, mmay_test_map_raw(2), mmay_test_map_raw(22), &changed
    ) == MMAY_MANAGED_OK && !changed);
    MMAY_EXPECT(mmay_managed_set_new(&heap, &mmay_test_raw_set, &set)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(1), &changed
    ) == MMAY_MANAGED_OK && changed);
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(2), &changed
    ) == MMAY_MANAGED_OK && changed);
    changed = true;
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(3), &changed
    ) == MMAY_MANAGED_LIMIT_REACHED && !changed);
    MMAY_EXPECT(mmay_managed_set_mutation_generation(
        &heap, set, &generation
    ) == MMAY_MANAGED_OK && generation == 2);
    changed = true;
    MMAY_EXPECT(mmay_managed_set_add(
        &heap, set, mmay_test_map_raw(2), &changed
    ) == MMAY_MANAGED_OK && !changed);
    MMAY_EXPECT(mmay_managed_set_mutation_generation(
        &heap, set, &generation
    ) == MMAY_MANAGED_OK && generation == 2);
    mmay_managed_heap_dispose(&heap);
}

void mmay_test_managed_map_set(void) {
    (void)mmay_test_map_raw_field;
    (void)mmay_test_map_string_field;
    mmay_test_managed_map_order_and_mutation();
    mmay_test_managed_map_seed_independent_order();
    mmay_test_managed_set_order_and_duplicates();
    mmay_test_managed_map_string_and_nominal_keys();
    mmay_test_managed_map_gc_graphs();
    mmay_test_managed_map_randomized_gc_graph();
    mmay_test_managed_map_allocation_sweep();
    mmay_test_managed_map_length_limit();
}
