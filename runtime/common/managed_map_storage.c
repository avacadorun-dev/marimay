#include "managed_internal.h"
#include "managed_map_storage_internal.h"
#include "managed_map_probe_internal.h"

static bool mmay_managed_map_storage_sizes(
    size_t entry_stride,
    size_t entry_capacity,
    size_t bucket_capacity,
    size_t *entry_bytes,
    size_t *bucket_bytes,
    size_t *total
) {
    return mmay_size_multiply(
            entry_capacity, entry_stride, entry_bytes
        ) && mmay_size_multiply(
            bucket_capacity, sizeof(size_t), bucket_bytes
        ) && mmay_size_add(*entry_bytes, *bucket_bytes, total);
}

size_t mmay_managed_map_storage_bytes(const MarimayManagedMap *map) {
    size_t entry_bytes = 0;
    size_t bucket_bytes = 0;
    size_t total = 0;
    return map != NULL && mmay_managed_map_storage_sizes(
            map->entry_stride, map->entry_capacity, map->bucket_capacity,
            &entry_bytes, &bucket_bytes, &total
        )
        ? total : 0;
}

void mmay_managed_map_dispose(
    MarimayManagedHeap *heap,
    MarimayManagedMap *map
) {
    size_t entry_bytes = 0;
    size_t bucket_bytes = 0;
    size_t total = 0;
    if (heap == NULL || map == NULL) {
        return;
    }
    (void)mmay_managed_map_storage_sizes(
        map->entry_stride, map->entry_capacity, map->bucket_capacity,
        &entry_bytes, &bucket_bytes, &total
    );
    (void)total;
    mmay_deallocate(&heap->allocator, map->entries, entry_bytes);
    mmay_deallocate(&heap->allocator, map->buckets, bucket_bytes);
    mmay_deallocate(&heap->allocator, map, sizeof(*map));
}

