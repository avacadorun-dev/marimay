#ifndef MMAY_MANAGED_MAP_INDEX_INTERNAL_H
#define MMAY_MANAGED_MAP_INDEX_INTERNAL_H

#include "mmay/base/allocator.h"
#include <string.h>

/* D090 private index representation. Capacity is zero or a power of two >= 8.
 * Control bytes share the bucket allocation; seven mirrored bytes permit an
 * eight-byte group load at any in-bounds position. The storage owner initializes
 * the whole tail before publication. Mutation owners maintain every mirror. */
#define MMAY_MAP_INDEX_EMPTY 0x80U
#define MMAY_MAP_INDEX_DELETED 0xfeU
#define MMAY_MAP_INDEX_ONES UINT64_C(0x0101010101010101)
#define MMAY_MAP_INDEX_HIGHS UINT64_C(0x8080808080808080)

static inline unsigned char *mmay_map_index_controls(size_t *buckets, size_t capacity) {
    return (unsigned char *)(buckets + capacity);
}

static inline bool mmay_map_index_bytes(size_t capacity, size_t *bytes) {
    size_t controls = 0;
    if (capacity == 0) {
        *bytes = 0;
        return true;
    }
    return mmay_size_multiply(capacity, sizeof(size_t), bytes)
        && mmay_size_add(capacity, 7U, &controls)
        && mmay_size_add(*bytes, controls, bytes);
}

static inline void mmay_map_index_control_store(size_t *buckets, size_t capacity,
    size_t position, unsigned char control) {
    unsigned char *controls = mmay_map_index_controls(buckets, capacity);
    controls[position] = control;
    if (position < 7U) controls[capacity + position] = control;
}

static inline void mmay_map_index_bucket_store(size_t *buckets, size_t capacity,
    size_t position, size_t entry, uint64_t hash) {
    buckets[position] = entry;
    mmay_map_index_control_store(buckets, capacity, position, (unsigned char)(hash >> 57U));
}

static inline void mmay_map_index_bucket_mark(size_t *buckets, size_t capacity,
    size_t position, size_t marker) {
    buckets[position] = marker;
    mmay_map_index_control_store(buckets, capacity, position,
        marker == SIZE_MAX ? MMAY_MAP_INDEX_EMPTY : MMAY_MAP_INDEX_DELETED);
}

static inline void mmay_map_index_reset(size_t *buckets, size_t capacity) {
    size_t index;
    if (capacity == 0) return;
    for (index = 0; index < capacity; index += 1U) buckets[index] = SIZE_MAX;
    memset(mmay_map_index_controls(buckets, capacity), MMAY_MAP_INDEX_EMPTY, capacity + 7U);
}

/* Explicit byte order keeps the byte-to-probe mapping portable and permits
 * compilers to combine the C17 expression into an unaligned scalar load. */
static inline uint64_t mmay_map_index_load(const unsigned char *bytes) {
    return (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8U)
        | ((uint64_t)bytes[2] << 16U) | ((uint64_t)bytes[3] << 24U)
        | ((uint64_t)bytes[4] << 32U) | ((uint64_t)bytes[5] << 40U)
        | ((uint64_t)bytes[6] << 48U) | ((uint64_t)bytes[7] << 56U);
}

static inline uint64_t mmay_map_index_match(uint64_t bytes, unsigned char control) {
    uint64_t different = bytes ^ ((uint64_t)control * MMAY_MAP_INDEX_ONES);
    /* Borrow can add a later false candidate. Full hash/key checks below are
     * mandatory; valid empty/deleted encodings have no ambiguous neighbour. */
    return (different - MMAY_MAP_INDEX_ONES) & ~different & MMAY_MAP_INDEX_HIGHS;
}

/* The probe owner calls this only with a nonzero match mask. */
static inline size_t mmay_map_index_first(uint64_t matches) {
#if defined(__clang__) || defined(__GNUC__)
    return (size_t)__builtin_ctzll((unsigned long long)matches) / 8U;
#else
    size_t index = 0;
    while ((matches & UINT64_C(0x80)) == 0) {
        matches >>= 8U;
        index += 1U;
    }
    return index;
#endif
}

#endif
