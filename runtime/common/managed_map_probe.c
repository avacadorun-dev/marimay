#include "managed_map_probe_internal.h"
#include "managed_map_storage_internal.h"

MarimayManagedStatus mmay_managed_map_probe(
    const MarimayManagedMap *map,
    uint64_t hash,
    bool raw_key,
    uint64_t key_bits,
    size_t *bucket,
    size_t *entry,
    bool *found,
    MarimayManagedMapProbe *progress
) {
    size_t index = progress != NULL ? progress->next_index : 0;
    size_t first_tombstone = progress != NULL
        ? progress->first_tombstone : MMAY_MAP_EMPTY;
    *bucket = MMAY_MAP_EMPTY;
    *entry = MMAY_MAP_EMPTY;
    if (map == NULL || map->bucket_capacity == 0 || map->buckets == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *found = false;
    for (; index < map->bucket_capacity; index += 1) {
        const size_t position = ((size_t)hash + index)
            & (map->bucket_capacity - 1U);
        const size_t candidate = map->buckets[position];
        if (candidate == MMAY_MAP_EMPTY) {
            *bucket = first_tombstone != MMAY_MAP_EMPTY
                ? first_tombstone : position;
            return MMAY_MANAGED_OK;
        }
        if (candidate == MMAY_MAP_TOMBSTONE) {
            if (first_tombstone == MMAY_MAP_EMPTY) first_tombstone = position;
            continue;
        }
        if (candidate >= map->entry_count || map->entries == NULL
            || !mmay_managed_map_entry_prefix(map, candidate)->active) {
            return MMAY_MANAGED_INVALID_REFERENCE;
        }
        if (mmay_managed_map_entry_prefix(map, candidate)->hash == hash
            && (!raw_key || mmay_managed_map_raw_key(map, candidate) == key_bits)) {
            if (progress != NULL) {
                progress->next_index = index + 1U;
                progress->first_tombstone = first_tombstone;
            }
            *bucket = position;
            *entry = candidate;
            *found = true;
            return MMAY_MANAGED_OK;
        }
    }
    if (first_tombstone != MMAY_MAP_EMPTY) {
        *bucket = first_tombstone;
        return MMAY_MANAGED_OK;
    }
    return MMAY_MANAGED_LIMIT_REACHED;
}
