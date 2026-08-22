#include "test_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/checked_i64.h"
#include "ruda/base/checked_i64_fast.h"
#include "ruda/base/integer_range.h"

typedef struct RudaTestRangeBlock {
    RudaIntegerRangeInstruction instructions[4];
    size_t instruction_count;
    RudaIntegerRangeTerminator terminator;
} RudaTestRangeBlock;

typedef struct RudaTestRangeProgram {
    RudaIntegerRange bounds[8];
    size_t register_count;
    RudaTestRangeBlock blocks[4];
    size_t block_count;
} RudaTestRangeProgram;

static bool ruda_test_range_bounds(
    const void *context,
    uint32_t register_id,
    RudaIntegerRange *range
) {
    const RudaTestRangeProgram *program = context;
    if (program == NULL || range == NULL
        || register_id >= program->register_count) {
        return false;
    }
    *range = program->bounds[register_id];
    return true;
}

static bool ruda_test_range_block_length(
    const void *context,
    size_t block_index,
    size_t *instruction_count
) {
    const RudaTestRangeProgram *program = context;
    if (program == NULL || instruction_count == NULL
        || block_index >= program->block_count) {
        return false;
    }
    *instruction_count = program->blocks[block_index].instruction_count;
    return true;
}

static bool ruda_test_range_instruction(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    RudaIntegerRangeInstruction *instruction
) {
    const RudaTestRangeProgram *program = context;
    if (program == NULL || instruction == NULL
        || block_index >= program->block_count
        || instruction_index
            >= program->blocks[block_index].instruction_count) {
        return false;
    }
    *instruction = program->blocks[block_index].instructions[instruction_index];
    return true;
}

static bool ruda_test_range_terminator(
    const void *context,
    size_t block_index,
    RudaIntegerRangeTerminator *terminator
) {
    const RudaTestRangeProgram *program = context;
    if (program == NULL || terminator == NULL
        || block_index >= program->block_count) {
        return false;
    }
    *terminator = program->blocks[block_index].terminator;
    return true;
}

static RudaIntegerRangeInstruction ruda_test_range_make(
    RudaIntegerRangeInstructionKind kind,
    uint32_t target,
    uint32_t first,
    uint32_t second,
    int64_t constant
) {
    RudaIntegerRangeInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.constant = constant;
    return instruction;
}

static void ruda_test_range_build_branch(RudaTestRangeProgram *program) {
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
    program->blocks[0].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 1, 0, 0, 10
    );
    program->blocks[0].instructions[1] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_LESS, 2, 0, 1, 0
    );
    program->blocks[0].instruction_count = 2;
    program->blocks[0].terminator.kind =
        RUDA_INTEGER_RANGE_TERMINATOR_BRANCH;
    program->blocks[0].terminator.condition = 2;
    program->blocks[0].terminator.target = 1;
    program->blocks[0].terminator.otherwise = 2;
    program->blocks[1].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 3, 0, 0, 1
    );
    program->blocks[1].instructions[1] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_ADD_CHECKED, 4, 0, 3, 0
    );
    program->blocks[1].instruction_count = 2;
    program->blocks[1].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_OTHER;
    program->blocks[2].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 3, 0, 0, 1
    );
    program->blocks[2].instructions[1] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_SUBTRACT_CHECKED, 5, 0, 3, 0
    );
    program->blocks[2].instruction_count = 2;
    program->blocks[2].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_OTHER;
}

static RudaIntegerRangeProgram ruda_test_range_adapter(
    const RudaTestRangeProgram *fixture,
    RudaAllocator allocator
) {
    RudaIntegerRangeProgram program;
    memset(&program, 0, sizeof(program));
    program.allocator = allocator;
    program.context = fixture;
    program.block_count = fixture->block_count;
    program.register_count = fixture->register_count;
    program.register_bounds = ruda_test_range_bounds;
    program.block_length = ruda_test_range_block_length;
    program.read_instruction = ruda_test_range_instruction;
    program.read_terminator = ruda_test_range_terminator;
    return program;
}

