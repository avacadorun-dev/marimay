#include "managed_internal.h"
#include "managed_map_storage_internal.h"
#include "managed_map_probe_internal.h"
#include "managed_map_index_internal.h"
#include "managed_map_set_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/hash.h"

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
    return (field->reference_kind == MMAY_MANAGED_OBJECT_ARRAY
            || field->reference_kind == MMAY_MANAGED_OBJECT_RECORD
            || field->reference_kind == MMAY_MANAGED_OBJECT_ENUM
            || field->reference_kind == MMAY_MANAGED_OBJECT_INTERFACE
            || field->reference_kind == MMAY_MANAGED_OBJECT_MAP)
        && field->reference_descriptor != NULL;
}

bool mmay_managed_map_descriptor_valid(
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

static uint64_t mmay_managed_map_hash_words(
    uint64_t first,
    uint64_t second,
    uint64_t seed0,
    uint64_t seed1
) {
    uint64_t words[2];
    words[0] = first;
    words[1] = second;
    return mmay_hash_seeded(words, sizeof(words), seed0, seed1);
}

static uint64_t mmay_managed_map_hash_raw64(
    uint64_t value,
    uint64_t seed0,
    uint64_t seed1
) {
    /* Keep both injected seeds; specialize dispatch, not collision behavior. */
    return mmay_hash_seeded_word(value, seed0, seed1);
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
        *result = mmay_managed_map_hash_raw64(
            value.f64_bits,
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
        *result = mmay_hash_seeded(
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

/* One validated, resolved string key for one lookup. The checked entry points
 * return the same status sequence as validation followed by hashing, but the
 * slot is resolved once and then reused by the hash, the probe and the key
 * comparison. Nothing here allocates, so the slot stays valid for the call. */
static MarimayManagedStatus mmay_managed_map_string_key_slot(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedCell key,
    const MarimayManagedSlot **slot_result
) {
    const MarimayManagedSlot *slot = NULL;
    if (slot_result == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *slot_result = NULL;
    if (!mmay_managed_heap_valid(heap) || field == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind != MMAY_MANAGED_CELL_REFERENCE
        || field->reference_kind != MMAY_MANAGED_OBJECT_STRING) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (mmay_managed_reference_is_invalid(key.reference)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    slot = mmay_managed_resolve_const(heap, key.reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_STRING || slot->as.string == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (field->reference_descriptor != NULL || field->is_mutable) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *slot_result = slot;
    return MMAY_MANAGED_OK;
}

static uint64_t mmay_managed_map_string_hash_slot(
    const MarimayManagedHeap *heap,
    const MarimayManagedSlot *slot
) {
    return mmay_hash_seeded(
        slot->as.string->bytes,
        slot->as.string->length,
        heap->options.collection_hash_seed0,
        heap->options.collection_hash_seed1
    );
}

static MarimayManagedStatus mmay_managed_map_string_slot_equal(
    const MarimayManagedSlot *entry_slot,
    const MarimayManagedSlot *key_slot,
    bool *result
) {
    if (entry_slot == NULL || key_slot == NULL || key_slot->as.string == NULL
        || result == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (entry_slot->kind != MMAY_MANAGED_OBJECT_STRING
        || entry_slot->as.string == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *result = entry_slot->as.string->length == key_slot->as.string->length
        && (key_slot->as.string->length == 0
            || memcmp(
                entry_slot->as.string->bytes,
                key_slot->as.string->bytes,
                key_slot->as.string->length
            ) == 0);
    return MMAY_MANAGED_OK;
}

/* `key_slot` carries the already-validated string key, or NULL for every other
 * key shape, which keeps the recursive generic comparison. */
static MarimayManagedStatus mmay_managed_map_find_reference(
    const MarimayManagedHeap *heap,
    const MarimayManagedMap *map,
    MarimayManagedCell key,
    const MarimayManagedSlot *key_slot,
    uint64_t hash,
    size_t *bucket,
    size_t *entry,
    bool *found
) {
    MarimayManagedMapProbe progress = {0, MMAY_MAP_EMPTY};
    for (;;) {
        bool equal = false;
        MarimayManagedStatus status = mmay_managed_map_probe(
            map, hash, false, 0, bucket, entry, found, &progress
        );
        if (status != MMAY_MANAGED_OK || !*found) return status;
        /* This comparison neither allocates nor mutates the map. Only hash
         * candidates enter recursive equality; all keys share one probe. */
        *found = false;
        if (key_slot != NULL) {
            status = mmay_managed_map_string_slot_equal(
                mmay_managed_resolve_const(
                    heap, mmay_managed_map_entry_key(map, *entry).reference
                ),
                key_slot,
                &equal
            );
        } else {
            status = mmay_managed_map_value_equal(
                heap, &map->descriptor->key,
                mmay_managed_map_entry_key(map, *entry), key, 0, &equal
            );
        }
        if (status != MMAY_MANAGED_OK) return status;
        if (equal) {
            *found = true;
            return MMAY_MANAGED_OK;
        }
    }
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
    mmay_managed_map_storage_layout(map, descriptor);
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

static inline MarimayManagedStatus mmay_managed_map_lookup_resolved(
    const MarimayManagedHeap *heap,
    const MarimayManagedMap *map,
    MarimayManagedCell key,
    size_t *bucket,
    size_t *entry,
    bool *found,
    uint64_t *hash_result
) {
    uint64_t hash = 0;
    const MarimayManagedSlot *key_slot = NULL;
    MarimayManagedStatus status;
    if (map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_RAW64) {
        if (!map->descriptor->is_set && map->lookup.valid
            && map->lookup.key_bits == key.f64_bits
            && map->lookup.generation == map->mutation_generation
            && map->lookup.seed0 == heap->options.collection_hash_seed0
            && map->lookup.seed1 == heap->options.collection_hash_seed1) {
            *bucket = map->lookup.bucket;
            *entry = map->lookup.entry;
            *found = map->lookup.found;
            if (hash_result != NULL) *hash_result = map->lookup.hash;
            return MMAY_MANAGED_OK;
        }
        hash = mmay_managed_map_hash_raw64(
            key.f64_bits,
            heap->options.collection_hash_seed0,
            heap->options.collection_hash_seed1
        );
    } else if (map->descriptor->key.reference_kind == MMAY_MANAGED_OBJECT_STRING) {
        status = mmay_managed_map_string_key_slot(
            heap, &map->descriptor->key, key, &key_slot
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        hash = mmay_managed_map_string_hash_slot(heap, key_slot);
    } else {
        status = mmay_managed_validate_field(
            heap, &map->descriptor->key, key
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        status = mmay_managed_map_value_hash(
            heap, &map->descriptor->key, key, 0, &hash
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    if (hash_result != NULL) *hash_result = hash;
    if (map->bucket_capacity == 0) {
        *bucket = MMAY_MAP_EMPTY;
        *entry = MMAY_MAP_EMPTY;
        *found = false;
        return MMAY_MANAGED_OK;
    }
    status = map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_RAW64
        ? mmay_managed_map_probe(
            map, hash, true, key.f64_bits, bucket, entry, found, NULL
        ) : mmay_managed_map_find_reference(
            heap, map, key, key_slot, hash, bucket, entry, found
        );
    return status;
}

static MarimayManagedStatus mmay_managed_map_lookup(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    const MarimayManagedMap **map_result,
    size_t *bucket,
    size_t *entry,
    bool *found,
    uint64_t *hash_result
) {
    const MarimayManagedMap *map = NULL;
    MarimayManagedStatus status = mmay_managed_map_resolve(heap, reference, &map);
    if (status == MMAY_MANAGED_OK) {
        status = mmay_managed_map_lookup_resolved(
            heap, map, key, bucket, entry, found, hash_result
        );
        if (status == MMAY_MANAGED_OK) *map_result = map;
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
        heap, reference, key, &map, &bucket, &entry, &found, NULL
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
    uint64_t hash = 0;
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
        heap, reference, key, &map, &bucket, &entry, found, &hash
    );
    if (status == MMAY_MANAGED_OK && *found) {
        if (map->descriptor->is_set) {
            return MMAY_MANAGED_TYPE_MISMATCH;
        }
        *value = mmay_managed_map_entry_value(map, entry);
    }
    if (status == MMAY_MANAGED_OK && !map->descriptor->is_set
        && map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_RAW64) {
        /* An isolate executes one task at a time; the heap-owned map object
         * is mutable even when this query borrows a const heap. No user state,
         * allocation, reference retention or mutation generation changes. */
        MarimayManagedMap *memoized = (MarimayManagedMap *)map;
        memoized->lookup.key_bits = key.f64_bits;
        memoized->lookup.hash = hash;
        memoized->lookup.generation = map->mutation_generation;
        memoized->lookup.seed0 = heap->options.collection_hash_seed0;
        memoized->lookup.seed1 = heap->options.collection_hash_seed1;
        memoized->lookup.bucket = bucket;
        memoized->lookup.entry = entry;
        memoized->lookup.found = *found;
        memoized->lookup.valid = true;
    }
    return status;
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((always_inline))
#endif
static inline MarimayManagedStatus mmay_managed_map_put_resolved(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedMap *lookup_map,
    MarimayManagedCell key,
    MarimayManagedCell value,
    bool *inserted
) {
    MarimayManagedMap *map = NULL;
    size_t bucket = MMAY_MAP_EMPTY;
    size_t entry = MMAY_MAP_EMPTY;
    bool found = false;
    uint64_t hash = 0;
    size_t previous_buckets = 0;
    size_t previous_entries = 0;
    MarimayManagedStatus status;
    /* Both checked entry points validate the receiver/output first. The
     * borrowed object stays within this owner; growth re-resolves its handle. */
    status = mmay_managed_map_lookup_resolved(
        heap, lookup_map, key, &bucket, &entry, &found, &hash
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (!lookup_map->descriptor->is_set
        && lookup_map->descriptor->value.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
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
        mmay_managed_map_store_value(map, entry, value);
        map->mutation_generation += UINT64_C(1);
        return MMAY_MANAGED_OK;
    }
    if (lookup_map->length >= mmay_managed_language_max_array_length()) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    if (lookup_map->length >= heap->options.max_collection_length) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    if (lookup_map->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    previous_buckets = lookup_map->bucket_capacity;
    previous_entries = lookup_map->entry_count;
    status = mmay_managed_map_ensure_storage(
        heap, reference, &key,
        lookup_map->descriptor->is_set ? NULL : &value,
        &map
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    /* Reuse the absent bucket unless growth or compaction rebuilt it. */
    if (map->bucket_capacity != previous_buckets
        || map->entry_count != previous_entries) {
        const MarimayManagedSlot *growth_slot = NULL;
        if (map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_REFERENCE
            && map->descriptor->key.reference_kind
                == MMAY_MANAGED_OBJECT_STRING) {
            status = mmay_managed_map_string_key_slot(
                heap, &map->descriptor->key, key, &growth_slot
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
        }
        status = map->descriptor->key.cell_kind == MMAY_MANAGED_CELL_RAW64
            ? mmay_managed_map_probe(
                map, hash, true, key.f64_bits, &bucket, &entry, &found, NULL
            ) : mmay_managed_map_find_reference(
                heap, map, key, growth_slot, hash, &bucket, &entry, &found
            );
    }
    if (status != MMAY_MANAGED_OK || found || bucket == MMAY_MAP_EMPTY
        || map->entry_count >= map->entry_capacity) {
        return status != MMAY_MANAGED_OK
            ? status : MMAY_MANAGED_INVALID_REFERENCE;
    }
    entry = map->entry_count;
    mmay_managed_map_entry_prefix(map, entry)->hash = hash;
    mmay_managed_map_store_key(map, entry, key);
    if (!map->descriptor->is_set) mmay_managed_map_store_value(map, entry, value);
    mmay_managed_map_entry_prefix(map, entry)->active = true;
    mmay_map_index_bucket_store(
        map->buckets, map->bucket_capacity, bucket, entry, hash
    );
    map->entry_count += 1U;
    map->length += 1U;
    map->mutation_generation += UINT64_C(1);
    *inserted = true;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_map_put(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    MarimayManagedCell value,
    bool *inserted
) {
    const MarimayManagedMap *map = NULL;
    MarimayManagedStatus status;
    if (inserted == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    *inserted = false;
    status = mmay_managed_map_resolve(heap, reference, &map);
    return status == MMAY_MANAGED_OK
        ? mmay_managed_map_put_resolved(heap, reference, map, key, value, inserted)
        : status;
}

MarimayManagedStatus mmay_managed_map_add_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedCell key,
    bool *inserted
) {
    const MarimayManagedSlot *slot = mmay_managed_resolve_const(heap, reference);
    MarimayManagedCell ignored = {0};
    /* Preserve Set's receiver-before-output failure order without resolving
     * the same handle again in Map.put. No resolved storage crosses owners. */
    if (slot == NULL) return MMAY_MANAGED_INVALID_REFERENCE;
    if (slot->kind != MMAY_MANAGED_OBJECT_MAP || slot->as.map == NULL
        || slot->as.map->descriptor == NULL || !slot->as.map->descriptor->is_set) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (inserted == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    *inserted = false;
    return mmay_managed_map_put_resolved(
        heap, reference, slot->as.map, key, ignored, inserted
    );
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
        heap, reference, key, &lookup_map, &bucket, &entry, &found, NULL
    );
    if (status != MMAY_MANAGED_OK || !found) {
        return status;
    }
    if (lookup_map->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    map = (MarimayManagedMap *)lookup_map;
    if (!map->descriptor->is_set) {
        *value = mmay_managed_map_entry_value(map, entry);
    }
    memset(mmay_managed_map_entry_prefix(map, entry), 0, map->entry_stride);
    mmay_map_index_bucket_mark(
        map->buckets, map->bucket_capacity, bucket, MMAY_MAP_TOMBSTONE
    );
    map->length -= 1U;
    if (map->length == 0) {
        /* All payloads have been erased by removal. Reuse the reservation
         * from its beginning without rebuilding or retaining old tombstones. */
        map->entry_count = 0;
        mmay_map_index_reset(map->buckets, map->bucket_capacity);
    }
    map->mutation_generation += UINT64_C(1);
    *removed = true;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_map_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    MarimayManagedMap *map = NULL;
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
        map->entry_capacity * map->entry_stride
    );
    mmay_map_index_reset(map->buckets, map->bucket_capacity);
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
        if (!mmay_managed_map_entry_prefix(map, index)->active) {
            continue;
        }
        if (active == ordinal) {
            *key = mmay_managed_map_entry_key(map, index);
            *value = mmay_managed_map_entry_value(map, index);
            return MMAY_MANAGED_OK;
        }
        active += 1U;
    }
    return MMAY_MANAGED_INVALID_REFERENCE;
}
