#include "test_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/checked_i64.h"
#include "mmay/base/checked_i64_fast.h"
#include "mmay/base/integer_range.h"

typedef struct MarimayTestRangeBlock {
    MarimayIntegerRangeInstruction instructions[4];
    size_t instruction_count;
    MarimayIntegerRangeTerminator terminator;
} MarimayTestRangeBlock;

typedef struct MarimayTestRangeProgram {
    MarimayIntegerRange bounds[8];
    size_t register_count;
    MarimayTestRangeBlock blocks[4];
    size_t block_count;
} MarimayTestRangeProgram;

static bool mmay_test_range_bounds(
    const void *context,
    uint32_t register_id,
    MarimayIntegerRange *range
) {
    const MarimayTestRangeProgram *program = context;
    if (program == NULL || range == NULL
        || register_id >= program->register_count) {
        return false;
    }
    *range = program->bounds[register_id];
    return true;
}

static bool mmay_test_range_block_length(
    const void *context,
    size_t block_index,
    size_t *instruction_count
) {
    const MarimayTestRangeProgram *program = context;
    if (program == NULL || instruction_count == NULL
        || block_index >= program->block_count) {
        return false;
    }
    *instruction_count = program->blocks[block_index].instruction_count;
    return true;
}

static bool mmay_test_range_instruction(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    MarimayIntegerRangeInstruction *instruction
) {
    const MarimayTestRangeProgram *program = context;
    if (program == NULL || instruction == NULL
        || block_index >= program->block_count
        || instruction_index
            >= program->blocks[block_index].instruction_count) {
        return false;
    }
    *instruction = program->blocks[block_index].instructions[instruction_index];
    return true;
}

static bool mmay_test_range_terminator(
    const void *context,
    size_t block_index,
    MarimayIntegerRangeTerminator *terminator
) {
    const MarimayTestRangeProgram *program = context;
    if (program == NULL || terminator == NULL
        || block_index >= program->block_count) {
        return false;
    }
    *terminator = program->blocks[block_index].terminator;
    return true;
}

static MarimayIntegerRangeInstruction mmay_test_range_make(
    MarimayIntegerRangeInstructionKind kind,
    uint32_t target,
    uint32_t first,
    uint32_t second,
    int64_t constant
) {
    MarimayIntegerRangeInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.constant = constant;
    return instruction;
}

static void mmay_test_range_build_branch(MarimayTestRangeProgram *program) {
    size_t index = 0;
    memset(program, 0, sizeof(*program));
    program->register_count = 6;
    program->block_count = 3;
    for (index = 0; index < program->register_count; index += 1) {
        program->bounds[index].minimum = INT64_MIN;
        program->bounds[index].maximum = INT64_MAX;
    }
    program->bounds[2].minimum = 0;
    program->bounds[2].maximum = 1;
    program->blocks[0].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 1, 0, 0, 10
    );
    program->blocks[0].instructions[1] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_LESS, 2, 0, 1, 0
    );
    program->blocks[0].instruction_count = 2;
    program->blocks[0].terminator.kind =
        MMAY_INTEGER_RANGE_TERMINATOR_BRANCH;
    program->blocks[0].terminator.condition = 2;
    program->blocks[0].terminator.target = 1;
    program->blocks[0].terminator.otherwise = 2;
    program->blocks[1].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 3, 0, 0, 1
    );
    program->blocks[1].instructions[1] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_ADD_CHECKED, 4, 0, 3, 0
    );
    program->blocks[1].instruction_count = 2;
    program->blocks[1].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_OTHER;
    program->blocks[2].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 3, 0, 0, 1
    );
    program->blocks[2].instructions[1] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_SUBTRACT_CHECKED, 5, 0, 3, 0
    );
    program->blocks[2].instruction_count = 2;
    program->blocks[2].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_OTHER;
}

static MarimayIntegerRangeProgram mmay_test_range_adapter(
    const MarimayTestRangeProgram *fixture,
    MarimayAllocator allocator
) {
    MarimayIntegerRangeProgram program;
    memset(&program, 0, sizeof(program));
    program.allocator = allocator;
    program.context = fixture;
    program.block_count = fixture->block_count;
    program.register_count = fixture->register_count;
    program.register_bounds = mmay_test_range_bounds;
    program.block_length = mmay_test_range_block_length;
    program.read_instruction = mmay_test_range_instruction;
    program.read_terminator = mmay_test_range_terminator;
    return program;
}

