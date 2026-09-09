#ifndef MMAY_BASE_INTEGER_RANGE_EDGES_H
#define MMAY_BASE_INTEGER_RANGE_EDGES_H

#include "mmay/base/integer_range.h"

/* A transient index of the immutable adapter's CFG. Each successor's list is
 * in ascending predecessor order, matching the lattice's original full scan.
 * A branch whose two arms share a target contributes only one predecessor. */
typedef struct MarimayIntegerRangeEdge {
    size_t predecessor;
    size_t next;
} MarimayIntegerRangeEdge;

typedef struct MarimayIntegerRangeEdges {
    MarimayAllocator allocator;
    void *storage;
    size_t storage_bytes;
    size_t *heads;
    MarimayIntegerRangeEdge *entries;
} MarimayIntegerRangeEdges;

MarimayIntegerRangeStatus mmay_integer_range_edges_init(
    const MarimayIntegerRangeProgram *program,
    MarimayIntegerRangeEdges *edges
);
void mmay_integer_range_edges_dispose(MarimayIntegerRangeEdges *edges);

#endif
