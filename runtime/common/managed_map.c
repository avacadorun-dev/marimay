#include "managed_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/hash.h"

#define MMAY_MAP_EMPTY SIZE_MAX
#define MMAY_MAP_TOMBSTONE (SIZE_MAX - 1U)
#define MMAY_MAP_MAX_KEY_DEPTH 64U

static bool mmay_managed_map_field_valid(
    const MarimayManagedFieldDescriptor *field,
    bool key
) {
    if (field == NULL || field->is_mutable) {
        return false;
    }
    if (field->cell_kind == MMAY_MANAGED_CELL_RAW64) {
        return field->reference_kind == MMAY_MANAGED_OBJECT_INVALID
            && field->reference_descriptor == NULL;
    }
    if (field->cell_kind != MMAY_MANAGED_CELL_REFERENCE
        || field->reference_kind == MMAY_MANAGED_OBJECT_INVALID) {
        return false;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_STRING) {
        return field->reference_descriptor == NULL;
    }
    if (key && field->reference_kind != MMAY_MANAGED_OBJECT_RECORD
        && field->reference_kind != MMAY_MANAGED_OBJECT_ENUM) {
        return false;
    }
    return field->reference_descriptor != NULL;
}

static bool mmay_managed_map_descriptor_valid(
    const MarimayManagedMapDescriptor *descriptor
) {
    if (descriptor == NULL
        || !mmay_managed_map_field_valid(&descriptor->key, true)) {
        return false;
    }
    if (descriptor->is_set) {
        return descriptor->value.cell_kind == MMAY_MANAGED_CELL_INVALID
            && descriptor->value.reference_kind == MMAY_MANAGED_OBJECT_INVALID
            && descriptor->value.reference_descriptor == NULL;
    }
    return mmay_managed_map_field_valid(&descriptor->value, false);
}