static void mmay_test_range_branch_proofs(void) {
    MarimayTestRangeProgram fixture;
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    MarimayIntegerRangeInstruction instruction;
    size_t work = 0;
    memset(&analysis, 0, sizeof(analysis));
    mmay_test_range_build_branch(&fixture);
    program = mmay_test_range_adapter(&fixture, mmay_system_allocator());
    MMAY_EXPECT(mmay_integer_range_analyze(
        &program, 4096, &work, &analysis
    ) == MMAY_INTEGER_RANGE_OK);
    MMAY_EXPECT(work > 0);

    MMAY_EXPECT(mmay_integer_range_begin_block(&analysis, 1));
    instruction = fixture.blocks[1].instructions[0];
    MMAY_EXPECT(mmay_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[1].instructions[1];
    MMAY_EXPECT(mmay_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    MMAY_EXPECT(mmay_integer_range_advance(&analysis, &instruction));

    MMAY_EXPECT(mmay_integer_range_begin_block(&analysis, 2));
    instruction = fixture.blocks[2].instructions[0];
    MMAY_EXPECT(mmay_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[2].instructions[1];
    MMAY_EXPECT(mmay_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    mmay_integer_range_analysis_dispose(&analysis);
}

static void mmay_test_range_retains_overflow(void) {
    MarimayTestRangeProgram fixture;
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    MarimayIntegerRangeInstruction instruction;
    size_t work = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&analysis, 0, sizeof(analysis));
    fixture.register_count = 3;
    fixture.block_count = 1;
    fixture.bounds[0] = (MarimayIntegerRange){INT64_MIN, INT64_MAX};
    fixture.bounds[1] = fixture.bounds[0];
    fixture.bounds[2] = fixture.bounds[0];
    fixture.blocks[0].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 0, 0, 0, INT64_MAX
    );
    fixture.blocks[0].instructions[1] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 1, 0, 0, 1
    );
    fixture.blocks[0].instructions[2] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_ADD_CHECKED, 2, 0, 1, 0
    );
    fixture.blocks[0].instruction_count = 3;
    fixture.blocks[0].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_OTHER;
    program = mmay_test_range_adapter(&fixture, mmay_system_allocator());
    MMAY_EXPECT(mmay_integer_range_analyze(
        &program, 4096, &work, &analysis
    ) == MMAY_INTEGER_RANGE_OK);
    MMAY_EXPECT(mmay_integer_range_begin_block(&analysis, 0));
    instruction = fixture.blocks[0].instructions[0];
    MMAY_EXPECT(mmay_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[0].instructions[1];
    MMAY_EXPECT(mmay_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[0].instructions[2];
    MMAY_EXPECT(!mmay_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    mmay_integer_range_analysis_dispose(&analysis);
}

static void mmay_test_range_loop_widening_and_narrowing(void) {
    MarimayTestRangeProgram fixture;
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    MarimayIntegerRangeInstruction instruction;
    size_t index = 0;
    size_t work = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&analysis, 0, sizeof(analysis));
    fixture.register_count = 4;
    fixture.block_count = 4;
    for (index = 0; index < fixture.register_count; index += 1) {
        fixture.bounds[index] = (MarimayIntegerRange){INT64_MIN, INT64_MAX};
    }
    fixture.blocks[0].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 0, 0, 0, 0
    );
    fixture.blocks[0].instructions[1] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 1, 0, 0, 100
    );
    fixture.blocks[0].instructions[2] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_CONSTANT, 2, 0, 0, 1
    );
    fixture.blocks[0].instruction_count = 3;
    fixture.blocks[0].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_GOTO;
    fixture.blocks[0].terminator.target = 1;
    fixture.blocks[1].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_LESS, 3, 0, 1, 0
    );
    fixture.blocks[1].instruction_count = 1;
    fixture.blocks[1].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_BRANCH;
    fixture.blocks[1].terminator.condition = 3;
    fixture.blocks[1].terminator.target = 2;
    fixture.blocks[1].terminator.otherwise = 3;
    fixture.blocks[2].instructions[0] = mmay_test_range_make(
        MMAY_INTEGER_RANGE_ADD_CHECKED, 0, 0, 2, 0
    );
    fixture.blocks[2].instruction_count = 1;
    fixture.blocks[2].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_GOTO;
    fixture.blocks[2].terminator.target = 1;
    fixture.blocks[3].terminator.kind = MMAY_INTEGER_RANGE_TERMINATOR_OTHER;
    program = mmay_test_range_adapter(&fixture, mmay_system_allocator());
    MMAY_EXPECT(mmay_integer_range_analyze(
        &program, 512, &work, &analysis
    ) == MMAY_INTEGER_RANGE_OK);
    MMAY_EXPECT(work > 0 && work < 512);
    MMAY_EXPECT(mmay_integer_range_begin_block(&analysis, 2));
    instruction = fixture.blocks[2].instructions[0];
    MMAY_EXPECT(mmay_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    mmay_integer_range_analysis_dispose(&analysis);
}

