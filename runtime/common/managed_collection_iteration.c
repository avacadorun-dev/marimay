#include "mmay/runtime/collection_iteration.h"

#include "managed_map_storage_internal.h"

#include <string.h>

static MarimayManagedStatus mmay_collection_iteration_resolve(
    const MarimayManagedHeap *heap, MarimayManagedReference collection,
    bool is_set, const MarimayManagedMap **map
) {
    MarimayManagedStatus status = mmay_managed_map_resolve(heap, collection, map);
    if (status != MMAY_MANAGED_OK) return status;
    if ((*map)->descriptor->is_set != is_set) return MMAY_MANAGED_TYPE_MISMATCH;
    if ((*map)->entry_count > (*map)->entry_capacity
        || ((*map)->entry_count != 0 && (*map)->entries == NULL)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_collection_next(
    const MarimayManagedHeap *heap, MarimayManagedReference collection,
    bool is_set, size_t cursor, size_t *next
) {
    const MarimayManagedMap *map = NULL;
    MarimayManagedStatus status;
    if (next == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    *next = 0;
    status = mmay_collection_iteration_resolve(heap, collection, is_set, &map);
    if (status != MMAY_MANAGED_OK) return status;
    if (cursor > map->entry_count) return MMAY_MANAGED_BOUNDS;
    for (; cursor < map->entry_count; cursor += 1U) {
        if (mmay_managed_map_entry_prefix(map, cursor)->active) {
            *next = cursor + 1U;
            return MMAY_MANAGED_OK;
        }
    }
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_collection_cursor_entry(
    const MarimayManagedHeap *heap, MarimayManagedReference collection,
    bool is_set, size_t cursor, MarimayManagedCell *key, MarimayManagedCell *value
) {
    const MarimayManagedMap *map = NULL;
    MarimayManagedStatus status;
    if (key != NULL) memset(key, 0, sizeof(*key));
    if (value != NULL) memset(value, 0, sizeof(*value));
    if (key == NULL || value == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    status = mmay_collection_iteration_resolve(heap, collection, is_set, &map);
    if (status != MMAY_MANAGED_OK) return status;
    if (cursor == 0 || cursor > map->entry_count
        || !mmay_managed_map_entry_prefix(map, cursor - 1U)->active) {
        return MMAY_MANAGED_BOUNDS;
    }
    *key = mmay_managed_map_entry_key(map, cursor - 1U);
    if (!is_set) *value = mmay_managed_map_entry_value(map, cursor - 1U);
    return MMAY_MANAGED_OK;
}
