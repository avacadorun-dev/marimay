#ifndef MMAY_MANAGED_MAP_STORAGE_INTERNAL_H
#define MMAY_MANAGED_MAP_STORAGE_INTERNAL_H

#include <string.h>

#include "mmay/runtime/generated_map.h"
#include "managed_reference_internal.h"

/* Descriptor-validated storage only. Every entry starts with this aligned
 * prefix, then exact raw-word/reference key bytes and (for Map) value bytes.
 * No unused reference tail or Set value is allocated or traced. */
typedef struct MarimayManagedMapEntry {
    bool active;
    uint64_t hash;
} MarimayManagedMapEntry;

_Static_assert(sizeof(MarimayManagedMapEntry) + 2U * sizeof(MarimayManagedCell) <= UINT8_MAX,
    "map layout byte counts must fit their immutable fields");

#if defined(__clang__) || defined(__GNUC__)
#define MMAY_MAP_STORAGE_INLINE static inline __attribute__((always_inline))
#else
#define MMAY_MAP_STORAGE_INLINE static inline
#endif

_Static_assert(sizeof(uint64_t) % _Alignof(MarimayManagedMapEntry) == 0,
    "raw map cells must preserve entry prefix alignment");
_Static_assert(sizeof(MarimayManagedReference) % _Alignof(MarimayManagedMapEntry) == 0,
    "reference map cells must preserve entry prefix alignment");

static inline size_t mmay_managed_map_cell_bytes(MarimayManagedCellKind kind) {
    return kind == MMAY_MANAGED_CELL_RAW64
        ? sizeof(uint64_t) : sizeof(MarimayManagedReference);
}

static inline size_t mmay_managed_map_entry_stride(
    const MarimayManagedMapDescriptor *descriptor
) {
    return sizeof(MarimayManagedMapEntry)
        + mmay_managed_map_cell_bytes(descriptor->key.cell_kind)
        + (descriptor->is_set ? 0
            : mmay_managed_map_cell_bytes(descriptor->value.cell_kind));
}

static inline void mmay_managed_map_storage_layout(
    MarimayManagedMap *map, const MarimayManagedMapDescriptor *descriptor
) {
    map->entry_stride = (uint8_t)mmay_managed_map_entry_stride(descriptor);
    map->key_bytes = (uint8_t)mmay_managed_map_cell_bytes(descriptor->key.cell_kind);
    map->value_bytes = descriptor->is_set ? 0
        : (uint8_t)mmay_managed_map_cell_bytes(descriptor->value.cell_kind);
}

/* Callers establish a live entry/storage index. Borrowed addresses never
 * cross allocation or collection; packed payload transfer uses memcpy, not
 * unaligned or type-punned cell dereferences. */
static inline MarimayManagedMapEntry *mmay_managed_map_entry_prefix(
    const MarimayManagedMap *map, size_t index
) {
    return (MarimayManagedMapEntry *)(void *)(map->entries + index * map->entry_stride);
}

static inline unsigned char *mmay_managed_map_key_address(
    const MarimayManagedMap *map, size_t index
) {
    return map->entries + index * map->entry_stride + sizeof(MarimayManagedMapEntry);
}

static inline unsigned char *mmay_managed_map_value_address(
    const MarimayManagedMap *map, size_t index
) {
    return mmay_managed_map_key_address(map, index)
        + map->key_bytes;
}

static inline MarimayManagedCell mmay_managed_map_load_cell(
    const unsigned char *address, size_t cell_bytes
) {
    MarimayManagedCell cell = {0};
    if (cell_bytes == sizeof(uint64_t)) {
        memcpy(&cell.f64_bits, address, sizeof(cell.f64_bits));
    } else {
        memcpy(&cell.reference, address, sizeof(cell.reference));
    }
    return cell;
}

static inline void mmay_managed_map_store_cell(
    unsigned char *address, size_t cell_bytes, MarimayManagedCell cell
) {
    /* Separate fixed-size transfers: a conditional 8/16-byte memcpy otherwise
     * becomes an out-of-line variable-length copy in current Clang release C.
     * The first word is valid object representation for either cell kind. */
    memcpy(address, &cell, sizeof(uint64_t));
    if (cell_bytes != sizeof(uint64_t)) {
        memcpy(address + sizeof(uint64_t),
            (const unsigned char *)&cell + sizeof(uint64_t),
            sizeof(MarimayManagedReference) - sizeof(uint64_t));
    }
}

static inline uint64_t mmay_managed_map_raw_key(
    const MarimayManagedMap *map, size_t index
) {
    uint64_t bits;
    memcpy(&bits, mmay_managed_map_key_address(map, index), sizeof(bits));
    return bits;
}

static inline MarimayManagedCell mmay_managed_map_entry_key(
    const MarimayManagedMap *map, size_t index
) {
    return mmay_managed_map_load_cell(mmay_managed_map_key_address(map, index),
        map->key_bytes);
}

static inline MarimayManagedCell mmay_managed_map_entry_value(
    const MarimayManagedMap *map, size_t index
) {
    return mmay_managed_map_load_cell(mmay_managed_map_value_address(map, index),
        map->value_bytes);
}

MMAY_MAP_STORAGE_INLINE void mmay_managed_map_store_key(
    const MarimayManagedMap *map, size_t index, MarimayManagedCell cell
) {
    mmay_managed_map_store_cell(mmay_managed_map_key_address(map, index),
        map->key_bytes, cell);
}

MMAY_MAP_STORAGE_INLINE void mmay_managed_map_store_value(
    const MarimayManagedMap *map, size_t index, MarimayManagedCell cell
) {
    mmay_managed_map_store_cell(mmay_managed_map_value_address(map, index),
        map->value_bytes, cell);
}

/* Shared checked handle boundary for operation and storage owners. */
static inline MarimayManagedStatus mmay_managed_map_resolve(
    const MarimayManagedHeap *heap, MarimayManagedReference reference,
    const MarimayManagedMap **result
) {
    const MarimayManagedSlot *slot;
    if (result != NULL) *result = NULL;
    if (!mmay_managed_heap_valid(heap) || result == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) return MMAY_MANAGED_INVALID_REFERENCE;
    if (slot->kind != MMAY_MANAGED_OBJECT_MAP || slot->as.map == NULL
        || slot->as.map->descriptor == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *result = slot->as.map;
    return MMAY_MANAGED_OK;
}

static inline MarimayManagedStatus mmay_managed_map_resolve_mutable(
    MarimayManagedHeap *heap, MarimayManagedReference reference,
    MarimayManagedMap **result
) {
    const MarimayManagedMap *resolved = NULL;
    MarimayManagedStatus status = mmay_managed_map_resolve(heap, reference, &resolved);
    if (result != NULL) {
        *result = status == MMAY_MANAGED_OK ? (MarimayManagedMap *)resolved : NULL;
    }
    return result == NULL ? MMAY_MANAGED_INVALID_ARGUMENT : status;
}

MarimayManagedStatus mmay_managed_map_ensure_storage(
    MarimayManagedHeap *heap, MarimayManagedReference reference,
    const MarimayManagedCell *key, const MarimayManagedCell *value,
    MarimayManagedMap **result
);

#undef MMAY_MAP_STORAGE_INLINE

#endif