static void mmay_test_range_limits_and_allocations(void) {
    MarimayTestRangeProgram fixture;
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    size_t failure_index = 0;
    size_t work = 99;
    bool reached_success = false;
    mmay_test_range_build_branch(&fixture);
    program = mmay_test_range_adapter(&fixture, mmay_system_allocator());
    memset(&analysis, 0, sizeof(analysis));
    MMAY_EXPECT(mmay_integer_range_analyze(
        &program, 0, &work, &analysis
    ) == MMAY_INTEGER_RANGE_LIMIT_REACHED);
    MMAY_EXPECT(analysis.incoming == NULL && analysis.scratch == NULL);
    for (failure_index = 0; failure_index < 16 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayIntegerRangeStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        program = mmay_test_range_adapter(
            &fixture, mmay_fail_allocator(&state)
        );
        memset(&analysis, 0, sizeof(analysis));
        status = mmay_integer_range_analyze(
            &program, 4096, &work, &analysis
        );
        MMAY_EXPECT(status == MMAY_INTEGER_RANGE_OK
            || status == MMAY_INTEGER_RANGE_ALLOCATION_FAILED);
        reached_success = status == MMAY_INTEGER_RANGE_OK;
        mmay_integer_range_analysis_dispose(&analysis);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_proven_helpers(void) {
    MMAY_EXPECT(mmay_i64_add_proven(INT64_MIN, 0) == INT64_MIN);
    MMAY_EXPECT(mmay_i64_add_proven(-7, 11) == 4);
    MMAY_EXPECT(mmay_i64_subtract_proven(INT64_MAX, INT64_MAX) == 0);
    MMAY_EXPECT(mmay_i64_subtract_proven(-7, 11) == -18);
    MMAY_EXPECT(mmay_i64_multiply_proven(INT64_MIN, 1) == INT64_MIN);
    MMAY_EXPECT(mmay_i64_multiply_proven(-7, -11) == 77);
}

static void mmay_test_fast_helpers_match_checked_boundary(void) {
    int64_t slow = INT64_C(123);
    int64_t fast = INT64_C(123);
    MMAY_EXPECT(mmay_i64_from_twos_complement_fast(UINT64_C(0)) == 0);
    MMAY_EXPECT(mmay_i64_from_twos_complement_fast(
        UINT64_C(0x8000000000000000)
    ) == INT64_MIN);
    MMAY_EXPECT(mmay_i64_from_twos_complement_fast(UINT64_MAX) == -1);
    MMAY_EXPECT(mmay_i64_add_proven_fast(-7, 11)
        == mmay_i64_add_proven(-7, 11));
    MMAY_EXPECT(mmay_i64_subtract_proven_fast(-7, 11)
        == mmay_i64_subtract_proven(-7, 11));
    MMAY_EXPECT(mmay_i64_multiply_proven_fast(-7, -11)
        == mmay_i64_multiply_proven(-7, -11));
    MMAY_EXPECT(mmay_checked_i64_add(1, 2, NULL)
        == mmay_checked_i64_add_fast(1, 2, NULL));
    MMAY_EXPECT(mmay_checked_i64_negate(1, NULL)
        == mmay_checked_i64_negate_fast(1, NULL));

    MMAY_EXPECT(mmay_checked_i64_add(INT64_MAX, 1, &slow)
        == mmay_checked_i64_add_fast(INT64_MAX, 1, &fast));
    MMAY_EXPECT(slow == INT64_C(123) && fast == INT64_C(123));
    MMAY_EXPECT(mmay_checked_i64_subtract(INT64_MIN, 1, &slow)
        == mmay_checked_i64_subtract_fast(INT64_MIN, 1, &fast));
    MMAY_EXPECT(mmay_checked_i64_multiply(INT64_MIN, -1, &slow)
        == mmay_checked_i64_multiply_fast(INT64_MIN, -1, &fast));
    MMAY_EXPECT(mmay_checked_i64_divide(INT64_MIN, -1, &slow)
        == mmay_checked_i64_divide_fast(INT64_MIN, -1, &fast));
    MMAY_EXPECT(mmay_checked_i64_remainder(1, 0, &slow)
        == mmay_checked_i64_remainder_fast(1, 0, &fast));
    MMAY_EXPECT(mmay_checked_i64_negate(INT64_MIN, &slow)
        == mmay_checked_i64_negate_fast(INT64_MIN, &fast));

    MMAY_EXPECT(mmay_checked_i64_multiply(-7, -11, &slow)
        == MMAY_CHECKED_I64_OK);
    MMAY_EXPECT(mmay_checked_i64_multiply_fast(-7, -11, &fast)
        == MMAY_CHECKED_I64_OK);
    MMAY_EXPECT(slow == 77 && fast == slow);
    MMAY_EXPECT(mmay_checked_i64_divide(-7, 2, &slow)
        == MMAY_CHECKED_I64_OK);
    MMAY_EXPECT(mmay_checked_i64_divide_fast(-7, 2, &fast)
        == MMAY_CHECKED_I64_OK);
    MMAY_EXPECT(slow == -3 && fast == slow);
}

void mmay_test_integer_range(void) {
    mmay_test_proven_helpers();
    mmay_test_fast_helpers_match_checked_boundary();
    mmay_test_range_branch_proofs();
    mmay_test_range_retains_overflow();
    mmay_test_range_loop_widening_and_narrowing();
    mmay_test_range_limits_and_allocations();
}
