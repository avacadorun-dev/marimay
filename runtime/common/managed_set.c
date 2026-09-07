#include "managed_internal.h"
#include "managed_map_set_internal.h"

#include <string.h>

static MarimayManagedStatus mmay_managed_set_require(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    const MarimayManagedSlot *slot = mmay_managed_resolve_const(heap, reference);
    return slot == NULL ? MMAY_MANAGED_INVALID_REFERENCE
        : slot->kind != MMAY_MANAGED_OBJECT_MAP || slot->as.map == NULL
            || slot->as.map->descriptor == NULL
            || !slot->as.map->descriptor->is_set
            ? MMAY_MANAGED_TYPE_MISMATCH : MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_set_new(
    MarimayManagedHeap *heap,
    const MarimayManagedMapDescriptor *descriptor,
    MarimayManagedReference *result
) {
    MarimayManagedMap *set = NULL;
    MarimayManagedStatus status;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL
        || !mmay_managed_map_descriptor_valid(descriptor)
        || !descriptor->is_set) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    set = mmay_allocate(&heap->allocator, sizeof(*set));
    if (set == NULL) {
        return mmay_managed_resource_host_failure(heap, sizeof(*set));
    }
    memset(set, 0, sizeof(*set));
    set->descriptor = descriptor;
    status = mmay_managed_publish_object(
        heap, MMAY_MANAGED_OBJECT_MAP, set, sizeof(*set),
        NULL, 0, NULL, 0, result
    );
    if (status != MMAY_MANAGED_OK) {
        mmay_managed_map_dispose(heap, set);
    }
    return status;
}

MarimayManagedStatus mmay_managed_set_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    size_t *length
) {
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_length(heap, set, length) : status;
}

MarimayManagedStatus mmay_managed_set_contains(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    MarimayManagedCell value,
    bool *contains
) {
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_contains(heap, set, value, contains) : status;
}

MarimayManagedStatus mmay_managed_set_add(
    MarimayManagedHeap *heap,
    MarimayManagedReference set,
    MarimayManagedCell value,
    bool *inserted
) {
    return mmay_managed_map_add_set(heap, set, value, inserted);
}

MarimayManagedStatus mmay_managed_set_remove(
    MarimayManagedHeap *heap,
    MarimayManagedReference set,
    MarimayManagedCell value,
    bool *removed
) {
    MarimayManagedCell ignored;
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    memset(&ignored, 0, sizeof(ignored));
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_remove(heap, set, value, &ignored, removed) : status;
}

MarimayManagedStatus mmay_managed_set_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference set
) {
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_clear(heap, set) : status;
}

MarimayManagedStatus mmay_managed_set_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    uint64_t *generation
) {
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_mutation_generation(heap, set, generation) : status;
}

MarimayManagedStatus mmay_managed_set_check_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    uint64_t expected_generation
) {
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_check_mutation_generation(
            heap, set, expected_generation
        ) : status;
}

MarimayManagedStatus mmay_managed_set_entry(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    size_t ordinal,
    MarimayManagedCell *value
) {
    const MarimayManagedSlot *slot = mmay_managed_resolve_const(heap, set);
    const MarimayManagedMap *map = slot != NULL ? slot->as.map : NULL;
    size_t index = 0;
    size_t active = 0;
    MarimayManagedStatus status = mmay_managed_set_require(heap, set);
    if (value != NULL) {
        memset(value, 0, sizeof(*value));
    }
    if (status != MMAY_MANAGED_OK || value == NULL) {
        return value == NULL ? MMAY_MANAGED_INVALID_ARGUMENT : status;
    }
    if (ordinal >= map->length) {
        return MMAY_MANAGED_BOUNDS;
    }
    for (index = 0; index < map->entry_count; index += 1) {
        if (!map->entries[index].active) {
            continue;
        }
        if (active == ordinal) {
            *value = map->entries[index].key;
            return MMAY_MANAGED_OK;
        }
        active += 1U;
    }
    return MMAY_MANAGED_INVALID_REFERENCE;
}
