#include "test_support.h"
#include "../../bootstrap/src/base/integer_range_edges.h"

#include <limits.h>
#include <string.h>

typedef struct MarimayRangeGraphBlock {
    MarimayIntegerRangeInstruction instructions[3];
    size_t count;
    MarimayIntegerRangeTerminator terminator;
} MarimayRangeGraphBlock;

typedef struct MarimayRangeGraphFixture {
    MarimayRangeGraphBlock blocks[64];
    size_t count;
    size_t terminator_reads;
    bool fail_terminator;
} MarimayRangeGraphFixture;

static bool range_graph_bounds(const void *context, uint32_t id,
    MarimayIntegerRange *range) {
    (void)context;
    if (id >= 4) return false;
    *range = (MarimayIntegerRange){INT64_MIN, INT64_MAX};
    return true;
}

static bool range_graph_length(const void *context, size_t block, size_t *count) {
    const MarimayRangeGraphFixture *fixture = context;
    if (block >= fixture->count) return false;
    *count = fixture->blocks[block].count;
    return true;
}

static bool range_graph_instruction(const void *context, size_t block,
    size_t index, MarimayIntegerRangeInstruction *instruction) {
    const MarimayRangeGraphFixture *fixture = context;
    if (block >= fixture->count || index >= fixture->blocks[block].count) {
        return false;
    }
    *instruction = fixture->blocks[block].instructions[index];
    return true;
}

static bool range_graph_terminator(const void *context, size_t block,
    MarimayIntegerRangeTerminator *terminator) {
    MarimayRangeGraphFixture *fixture = (MarimayRangeGraphFixture *)context;
    fixture->terminator_reads += 1;
    if (block >= fixture->count || fixture->fail_terminator) return false;
    *terminator = fixture->blocks[block].terminator;
    return true;
}

static MarimayIntegerRangeProgram range_graph_program(
    MarimayRangeGraphFixture *fixture, MarimayAllocator allocator) {
    return (MarimayIntegerRangeProgram){allocator, fixture, fixture->count, 4,
        range_graph_bounds, range_graph_length, range_graph_instruction,
        range_graph_terminator};
}

static MarimayIntegerRangeInstruction range_graph_constant(uint32_t target,
    int64_t value) {
    return (MarimayIntegerRangeInstruction){
        MMAY_INTEGER_RANGE_CONSTANT, target, 0, 0, value};
}

static void range_graph_goto(MarimayRangeGraphFixture *fixture,
    size_t block, uint32_t target) {
    fixture->blocks[block].terminator = (MarimayIntegerRangeTerminator){
        MMAY_INTEGER_RANGE_TERMINATOR_GOTO, 0, target, 0};
}

static void range_graph_fixture(MarimayRangeGraphFixture *fixture,
    unsigned shape) {
    memset(fixture, 0, sizeof(*fixture));
    if (shape == 0) {
        fixture->count = 64;
        for (size_t block = 0; block < fixture->count; block += 1) {
            fixture->blocks[block].count = 1;
            fixture->blocks[block].instructions[0] =
                range_graph_constant(0, (int64_t)block);
            if (block + 1 < fixture->count) {
                range_graph_goto(fixture, block, (uint32_t)block + 1);
            }
        }
        return;
    }
    fixture->count = 7;
    fixture->blocks[0].count = 3;
    fixture->blocks[0].instructions[0] = range_graph_constant(0, 0);
    fixture->blocks[0].instructions[1] = range_graph_constant(1, 10);
    fixture->blocks[0].instructions[2] = range_graph_constant(2, 1);
    range_graph_goto(fixture, 0, 4);
    fixture->blocks[4].count = 1;
    fixture->blocks[4].instructions[0] = (MarimayIntegerRangeInstruction){
        MMAY_INTEGER_RANGE_LESS, 3, 0, 1, 0};
    fixture->blocks[4].terminator = (MarimayIntegerRangeTerminator){
        MMAY_INTEGER_RANGE_TERMINATOR_BRANCH, 3, 2, 6};
    fixture->blocks[2].count = 1;
    fixture->blocks[2].instructions[0] = (MarimayIntegerRangeInstruction){
        MMAY_INTEGER_RANGE_ADD_CHECKED, 0, 0, 2, 0};
    range_graph_goto(fixture, 2, 4);
    range_graph_goto(fixture, 6, 1);
    range_graph_goto(fixture, 1, 5);
    /* Block 3 is unreachable. Shape 2 has two equal branch targets; shape 3
     * refines <=; shape 4 changes the comparison's definition before its use. */
    if (shape == 2) fixture->blocks[4].terminator.otherwise = 2;
    if (shape == 3) {
        fixture->blocks[4].instructions[0].kind = MMAY_INTEGER_RANGE_LESS_EQUAL;
    }
    if (shape == 4) {
        fixture->blocks[4].count = 2;
        fixture->blocks[4].instructions[1] = range_graph_constant(3, 0);
    }
}