static void ruda_test_range_branch_proofs(void) {
    RudaTestRangeProgram fixture;
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    RudaIntegerRangeInstruction instruction;
    size_t work = 0;
    memset(&analysis, 0, sizeof(analysis));
    ruda_test_range_build_branch(&fixture);
    program = ruda_test_range_adapter(&fixture, ruda_system_allocator());
    RUDA_EXPECT(ruda_integer_range_analyze(
        &program, 4096, &work, &analysis
    ) == RUDA_INTEGER_RANGE_OK);
    RUDA_EXPECT(work > 0);

    RUDA_EXPECT(ruda_integer_range_begin_block(&analysis, 1));
    instruction = fixture.blocks[1].instructions[0];
    RUDA_EXPECT(ruda_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[1].instructions[1];
    RUDA_EXPECT(ruda_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    RUDA_EXPECT(ruda_integer_range_advance(&analysis, &instruction));

    RUDA_EXPECT(ruda_integer_range_begin_block(&analysis, 2));
    instruction = fixture.blocks[2].instructions[0];
    RUDA_EXPECT(ruda_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[2].instructions[1];
    RUDA_EXPECT(ruda_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    ruda_integer_range_analysis_dispose(&analysis);
}

static void ruda_test_range_retains_overflow(void) {
    RudaTestRangeProgram fixture;
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    RudaIntegerRangeInstruction instruction;
    size_t work = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&analysis, 0, sizeof(analysis));
    fixture.register_count = 3;
    fixture.block_count = 1;
    fixture.bounds[0] = (RudaIntegerRange){INT64_MIN, INT64_MAX};
    fixture.bounds[1] = fixture.bounds[0];
    fixture.bounds[2] = fixture.bounds[0];
    fixture.blocks[0].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 0, 0, 0, INT64_MAX
    );
    fixture.blocks[0].instructions[1] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 1, 0, 0, 1
    );
    fixture.blocks[0].instructions[2] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_ADD_CHECKED, 2, 0, 1, 0
    );
    fixture.blocks[0].instruction_count = 3;
    fixture.blocks[0].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_OTHER;
    program = ruda_test_range_adapter(&fixture, ruda_system_allocator());
    RUDA_EXPECT(ruda_integer_range_analyze(
        &program, 4096, &work, &analysis
    ) == RUDA_INTEGER_RANGE_OK);
    RUDA_EXPECT(ruda_integer_range_begin_block(&analysis, 0));
    instruction = fixture.blocks[0].instructions[0];
    RUDA_EXPECT(ruda_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[0].instructions[1];
    RUDA_EXPECT(ruda_integer_range_advance(&analysis, &instruction));
    instruction = fixture.blocks[0].instructions[2];
    RUDA_EXPECT(!ruda_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    ruda_integer_range_analysis_dispose(&analysis);
}

static void ruda_test_range_loop_widening_and_narrowing(void) {
    RudaTestRangeProgram fixture;
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    RudaIntegerRangeInstruction instruction;
    size_t index = 0;
    size_t work = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&analysis, 0, sizeof(analysis));
    fixture.register_count = 4;
    fixture.block_count = 4;
    for (index = 0; index < fixture.register_count; index += 1) {
        fixture.bounds[index] = (RudaIntegerRange){INT64_MIN, INT64_MAX};
    }
    fixture.blocks[0].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 0, 0, 0, 0
    );
    fixture.blocks[0].instructions[1] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 1, 0, 0, 100
    );
    fixture.blocks[0].instructions[2] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_CONSTANT, 2, 0, 0, 1
    );
    fixture.blocks[0].instruction_count = 3;
    fixture.blocks[0].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_GOTO;
    fixture.blocks[0].terminator.target = 1;
    fixture.blocks[1].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_LESS, 3, 0, 1, 0
    );
    fixture.blocks[1].instruction_count = 1;
    fixture.blocks[1].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_BRANCH;
    fixture.blocks[1].terminator.condition = 3;
    fixture.blocks[1].terminator.target = 2;
    fixture.blocks[1].terminator.otherwise = 3;
    fixture.blocks[2].instructions[0] = ruda_test_range_make(
        RUDA_INTEGER_RANGE_ADD_CHECKED, 0, 0, 2, 0
    );
    fixture.blocks[2].instruction_count = 1;
    fixture.blocks[2].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_GOTO;
    fixture.blocks[2].terminator.target = 1;
    fixture.blocks[3].terminator.kind = RUDA_INTEGER_RANGE_TERMINATOR_OTHER;
    program = ruda_test_range_adapter(&fixture, ruda_system_allocator());
    RUDA_EXPECT(ruda_integer_range_analyze(
        &program, 512, &work, &analysis
    ) == RUDA_INTEGER_RANGE_OK);
    RUDA_EXPECT(work > 0 && work < 512);
    RUDA_EXPECT(ruda_integer_range_begin_block(&analysis, 2));
    instruction = fixture.blocks[2].instructions[0];
    RUDA_EXPECT(ruda_integer_range_proves_instruction(
        &analysis, &instruction
    ));
    ruda_integer_range_analysis_dispose(&analysis);
}