MarimayManagedStatus mmay_managed_map_trace(
    MarimayManagedHeap *heap,
    const MarimayManagedMap *map,
    uint32_t *gray_head
) {
    size_t index = 0;
    if (!mmay_managed_heap_valid(heap) || map == NULL || gray_head == NULL
        || !mmay_managed_map_descriptor_valid(map->descriptor)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    for (index = 0; index < map->entry_count; index += 1) {
        MarimayManagedStatus status;
        if (!mmay_managed_map_entry_prefix(map, index)->active) {
            continue;
        }
        if (map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
            status = mmay_managed_mark_reference(
                heap, mmay_managed_map_entry_key(map, index).reference, gray_head
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
        }
        if (!map->descriptor->is_set
            && map->descriptor->value.cell_kind
                == MMAY_MANAGED_CELL_REFERENCE) {
            status = mmay_managed_mark_reference(
                heap, mmay_managed_map_entry_value(map, index).reference, gray_head
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
        }
    }
    return MMAY_MANAGED_OK;
}

static size_t mmay_managed_map_temporary_roots(
    const MarimayManagedMap *map,
    MarimayManagedReference map_reference,
    const MarimayManagedCell *key,
    const MarimayManagedCell *value,
    MarimayManagedReference roots[3]
) {
    size_t count = 1;
    roots[0] = map_reference;
    if (key != NULL && map->descriptor->key.cell_kind
        == MMAY_MANAGED_CELL_REFERENCE) {
        roots[count++] = key->reference;
    }
    if (value != NULL && !map->descriptor->is_set
        && map->descriptor->value.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
        roots[count++] = value->reference;
    }
    return count;
}

static bool mmay_managed_map_plan_storage(
    const MarimayManagedMap *map,
    size_t *entry_capacity,
    size_t *bucket_capacity
) {
    size_t entries = map->entry_capacity == 0 ? 4U : map->entry_capacity;
    size_t buckets = map->bucket_capacity == 0 ? 8U : map->bucket_capacity;
    size_t required = 0;
    bool compact = map->entry_count >= entries && map->length < map->entry_count;
    if (map->length == SIZE_MAX) {
        return false;
    }
    required = map->length + 1U;
    if (!compact && map->entry_count >= entries) {
        if (!mmay_size_multiply(entries, 2U, &entries)) {
            return false;
        }
    }
    while (map->length + 1U > (buckets / 4U) * 3U) {
        if (!mmay_size_multiply(buckets, 2U, &buckets)) {
            return false;
        }
    }
    *entry_capacity = entries;
    *bucket_capacity = buckets;
    return entries >= required && buckets >= 8U
        && (buckets & (buckets - 1U)) == 0;
}

static void mmay_managed_map_rebuild_buckets(
    const MarimayManagedMap *map,
    size_t *buckets,
    size_t bucket_capacity
) {
    size_t index = 0;
    for (index = 0; index < bucket_capacity; index += 1) {
        buckets[index] = MMAY_MAP_EMPTY;
    }
    for (index = 0; index < map->entry_count; index += 1) {
        size_t probe = 0;
        if (!mmay_managed_map_entry_prefix(map, index)->active) {
            continue;
        }
        for (probe = 0; probe < bucket_capacity; probe += 1) {
            size_t position = ((size_t)mmay_managed_map_entry_prefix(map, index)->hash + probe)
                & (bucket_capacity - 1U);
            if (buckets[position] == MMAY_MAP_EMPTY) {
                buckets[position] = index;
                break;
            }
        }
    }
}

MarimayManagedStatus mmay_managed_map_ensure_storage(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedCell *key,
    const MarimayManagedCell *value,
    MarimayManagedMap **result
) {
    MarimayManagedMap *map = NULL;
    unsigned char *entries = NULL;
    size_t *buckets = NULL;
    size_t entry_capacity = 0;
    size_t bucket_capacity = 0;
    size_t old_entry_bytes = 0;
    size_t old_bucket_bytes = 0;
    size_t old_total = 0;
    size_t entry_bytes = 0;
    size_t bucket_bytes = 0;
    size_t total = 0;
    size_t added = 0;
    size_t next_live = 0;
    size_t index = 0;
    size_t written = 0;
    MarimayManagedReference roots[3];
    size_t root_count = 0;
    MarimayManagedStatus status = mmay_managed_map_resolve_mutable(
        heap, reference, &map
    );
    bool collected = false;
    if (result != NULL) {
        *result = NULL;
    }
    if (status != MMAY_MANAGED_OK || result == NULL) {
        return result == NULL ? MMAY_MANAGED_INVALID_ARGUMENT : status;
    }
retry:
    if (!mmay_managed_map_plan_storage(
            map, &entry_capacity, &bucket_capacity
        ) || !mmay_managed_map_storage_sizes(
            map->entry_stride, map->entry_capacity, map->bucket_capacity,
            &old_entry_bytes, &old_bucket_bytes, &old_total
        ) || !mmay_managed_map_storage_sizes(
            map->entry_stride, entry_capacity, bucket_capacity,
            &entry_bytes, &bucket_bytes, &total
        ) || total < old_total) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    if (map->entry_count < map->entry_capacity
        && map->length + 1U <= (map->bucket_capacity / 4U) * 3U) {
        *result = map;
        return MMAY_MANAGED_OK;
    }
    root_count = mmay_managed_map_temporary_roots(
        map, reference, key, value, roots
    );
    if (heap->options.collect_before_allocation && !collected) {
        status = mmay_managed_collect_internal(
            heap, roots, root_count, NULL, 0
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        collected = true;
        status = mmay_managed_map_resolve_mutable(heap, reference, &map);
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        goto retry;
    }
    added = total - old_total;
    if (!mmay_managed_add_within_limit(
            heap->live_byte_count, added,
            heap->options.max_live_bytes, &next_live
        )) {
        if (!collected) {
            status = mmay_managed_collect_internal(
                heap, roots, root_count, NULL, 0
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
            collected = true;
            status = mmay_managed_map_resolve_mutable(heap, reference, &map);
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
            goto retry;
        }
        return mmay_managed_resource_budget_failure(heap, added);
    }
    entries = mmay_allocate(&heap->allocator, entry_bytes);
    if (entries == NULL) {
        return mmay_managed_resource_host_failure(heap, entry_bytes);
    }
    memset(entries, 0, entry_bytes);
    buckets = mmay_allocate(&heap->allocator, bucket_bytes);
    if (buckets == NULL) {
        mmay_deallocate(&heap->allocator, entries, entry_bytes);
        return mmay_managed_resource_host_failure(heap, bucket_bytes);
    }
    for (index = 0; index < map->entry_count; index += 1) {
        if (mmay_managed_map_entry_prefix(map, index)->active) {
            memcpy(entries + written * map->entry_stride,
                mmay_managed_map_entry_prefix(map, index), map->entry_stride);
            written += 1U;
        }
    }
    {
        MarimayManagedMap rebuilt = *map;
        rebuilt.entries = entries;
        rebuilt.entry_count = written;
        mmay_managed_map_rebuild_buckets(&rebuilt, buckets, bucket_capacity);
    }
    mmay_deallocate(&heap->allocator, map->entries, old_entry_bytes);
    mmay_deallocate(&heap->allocator, map->buckets, old_bucket_bytes);
    map->entries = entries;
    map->buckets = buckets;
    map->entry_capacity = entry_capacity;
    map->bucket_capacity = bucket_capacity;
    map->entry_count = written;
    map->lookup.valid = false;
    heap->live_byte_count = next_live;
    *result = map;
    return MMAY_MANAGED_OK;
}