static uint64_t range_graph_hash(uint64_t hash, uint64_t value) {
    for (unsigned index = 0; index < 8; index += 1) {
        hash ^= value & UINT64_C(255);
        hash *= UINT64_C(1099511628211);
        value >>= 8;
    }
    return hash;
}

/* Hash explicit integers, never padding, pointers or host byte order. This
 * transcript includes every finite work boundary, all incoming facts, and
 * every arithmetic proof while replaying each reachable block. The constants
 * were recorded from the pre-index analyzer at revision 880d9bd. */
static uint64_t range_graph_observe(MarimayRangeGraphFixture *fixture,
    size_t limit, uint64_t hash, size_t *work_output) {
    MarimayIntegerRangeProgram program =
        range_graph_program(fixture, mmay_system_allocator());
    MarimayIntegerRangeAnalysis analysis = {0};
    size_t work = 0;
    MarimayIntegerRangeStatus status =
        mmay_integer_range_analyze(&program, limit, &work, &analysis);
    hash = range_graph_hash(hash, (uint64_t)status);
    hash = range_graph_hash(hash, (uint64_t)work);
    if (status == MMAY_INTEGER_RANGE_OK) {
        for (size_t block = 0; block < fixture->count; block += 1) {
            bool ready = mmay_integer_range_begin_block(&analysis, block);
            hash = range_graph_hash(hash, ready ? 1 : 0);
            if (!ready) continue;
            for (size_t reg = 0; reg < 4; reg += 1) {
                hash = range_graph_hash(hash, (uint64_t)analysis.scratch[reg].minimum);
                hash = range_graph_hash(hash, (uint64_t)analysis.scratch[reg].maximum);
            }
            for (size_t index = 0; index < fixture->blocks[block].count; index += 1) {
                const MarimayIntegerRangeInstruction *instruction =
                    &fixture->blocks[block].instructions[index];
                hash = range_graph_hash(hash,
                    mmay_integer_range_proves_instruction(&analysis, instruction) ? 1 : 0);
                MMAY_EXPECT(mmay_integer_range_advance(&analysis, instruction));
            }
        }
    } else {
        MMAY_EXPECT(status == MMAY_INTEGER_RANGE_LIMIT_REACHED);
        MMAY_EXPECT(analysis.incoming == NULL && analysis.scratch == NULL
            && analysis.incoming_ready == NULL);
    }
    mmay_integer_range_analysis_dispose(&analysis);
    *work_output = work;
    return hash;
}

static void range_graph_contract(void) {
    static const uint64_t expected[] = {
        UINT64_C(16175964774397066354), UINT64_C(29646276445408146),
        UINT64_C(13278253036071772039), UINT64_C(11448459166429346608),
        UINT64_C(6754652222473348678)
    };
    for (unsigned shape = 0; shape < 5; shape += 1) {
        MarimayRangeGraphFixture fixture;
        size_t work = 0;
        range_graph_fixture(&fixture, shape);
        uint64_t hash = range_graph_observe(&fixture, SIZE_MAX,
            UINT64_C(14695981039346656037), &work);
        size_t complete_work = work;
        if (shape == 0) {
            MMAY_EXPECT(complete_work == 3 * (64 * 2 + 63 * 4));
            /* Three lattice passes visit only real edges, plus one CFG
             * indexing pass. This detects a return to a quadratic scan. */
            MMAY_EXPECT(fixture.terminator_reads == 64 + 3 * 63);
        }
        for (size_t limit = 0; limit <= complete_work + 1; limit += 1) {
            hash = range_graph_observe(&fixture, limit, hash, &work);
        }
        MMAY_EXPECT(hash == expected[shape]);
    }
}