static MarimayManagedStatus mmay_managed_map_resolve(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedMap **result
) {
    const MarimayManagedSlot *slot = NULL;
    if (result != NULL) {
        *result = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_MAP || slot->as.map == NULL
        || !mmay_managed_map_descriptor_valid(slot->as.map->descriptor)) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *result = slot->as.map;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_map_resolve_mutable(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedMap **result
) {
    const MarimayManagedMap *resolved = NULL;
    MarimayManagedStatus status = mmay_managed_map_resolve(
        heap, reference, &resolved
    );
    if (result != NULL) {
        *result = status == MMAY_MANAGED_OK ? (MarimayManagedMap *)resolved : NULL;
    }
    return result == NULL ? MMAY_MANAGED_INVALID_ARGUMENT : status;
}

static uint64_t mmay_managed_map_hash_words(
    uint64_t first,
    uint64_t second,
    uint64_t seed0,
    uint64_t seed1
) {
    uint64_t words[2];
    words[0] = first;
    words[1] = second;
    return mmay_hash_sip24(words, sizeof(words), seed0, seed1);
}

static MarimayManagedStatus mmay_managed_map_value_hash(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedCell value,
    size_t depth,
    uint64_t *result
);

static MarimayManagedStatus mmay_managed_map_record_hash(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedReference reference,
    size_t depth,
    uint64_t *result
) {
    const MarimayManagedSlot *slot = mmay_managed_resolve_const(heap, reference);
    const MarimayManagedRecord *record = slot != NULL ? slot->as.record : NULL;
    size_t index = 0;
    uint64_t hash = mmay_managed_map_hash_words(
        UINT64_C(0x7265636f72640000),
        record != NULL && record->descriptor != NULL
            ? (uint64_t)record->descriptor->field_count : 0,
        heap->options.collection_hash_seed0,
        heap->options.collection_hash_seed1
    );
    if (slot == NULL || slot->kind != MMAY_MANAGED_OBJECT_RECORD
        || record == NULL || record->descriptor != field->reference_descriptor) {
        return slot == NULL
            ? MMAY_MANAGED_INVALID_REFERENCE : MMAY_MANAGED_TYPE_MISMATCH;
    }
    for (index = 0; index < record->descriptor->field_count; index += 1) {
        uint64_t child = 0;
        MarimayManagedStatus status = mmay_managed_map_value_hash(
            heap,
            &record->descriptor->fields[index],
            record->fields[index],
            depth + 1U,
            &child
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        hash = mmay_managed_map_hash_words(
            hash, child,
            heap->options.collection_hash_seed0,
            heap->options.collection_hash_seed1
        );
    }
    *result = hash;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_map_enum_parts(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedEnumDescriptor **descriptor,
    size_t *variant,
    const MarimayManagedCell **payload,
    MarimayManagedCell *immediate
) {
    const MarimayManagedSlot *slot = NULL;
    uint64_t bits = 0;
    if (descriptor == NULL || variant == NULL || payload == NULL
        || immediate == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *payload = NULL;
    if (mmay_managed_immediate_enum_decode(
            heap, reference, descriptor, variant, &bits
        )) {
        immediate->f64_bits = bits;
        if (*variant >= (*descriptor)->variant_count) {
            return MMAY_MANAGED_INVALID_REFERENCE;
        }
        if ((*descriptor)->variants[*variant].payload_count == 1U) {
            *payload = immediate;
        }
        return MMAY_MANAGED_OK;
    }
    slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ENUM || slot->as.enumeration == NULL
        || slot->as.enumeration->descriptor == NULL
        || slot->as.enumeration->variant
            >= slot->as.enumeration->descriptor->variant_count) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.enumeration->descriptor;
    *variant = slot->as.enumeration->variant;
    *payload = slot->as.enumeration->payload;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_map_enum_hash(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedReference reference,
    size_t depth,
    uint64_t *result
) {
    const MarimayManagedEnumDescriptor *descriptor = NULL;
    const MarimayManagedEnumVariantDescriptor *selected = NULL;
    const MarimayManagedCell *payload = NULL;
    MarimayManagedCell immediate;
    size_t variant = 0;
    size_t index = 0;
    uint64_t hash = 0;
    MarimayManagedStatus status = mmay_managed_map_enum_parts(
        heap, reference, &descriptor, &variant, &payload, &immediate
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (descriptor != field->reference_descriptor) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    selected = &descriptor->variants[variant];
    if ((payload == NULL && selected->payload_count != 0)
        || (selected->payloads == NULL && selected->payload_count != 0)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    hash = mmay_managed_map_hash_words(
        UINT64_C(0x656e756d00000000), (uint64_t)variant,
        heap->options.collection_hash_seed0,
        heap->options.collection_hash_seed1
    );
    for (index = 0; index < selected->payload_count; index += 1) {
        uint64_t child = 0;
        status = mmay_managed_map_value_hash(
            heap, &selected->payloads[index], payload[index], depth + 1U,
            &child
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        hash = mmay_managed_map_hash_words(
            hash, child,
            heap->options.collection_hash_seed0,
            heap->options.collection_hash_seed1
        );
    }
    *result = hash;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_map_value_hash(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedCell value,
    size_t depth,
    uint64_t *result
) {
    const MarimayManagedSlot *slot = NULL;
    if (result != NULL) {
        *result = 0;
    }
    if (!mmay_managed_heap_valid(heap) || field == NULL || result == NULL
        || depth > MMAY_MAP_MAX_KEY_DEPTH || field->is_mutable) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind == MMAY_MANAGED_CELL_RAW64) {
        if (!mmay_managed_map_field_valid(field, true)) {
            return MMAY_MANAGED_INVALID_ARGUMENT;
        }
        *result = mmay_hash_sip24(
            &value.f64_bits, sizeof(value.f64_bits),
            heap->options.collection_hash_seed0,
            heap->options.collection_hash_seed1
        );
        return MMAY_MANAGED_OK;
    }
    if (field->cell_kind != MMAY_MANAGED_CELL_REFERENCE) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_ENUM) {
        return mmay_managed_map_enum_hash(
            heap, field, value.reference, depth, result
        );
    }
    slot = mmay_managed_resolve_const(heap, value.reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_STRING) {
        if (slot->kind != MMAY_MANAGED_OBJECT_STRING || slot->as.string == NULL
            || field->reference_descriptor != NULL) {
            return MMAY_MANAGED_TYPE_MISMATCH;
        }
        *result = mmay_hash_sip24(
            slot->as.string->bytes, slot->as.string->length,
            heap->options.collection_hash_seed0,
            heap->options.collection_hash_seed1
        );
        return MMAY_MANAGED_OK;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_RECORD) {
        return mmay_managed_map_record_hash(
            heap, field, value.reference, depth, result
        );
    }
    return MMAY_MANAGED_INVALID_ARGUMENT;
}

static MarimayManagedStatus mmay_managed_map_value_equal(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedCell left,
    MarimayManagedCell right,
    size_t depth,
    bool *result
);

static MarimayManagedStatus mmay_managed_map_record_equal(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedReference left,
    MarimayManagedReference right,
    size_t depth,
    bool *result
) {
    const MarimayManagedSlot *left_slot = mmay_managed_resolve_const(heap, left);
    const MarimayManagedSlot *right_slot = mmay_managed_resolve_const(heap, right);
    const MarimayManagedRecord *left_record = left_slot != NULL
        ? left_slot->as.record : NULL;
    const MarimayManagedRecord *right_record = right_slot != NULL
        ? right_slot->as.record : NULL;
    size_t index = 0;
    if (left_slot == NULL || right_slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (left_slot->kind != MMAY_MANAGED_OBJECT_RECORD
        || right_slot->kind != MMAY_MANAGED_OBJECT_RECORD
        || left_record == NULL || right_record == NULL
        || left_record->descriptor != field->reference_descriptor
        || right_record->descriptor != field->reference_descriptor) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    for (index = 0; index < left_record->descriptor->field_count; index += 1) {
        bool equal = false;
        MarimayManagedStatus status = mmay_managed_map_value_equal(
            heap,
            &left_record->descriptor->fields[index],
            left_record->fields[index],
            right_record->fields[index],
            depth + 1U,
            &equal
        );
        if (status != MMAY_MANAGED_OK || !equal) {
            *result = false;
            return status;
        }
    }
    *result = true;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_map_enum_equal(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedReference left,
    MarimayManagedReference right,
    size_t depth,
    bool *result
) {
    const MarimayManagedEnumDescriptor *left_descriptor = NULL;
    const MarimayManagedEnumDescriptor *right_descriptor = NULL;
    const MarimayManagedCell *left_payload = NULL;
    const MarimayManagedCell *right_payload = NULL;
    MarimayManagedCell left_immediate;
    MarimayManagedCell right_immediate;
    size_t left_variant = 0;
    size_t right_variant = 0;
    size_t index = 0;
    MarimayManagedStatus status = mmay_managed_map_enum_parts(
        heap, left, &left_descriptor, &left_variant, &left_payload,
        &left_immediate
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_map_enum_parts(
        heap, right, &right_descriptor, &right_variant, &right_payload,
        &right_immediate
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (left_descriptor != field->reference_descriptor
        || right_descriptor != field->reference_descriptor) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (left_variant != right_variant) {
        *result = false;
        return MMAY_MANAGED_OK;
    }
    for (index = 0;
        index < left_descriptor->variants[left_variant].payload_count;
        index += 1) {
        bool equal = false;
        status = mmay_managed_map_value_equal(
            heap,
            &left_descriptor->variants[left_variant].payloads[index],
            left_payload[index],
            right_payload[index],
            depth + 1U,
            &equal
        );
        if (status != MMAY_MANAGED_OK || !equal) {
            *result = false;
            return status;
        }
    }
    *result = true;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_map_value_equal(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedCell left,
    MarimayManagedCell right,
    size_t depth,
    bool *result
) {
    const MarimayManagedSlot *left_slot = NULL;
    const MarimayManagedSlot *right_slot = NULL;
    if (result != NULL) {
        *result = false;
    }
    if (!mmay_managed_heap_valid(heap) || field == NULL || result == NULL
        || depth > MMAY_MAP_MAX_KEY_DEPTH || field->is_mutable) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind == MMAY_MANAGED_CELL_RAW64) {
        if (!mmay_managed_map_field_valid(field, true)) {
            return MMAY_MANAGED_INVALID_ARGUMENT;
        }
        *result = left.f64_bits == right.f64_bits;
        return MMAY_MANAGED_OK;
    }
    if (field->cell_kind != MMAY_MANAGED_CELL_REFERENCE) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_ENUM) {
        return mmay_managed_map_enum_equal(
            heap, field, left.reference, right.reference, depth, result
        );
    }
    left_slot = mmay_managed_resolve_const(heap, left.reference);
    right_slot = mmay_managed_resolve_const(heap, right.reference);
    if (left_slot == NULL || right_slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_STRING) {
        if (left_slot->kind != MMAY_MANAGED_OBJECT_STRING
            || right_slot->kind != MMAY_MANAGED_OBJECT_STRING
            || left_slot->as.string == NULL || right_slot->as.string == NULL
            || field->reference_descriptor != NULL) {
            return MMAY_MANAGED_TYPE_MISMATCH;
        }
        *result = left_slot->as.string->length == right_slot->as.string->length
            && (left_slot->as.string->length == 0
                || memcmp(
                    left_slot->as.string->bytes,
                    right_slot->as.string->bytes,
                    left_slot->as.string->length
                ) == 0);
        return MMAY_MANAGED_OK;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_RECORD) {
        return mmay_managed_map_record_equal(
            heap, field, left.reference, right.reference, depth, result
        );
    }
    return MMAY_MANAGED_INVALID_ARGUMENT;
}

static MarimayManagedStatus mmay_managed_map_find(
    const MarimayManagedHeap *heap,
    const MarimayManagedMap *map,
    MarimayManagedCell key,
    uint64_t hash,
    size_t *bucket,
    size_t *entry,
    bool *found
) {
    size_t index = 0;
    size_t first_tombstone = MMAY_MAP_EMPTY;
    if (bucket != NULL) {
        *bucket = MMAY_MAP_EMPTY;
    }
    if (entry != NULL) {
        *entry = MMAY_MAP_EMPTY;
    }
    if (map == NULL || bucket == NULL || entry == NULL || found == NULL
        || map->bucket_capacity == 0 || map->buckets == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *found = false;
    for (index = 0; index < map->bucket_capacity; index += 1) {
        size_t position = ((size_t)hash + index) & (map->bucket_capacity - 1U);
        size_t candidate = map->buckets[position];
        if (candidate == MMAY_MAP_EMPTY) {
            *bucket = first_tombstone != MMAY_MAP_EMPTY
                ? first_tombstone : position;
            return MMAY_MANAGED_OK;
        }
        if (candidate == MMAY_MAP_TOMBSTONE) {
            if (first_tombstone == MMAY_MAP_EMPTY) {
                first_tombstone = position;
            }
            continue;
        }
        if (candidate >= map->entry_count || map->entries == NULL
            || !map->entries[candidate].active) {
            return MMAY_MANAGED_INVALID_REFERENCE;
        }
        if (map->entries[candidate].hash == hash) {
            bool equal = false;
            MarimayManagedStatus status = mmay_managed_map_value_equal(
                heap, &map->descriptor->key, map->entries[candidate].key,
                key, 0, &equal
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
            if (equal) {
                *bucket = position;
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

static bool mmay_managed_map_storage_sizes(
    size_t entry_capacity,
    size_t bucket_capacity,
    size_t *entry_bytes,
    size_t *bucket_bytes,
    size_t *total
) {
    return mmay_size_multiply(
            entry_capacity, sizeof(MarimayManagedMapEntry), entry_bytes
        ) && mmay_size_multiply(
            bucket_capacity, sizeof(size_t), bucket_bytes
        ) && mmay_size_add(*entry_bytes, *bucket_bytes, total);
}

size_t mmay_managed_map_storage_bytes(const MarimayManagedMap *map) {
    size_t entry_bytes = 0;
    size_t bucket_bytes = 0;
    size_t total = 0;
    return map != NULL && mmay_managed_map_storage_sizes(
            map->entry_capacity, map->bucket_capacity,
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
        map->entry_capacity, map->bucket_capacity,
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
        if (!map->entries[index].active) {
            continue;
        }
        if (map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
            status = mmay_managed_mark_reference(
                heap, map->entries[index].key.reference, gray_head
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
        }
        if (!map->descriptor->is_set
            && map->descriptor->value.cell_kind
                == MMAY_MANAGED_CELL_REFERENCE) {
            status = mmay_managed_mark_reference(
                heap, map->entries[index].value.reference, gray_head
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
    MarimayManagedMapEntry *entries,
    size_t entry_count,
    size_t *buckets,
    size_t bucket_capacity
) {
    size_t index = 0;
    for (index = 0; index < bucket_capacity; index += 1) {
        buckets[index] = MMAY_MAP_EMPTY;
    }
    for (index = 0; index < entry_count; index += 1) {
        size_t probe = 0;
        if (!entries[index].active) {
            continue;
        }
        for (probe = 0; probe < bucket_capacity; probe += 1) {
            size_t position = ((size_t)entries[index].hash + probe)
                & (bucket_capacity - 1U);
            if (buckets[position] == MMAY_MAP_EMPTY) {
                buckets[position] = index;
                break;
            }
        }
    }
}

static MarimayManagedStatus mmay_managed_map_ensure_storage(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedCell *key,
    const MarimayManagedCell *value,
    MarimayManagedMap **result
) {
    MarimayManagedMap *map = NULL;
    MarimayManagedMapEntry *entries = NULL;
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
            map->entry_capacity, map->bucket_capacity,
            &old_entry_bytes, &old_bucket_bytes, &old_total
        ) || !mmay_managed_map_storage_sizes(
            entry_capacity, bucket_capacity,
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
        if (map->entries[index].active) {
            entries[written++] = map->entries[index];
        }
    }
    mmay_managed_map_rebuild_buckets(
        entries, written, buckets, bucket_capacity
    );
    mmay_deallocate(&heap->allocator, map->entries, old_entry_bytes);
    mmay_deallocate(&heap->allocator, map->buckets, old_bucket_bytes);
    map->entries = entries;
    map->buckets = buckets;
    map->entry_capacity = entry_capacity;
    map->bucket_capacity = bucket_capacity;
    map->entry_count = written;
    heap->live_byte_count = next_live;
    *result = map;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_map_new(
    MarimayManagedHeap *heap,
    const MarimayManagedMapDescriptor *descriptor,
    MarimayManagedReference *result
) {
    MarimayManagedMap *map = NULL;
    MarimayManagedStatus status;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL
        || !mmay_managed_map_descriptor_valid(descriptor)
        || descriptor->is_set) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    map = mmay_allocate(&heap->allocator, sizeof(*map));
    if (map == NULL) {
        return mmay_managed_resource_host_failure(heap, sizeof(*map));
    }
    memset(map, 0, sizeof(*map));
    map->descriptor = descriptor;
    status = mmay_managed_publish_object(
        heap, MMAY_MANAGED_OBJECT_MAP, map, sizeof(*map),
        NULL, 0, NULL, 0, result
    );
    if (status != MMAY_MANAGED_OK) {
        mmay_managed_map_dispose(heap, map);
    }
    return status;
}

MarimayManagedStatus mmay_managed_map_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    size_t *length
) {
    const MarimayManagedMap *map = NULL;
    MarimayManagedStatus status = mmay_managed_map_resolve(
        heap, reference, &map
    );
    if (length != NULL) {
        *length = status == MMAY_MANAGED_OK ? map->length : 0;
    }
    return length == NULL ? MMAY_MANAGED_INVALID_ARGUMENT : status;
}

static MarimayManagedStatus mmay_managed_map_lookup(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    const MarimayManagedMap **map_result,
    size_t *bucket,
    size_t *entry,
    bool *found
) {
    const MarimayManagedMap *map = NULL;
    uint64_t hash = 0;
    MarimayManagedStatus status = mmay_managed_map_resolve(
        heap, reference, &map
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_validate_field(heap, &map->descriptor->key, key);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_map_value_hash(
        heap, &map->descriptor->key, key, 0, &hash
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (map->bucket_capacity == 0) {
        *bucket = MMAY_MAP_EMPTY;
        *entry = MMAY_MAP_EMPTY;
        *found = false;
        *map_result = map;
        return MMAY_MANAGED_OK;
    }
    status = mmay_managed_map_find(
        heap, map, key, hash, bucket, entry, found
    );
    if (status == MMAY_MANAGED_OK) {
        *map_result = map;
    }
    return status;
}

MarimayManagedStatus mmay_managed_map_contains(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    bool *contains
) {
    const MarimayManagedMap *map = NULL;
    size_t bucket = 0;
    size_t entry = 0;
    bool found = false;
    MarimayManagedStatus status;
    if (contains != NULL) {
        *contains = false;
    }
    if (contains == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_map_lookup(
        heap, reference, key, &map, &bucket, &entry, &found
    );
    (void)map;
    (void)bucket;
    (void)entry;
    if (status == MMAY_MANAGED_OK) {
        *contains = found;
    }
    return status;
}

MarimayManagedStatus mmay_managed_map_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    MarimayManagedCell *value,
    bool *found
) {
    const MarimayManagedMap *map = NULL;
    size_t bucket = 0;
    size_t entry = 0;
    MarimayManagedStatus status;
    if (value != NULL) {
        memset(value, 0, sizeof(*value));
    }
    if (found != NULL) {
        *found = false;
    }
    if (value == NULL || found == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_map_lookup(
        heap, reference, key, &map, &bucket, &entry, found
    );
    (void)bucket;
    if (status == MMAY_MANAGED_OK && *found) {
        if (map->descriptor->is_set) {
            return MMAY_MANAGED_TYPE_MISMATCH;
        }
        *value = map->entries[entry].value;
    }
    return status;
}

MarimayManagedStatus mmay_managed_map_put(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    MarimayManagedCell value,
    bool *inserted
) {
    MarimayManagedMap *map = NULL;
    const MarimayManagedMap *lookup_map = NULL;
    size_t bucket = MMAY_MAP_EMPTY;
    size_t entry = MMAY_MAP_EMPTY;
    bool found = false;
    uint64_t hash = 0;
    MarimayManagedStatus status;
    if (inserted != NULL) {
        *inserted = false;
    }
    if (inserted == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_map_lookup(
        heap, reference, key, &lookup_map, &bucket, &entry, &found
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (!lookup_map->descriptor->is_set) {
        status = mmay_managed_validate_field(
            heap, &lookup_map->descriptor->value, value
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    if (found) {
        map = (MarimayManagedMap *)lookup_map;
        if (map->descriptor->is_set) {
            return MMAY_MANAGED_OK;
        }
        if (map->mutation_generation == UINT64_MAX) {
            return MMAY_MANAGED_LIMIT_REACHED;
        }
        map->entries[entry].value = value;
        map->mutation_generation += UINT64_C(1);
        return MMAY_MANAGED_OK;
    }
    if (lookup_map->length >= heap->options.max_collection_length) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    if (lookup_map->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    status = mmay_managed_map_value_hash(
        heap, &lookup_map->descriptor->key, key, 0, &hash
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_map_ensure_storage(
        heap, reference, &key,
        lookup_map->descriptor->is_set ? NULL : &value,
        &map
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_map_find(
        heap, map, key, hash, &bucket, &entry, &found
    );
    if (status != MMAY_MANAGED_OK || found || bucket == MMAY_MAP_EMPTY
        || map->entry_count >= map->entry_capacity) {
        return status != MMAY_MANAGED_OK
            ? status : MMAY_MANAGED_INVALID_REFERENCE;
    }
    entry = map->entry_count;
    map->entries[entry].hash = hash;
    map->entries[entry].key = key;
    map->entries[entry].value = value;
    map->entries[entry].active = true;
    map->buckets[bucket] = entry;
    map->entry_count += 1U;
    map->length += 1U;
    map->mutation_generation += UINT64_C(1);
    *inserted = true;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_map_remove(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    MarimayManagedCell *value,
    bool *removed
) {
    const MarimayManagedMap *lookup_map = NULL;
    MarimayManagedMap *map = NULL;
    size_t bucket = 0;
    size_t entry = 0;
    bool found = false;
    MarimayManagedStatus status;
    if (value != NULL) {
        memset(value, 0, sizeof(*value));
    }
    if (removed != NULL) {
        *removed = false;
    }
    if (value == NULL || removed == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_map_lookup(
        heap, reference, key, &lookup_map, &bucket, &entry, &found
    );
    if (status != MMAY_MANAGED_OK || !found) {
        return status;
    }
    if (lookup_map->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    map = (MarimayManagedMap *)lookup_map;
    if (!map->descriptor->is_set) {
        *value = map->entries[entry].value;
    }
    memset(&map->entries[entry].key, 0, sizeof(map->entries[entry].key));
    memset(&map->entries[entry].value, 0, sizeof(map->entries[entry].value));
    map->entries[entry].active = false;
    map->buckets[bucket] = MMAY_MAP_TOMBSTONE;
    map->length -= 1U;
    map->mutation_generation += UINT64_C(1);
    *removed = true;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_map_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    MarimayManagedMap *map = NULL;
    size_t index = 0;
    MarimayManagedStatus status = mmay_managed_map_resolve_mutable(
        heap, reference, &map
    );
    if (status != MMAY_MANAGED_OK || map->length == 0) {
        return status;
    }
    if (map->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    memset(
        map->entries, 0,
        map->entry_capacity * sizeof(*map->entries)
    );
    for (index = 0; index < map->bucket_capacity; index += 1) {
        map->buckets[index] = MMAY_MAP_EMPTY;
    }
    map->length = 0;
    map->entry_count = 0;
    map->mutation_generation += UINT64_C(1);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_map_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    uint64_t *generation
) {
    const MarimayManagedMap *map = NULL;
    MarimayManagedStatus status = mmay_managed_map_resolve(
        heap, reference, &map
    );
    if (generation != NULL) {
        *generation = status == MMAY_MANAGED_OK
            ? map->mutation_generation : 0;
    }
    return generation == NULL ? MMAY_MANAGED_INVALID_ARGUMENT : status;
}

MarimayManagedStatus mmay_managed_map_check_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    uint64_t expected_generation
) {
    uint64_t generation = 0;
    MarimayManagedStatus status = mmay_managed_map_mutation_generation(
        heap, reference, &generation
    );
    return status != MMAY_MANAGED_OK ? status
        : generation == expected_generation
            ? MMAY_MANAGED_OK : MMAY_MANAGED_ITERATOR_INVALIDATED;
}

MarimayManagedStatus mmay_managed_map_entry(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    size_t ordinal,
    MarimayManagedCell *key,
    MarimayManagedCell *value
) {
    const MarimayManagedMap *map = NULL;
    size_t index = 0;
    size_t active = 0;
    MarimayManagedStatus status = mmay_managed_map_resolve(
        heap, reference, &map
    );
    if (key != NULL) {
        memset(key, 0, sizeof(*key));
    }
    if (value != NULL) {
        memset(value, 0, sizeof(*value));
    }
    if (status != MMAY_MANAGED_OK || key == NULL || value == NULL) {
        return key == NULL || value == NULL
            ? MMAY_MANAGED_INVALID_ARGUMENT : status;
    }
    if (map->descriptor->is_set) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (ordinal >= map->length) {
        return MMAY_MANAGED_BOUNDS;
    }
    for (index = 0; index < map->entry_count; index += 1) {
        if (!map->entries[index].active) {
            continue;
        }
        if (active == ordinal) {
            *key = map->entries[index].key;
            *value = map->entries[index].value;
            return MMAY_MANAGED_OK;
        }
        active += 1U;
    }
    return MMAY_MANAGED_INVALID_REFERENCE;
}
