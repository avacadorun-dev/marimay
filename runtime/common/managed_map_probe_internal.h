#ifndef MMAY_MANAGED_MAP_PROBE_INTERNAL_H
#define MMAY_MANAGED_MAP_PROBE_INTERNAL_H

#include "mmay/runtime/generated_map.h"

#define MMAY_MAP_EMPTY SIZE_MAX
#define MMAY_MAP_TOMBSTONE (SIZE_MAX - 1U)

/* Borrowed search progress, never a retained object/storage pointer. A caller
 * rejecting a reference-key hash candidate resumes the same bounded walk. */
typedef struct MarimayManagedMapProbe {
    size_t next_index;
    size_t first_tombstone;
} MarimayManagedMapProbe;

/* The map owner has validated the receiver and key. This allocation-free
 * owner filters private index metadata in groups, checks each inspected
 * bucket/entry candidate and performs the common bounded linear probe.
 * Raw keys compare exact bits; reference keys yield hash candidates for the
 * map owner's nominal/string equality. NULL progress starts a one-shot scan.
 * All three output pointers are required, non-NULL caller-owned locations.
 * A non-NULL progress starts as {0, MMAY_MAP_EMPTY} and is modified only here;
 * the map must not mutate between calls. The initialized index has a
 * power-of-two capacity of at least eight and a complete mirrored control tail.
 * found is false at an empty slot or exhausted tombstone scan. These
 * preconditions are established by the checked Map/Set APIs; progress never
 * comes from source or bytecode. */
MarimayManagedStatus mmay_managed_map_probe(
    const MarimayManagedMap *map,
    uint64_t hash,
    bool raw_key,
    uint64_t key_bits,
    size_t *bucket,
    size_t *entry,
    bool *found,
    MarimayManagedMapProbe *progress
);

#endif