static void range_graph_allocations(void) {
    MarimayRangeGraphFixture fixture;
    range_graph_fixture(&fixture, 1);
    bool completed = false;
    for (size_t fail = 0; fail < 32 && !completed; fail += 1) {
        MarimayFailAllocator allocator;
        mmay_fail_allocator_init(&allocator, mmay_system_allocator(), fail);
        MarimayIntegerRangeProgram program =
            range_graph_program(&fixture, mmay_fail_allocator(&allocator));
        MarimayIntegerRangeAnalysis analysis = {0};
        size_t work = 0;
        MarimayIntegerRangeStatus status =
            mmay_integer_range_analyze(&program, SIZE_MAX, &work, &analysis);
        MMAY_EXPECT(status == MMAY_INTEGER_RANGE_OK
            || status == MMAY_INTEGER_RANGE_ALLOCATION_FAILED);
        completed = status == MMAY_INTEGER_RANGE_OK;
        mmay_integer_range_analysis_dispose(&analysis);
        MMAY_EXPECT(allocator.live_allocations == 0 && allocator.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
}

static void range_graph_invalid_edges(void) {
    MarimayRangeGraphFixture fixture;
    range_graph_fixture(&fixture, 1);
    MarimayFailAllocator allocator;
    mmay_fail_allocator_init(&allocator, mmay_system_allocator(), SIZE_MAX);
    MarimayIntegerRangeProgram program =
        range_graph_program(&fixture, mmay_fail_allocator(&allocator));
    MarimayIntegerRangeEdges edges = {0};
    MMAY_EXPECT(mmay_integer_range_edges_init(NULL, &edges) == MMAY_INTEGER_RANGE_INVALID);
    MMAY_EXPECT(mmay_integer_range_edges_init(&program, NULL) == MMAY_INTEGER_RANGE_INVALID);
    size_t blocks = program.block_count;
    program.block_count = SIZE_MAX;
    MMAY_EXPECT(mmay_integer_range_edges_init(&program, &edges) == MMAY_INTEGER_RANGE_INVALID);
    mmay_integer_range_edges_dispose(&edges);
    MMAY_EXPECT(allocator.attempts == 0);
    program.block_count = blocks;
    fixture.fail_terminator = true;
    MMAY_EXPECT(mmay_integer_range_edges_init(&program, &edges) == MMAY_INTEGER_RANGE_OK);
    for (size_t block = 0; block < blocks; block += 1) {
        MMAY_EXPECT(edges.heads[block] == SIZE_MAX);
    }
    mmay_integer_range_edges_dispose(&edges);
    fixture.fail_terminator = false;
    /* Invalid targets do not index outside the arena; duplicate branch arms
     * contribute once. Self-edges and incoming edges to entry remain indexed. */
    fixture.blocks[4].terminator.otherwise = UINT32_MAX;
    range_graph_goto(&fixture, 3, 0);
    range_graph_goto(&fixture, 1, 1);
    MMAY_EXPECT(mmay_integer_range_edges_init(&program, &edges) == MMAY_INTEGER_RANGE_OK);
    MMAY_EXPECT(edges.entries[edges.heads[0]].predecessor == 3);
    MMAY_EXPECT(edges.entries[edges.heads[1]].predecessor == 1);
    size_t first = edges.heads[4];
    MMAY_EXPECT(edges.entries[first].predecessor == 0);
    MMAY_EXPECT(edges.entries[edges.entries[first].next].predecessor == 2);
    mmay_integer_range_edges_dispose(&edges);
    mmay_integer_range_edges_dispose(&edges);
    MMAY_EXPECT(allocator.live_allocations == 0 && allocator.live_bytes == 0);
}

void mmay_test_integer_range_edges(void) {
    range_graph_contract();
    range_graph_allocations();
    range_graph_invalid_edges();
}
