#include "managed_map_probe_internal.h"
#include "managed_map_storage_internal.h"
#include "managed_map_index_internal.h"

/* A single eight-position index fits in one short scalar walk. Avoid group
 * mask construction there; larger indices filter eight controls at once. */
static inline MarimayManagedStatus mmay_map_probe_small(
    const MarimayManagedMap *map, uint64_t hash, bool raw_key, uint64_t key_bits,
    size_t *bucket, size_t *entry, bool *found, MarimayManagedMapProbe *progress
) {
    size_t index = progress != NULL ? progress->next_index : 0;
    size_t first_tombstone = progress != NULL ? progress->first_tombstone : MMAY_MAP_EMPTY;
    const unsigned char *controls = mmay_map_index_controls(map->buckets, 8U);
    for (; index < 8U; index += 1U) {
        size_t position = ((size_t)hash + index) & 7U;
        unsigned char control = controls[position];
        size_t candidate;
        if (control == MMAY_MAP_INDEX_EMPTY) {
            if (map->buckets[position] != MMAY_MAP_EMPTY) return MMAY_MANAGED_INVALID_REFERENCE;
            *bucket = first_tombstone == MMAY_MAP_EMPTY ? position : first_tombstone;
            return MMAY_MANAGED_OK;
        }
        if (control == MMAY_MAP_INDEX_DELETED) {
            if (map->buckets[position] != MMAY_MAP_TOMBSTONE) return MMAY_MANAGED_INVALID_REFERENCE;
            if (first_tombstone == MMAY_MAP_EMPTY) first_tombstone = position;
            continue;
        }
        if (control != (unsigned char)(hash >> 57U)) continue;
        candidate = map->buckets[position];
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
    if (first_tombstone == MMAY_MAP_EMPTY) return MMAY_MANAGED_LIMIT_REACHED;
    *bucket = first_tombstone;
    return MMAY_MANAGED_OK;
}

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
    const unsigned char *controls;
    *bucket = MMAY_MAP_EMPTY;
    *entry = MMAY_MAP_EMPTY;
    *found = false;
    if (map == NULL || map->bucket_capacity < 8U || map->buckets == NULL
        || (map->bucket_capacity & (map->bucket_capacity - 1U)) != 0) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (map->bucket_capacity == 8U) {
        return mmay_map_probe_small(map, hash, raw_key, key_bits, bucket, entry, found, progress);
    }
    controls = mmay_map_index_controls(map->buckets, map->bucket_capacity);
    for (; index < map->bucket_capacity; index += 8U) {
        size_t position = ((size_t)hash + index) & (map->bucket_capacity - 1U);
        size_t remaining = map->bucket_capacity - index;
        uint64_t group = mmay_map_index_load(controls + position);
        uint64_t deleted = mmay_map_index_match(group, MMAY_MAP_INDEX_DELETED);
        uint64_t candidates = mmay_map_index_match(group, (unsigned char)(hash >> 57U))
            | mmay_map_index_match(group, MMAY_MAP_INDEX_EMPTY)
            | (first_tombstone == MMAY_MAP_EMPTY ? deleted : 0);
        /* A reference-key continuation can start partway through the final
         * group. Do not revisit the beginning after the capacity-bound walk. */
        if (remaining < 8U) {
            candidates &= (UINT64_C(1) << (remaining * 8U)) - 1U;
        }
        while (candidates != 0) {
            size_t offset = mmay_map_index_first(candidates);
            size_t selected = (position + offset) & (map->bucket_capacity - 1U);
            unsigned char control = controls[selected];
            size_t candidate;
            candidates &= candidates - 1U;
            if (control == MMAY_MAP_INDEX_EMPTY) {
                if (map->buckets[selected] != MMAY_MAP_EMPTY) {
                    return MMAY_MANAGED_INVALID_REFERENCE;
                }
                *bucket = first_tombstone == MMAY_MAP_EMPTY ? selected : first_tombstone;
                return MMAY_MANAGED_OK;
            }
            if (control == MMAY_MAP_INDEX_DELETED) {
                if (map->buckets[selected] != MMAY_MAP_TOMBSTONE) {
                    return MMAY_MANAGED_INVALID_REFERENCE;
                }
                if (first_tombstone == MMAY_MAP_EMPTY) first_tombstone = selected;
                candidates &= ~deleted;
                continue;
            }
            candidate = map->buckets[selected];
            if (candidate >= map->entry_count || map->entries == NULL
                || !mmay_managed_map_entry_prefix(map, candidate)->active) {
                return MMAY_MANAGED_INVALID_REFERENCE;
            }
            if (mmay_managed_map_entry_prefix(map, candidate)->hash == hash
                && (!raw_key || mmay_managed_map_raw_key(map, candidate) == key_bits)) {
                if (progress != NULL) {
                    progress->next_index = index + offset + 1U;
                    progress->first_tombstone = first_tombstone;
                }
                *bucket = selected;
                *entry = candidate;
                *found = true;
                return MMAY_MANAGED_OK;
            }
        }
    }
    if (first_tombstone != MMAY_MAP_EMPTY) {
        *bucket = first_tombstone;
        return MMAY_MANAGED_OK;
    }
    return MMAY_MANAGED_LIMIT_REACHED;
}
