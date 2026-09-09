#include "integer_range_edges.h"

#include <string.h>

static void mmay_integer_range_edges_prepend(
    MarimayIntegerRangeEdges *edges,
    size_t block_count,
    size_t predecessor,
    size_t successor,
    size_t *count
) {
    /* Match the old predecessor predicate: an unrepresentable successor or
     * failed terminator read supplies no edge. Structural validation belongs
     * to the MIR/bytecode verifier before this representation-neutral pass. */
    if (successor >= block_count) return;
    edges->entries[*count] = (MarimayIntegerRangeEdge){
        predecessor, edges->heads[successor]
    };
    edges->heads[successor] = *count;
    *count += 1;
}

MarimayIntegerRangeStatus mmay_integer_range_edges_init(
    const MarimayIntegerRangeProgram *program,
    MarimayIntegerRangeEdges *edges
) {
    size_t heads_bytes = 0;
    size_t entry_count = 0;
    size_t entries_bytes = 0;
    size_t count = 0;
    if (edges == NULL) return MMAY_INTEGER_RANGE_INVALID;
    memset(edges, 0, sizeof(*edges));
    if (program == NULL || program->block_count == 0
        || program->read_terminator == NULL) {
        return MMAY_INTEGER_RANGE_INVALID;
    }
    if (!mmay_size_multiply(program->block_count, sizeof(size_t), &heads_bytes)
        || !mmay_size_multiply(program->block_count, 2, &entry_count)
        || !mmay_size_multiply(entry_count, sizeof(*edges->entries), &entries_bytes)
        || !mmay_size_add(heads_bytes, entries_bytes, &edges->storage_bytes)) {
        return MMAY_INTEGER_RANGE_INVALID;
    }
    edges->allocator = program->allocator;
    edges->storage = mmay_allocate(&edges->allocator, edges->storage_bytes);
    if (edges->storage == NULL) {
        mmay_integer_range_edges_dispose(edges);
        return MMAY_INTEGER_RANGE_ALLOCATION_FAILED;
    }
    edges->heads = edges->storage;
    /* Both regions contain only size_t fields, so heads_bytes preserves the
     * required alignment without an unchecked alignment calculation. */
    edges->entries = (MarimayIntegerRangeEdge *)(
        (unsigned char *)edges->storage + heads_bytes);
    for (size_t block = 0; block < program->block_count; block += 1) {
        edges->heads[block] = SIZE_MAX;
    }
    for (size_t cursor = program->block_count; cursor != 0; cursor -= 1) {
        size_t predecessor = cursor - 1;
        MarimayIntegerRangeTerminator terminator = {0};
        if (!program->read_terminator(
                program->context, predecessor, &terminator)) continue;
        if (terminator.kind == MMAY_INTEGER_RANGE_TERMINATOR_GOTO
            || terminator.kind == MMAY_INTEGER_RANGE_TERMINATOR_BRANCH) {
            mmay_integer_range_edges_prepend(edges, program->block_count,
                predecessor, terminator.target, &count);
        }
        if (terminator.kind == MMAY_INTEGER_RANGE_TERMINATOR_BRANCH
            && terminator.otherwise != terminator.target) {
            mmay_integer_range_edges_prepend(edges, program->block_count,
                predecessor, terminator.otherwise, &count);
        }
    }
    return MMAY_INTEGER_RANGE_OK;
}

void mmay_integer_range_edges_dispose(MarimayIntegerRangeEdges *edges) {
    if (edges == NULL) return;
    mmay_deallocate(&edges->allocator, edges->storage, edges->storage_bytes);
    memset(edges, 0, sizeof(*edges));
}