static void ruda_test_range_limits_and_allocations(void) {
    RudaTestRangeProgram fixture;
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    size_t failure_index = 0;
    size_t work = 99;
    bool reached_success = false;
    ruda_test_range_build_branch(&fixture);
    program = ruda_test_range_adapter(&fixture, ruda_system_allocator());
    memset(&analysis, 0, sizeof(analysis));
    RUDA_EXPECT(ruda_integer_range_analyze(
        &program, 0, &work, &analysis
    ) == RUDA_INTEGER_RANGE_LIMIT_REACHED);
    RUDA_EXPECT(analysis.incoming == NULL && analysis.scratch == NULL);
    for (failure_index = 0; failure_index < 16 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaIntegerRangeStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        program = ruda_test_range_adapter(
            &fixture, ruda_fail_allocator(&state)
        );
        memset(&analysis, 0, sizeof(analysis));
        status = ruda_integer_range_analyze(
            &program, 4096, &work, &analysis
        );
        RUDA_EXPECT(status == RUDA_INTEGER_RANGE_OK
            || status == RUDA_INTEGER_RANGE_ALLOCATION_FAILED);
        reached_success = status == RUDA_INTEGER_RANGE_OK;
        ruda_integer_range_analysis_dispose(&analysis);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_proven_helpers(void) {
    RUDA_EXPECT(ruda_i64_add_proven(INT64_MIN, 0) == INT64_MIN);
    RUDA_EXPECT(ruda_i64_add_proven(-7, 11) == 4);
    RUDA_EXPECT(ruda_i64_subtract_proven(INT64_MAX, INT64_MAX) == 0);
    RUDA_EXPECT(ruda_i64_subtract_proven(-7, 11) == -18);
    RUDA_EXPECT(ruda_i64_multiply_proven(INT64_MIN, 1) == INT64_MIN);
    RUDA_EXPECT(ruda_i64_multiply_proven(-7, -11) == 77);
}

static void ruda_test_fast_helpers_match_checked_boundary(void) {
    int64_t slow = INT64_C(123);
    int64_t fast = INT64_C(123);
    RUDA_EXPECT(ruda_i64_from_twos_complement_fast(UINT64_C(0)) == 0);
    RUDA_EXPECT(ruda_i64_from_twos_complement_fast(
        UINT64_C(0x8000000000000000)
    ) == INT64_MIN);
    RUDA_EXPECT(ruda_i64_from_twos_complement_fast(UINT64_MAX) == -1);
    RUDA_EXPECT(ruda_i64_add_proven_fast(-7, 11)
        == ruda_i64_add_proven(-7, 11));
    RUDA_EXPECT(ruda_i64_subtract_proven_fast(-7, 11)
        == ruda_i64_subtract_proven(-7, 11));
    RUDA_EXPECT(ruda_i64_multiply_proven_fast(-7, -11)
        == ruda_i64_multiply_proven(-7, -11));
    RUDA_EXPECT(ruda_checked_i64_add(1, 2, NULL)
        == ruda_checked_i64_add_fast(1, 2, NULL));
    RUDA_EXPECT(ruda_checked_i64_negate(1, NULL)
        == ruda_checked_i64_negate_fast(1, NULL));

    RUDA_EXPECT(ruda_checked_i64_add(INT64_MAX, 1, &slow)
        == ruda_checked_i64_add_fast(INT64_MAX, 1, &fast));
    RUDA_EXPECT(slow == INT64_C(123) && fast == INT64_C(123));
    RUDA_EXPECT(ruda_checked_i64_subtract(INT64_MIN, 1, &slow)
        == ruda_checked_i64_subtract_fast(INT64_MIN, 1, &fast));
    RUDA_EXPECT(ruda_checked_i64_multiply(INT64_MIN, -1, &slow)
        == ruda_checked_i64_multiply_fast(INT64_MIN, -1, &fast));
    RUDA_EXPECT(ruda_checked_i64_divide(INT64_MIN, -1, &slow)
        == ruda_checked_i64_divide_fast(INT64_MIN, -1, &fast));
    RUDA_EXPECT(ruda_checked_i64_remainder(1, 0, &slow)
        == ruda_checked_i64_remainder_fast(1, 0, &fast));
    RUDA_EXPECT(ruda_checked_i64_negate(INT64_MIN, &slow)
        == ruda_checked_i64_negate_fast(INT64_MIN, &fast));

    RUDA_EXPECT(ruda_checked_i64_multiply(-7, -11, &slow)
        == RUDA_CHECKED_I64_OK);
    RUDA_EXPECT(ruda_checked_i64_multiply_fast(-7, -11, &fast)
        == RUDA_CHECKED_I64_OK);
    RUDA_EXPECT(slow == 77 && fast == slow);
    RUDA_EXPECT(ruda_checked_i64_divide(-7, 2, &slow)
        == RUDA_CHECKED_I64_OK);
    RUDA_EXPECT(ruda_checked_i64_divide_fast(-7, 2, &fast)
        == RUDA_CHECKED_I64_OK);
    RUDA_EXPECT(slow == -3 && fast == slow);
}

void ruda_test_integer_range(void) {
    ruda_test_proven_helpers();
    ruda_test_fast_helpers_match_checked_boundary();
    ruda_test_range_branch_proofs();
    ruda_test_range_retains_overflow();
    ruda_test_range_loop_widening_and_narrowing();
    ruda_test_range_limits_and_allocations();
}
