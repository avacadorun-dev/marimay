#ifndef RUDA_BASE_INTEGER_RANGE_H
#define RUDA_BASE_INTEGER_RANGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"

#define RUDA_INTEGER_RANGE_NO_REGISTER UINT32_MAX

typedef struct RudaIntegerRange {
    int64_t minimum;
    int64_t maximum;
} RudaIntegerRange;

typedef enum RudaIntegerRangeInstructionKind {
    RUDA_INTEGER_RANGE_OTHER,
    RUDA_INTEGER_RANGE_CONSTANT,
    RUDA_INTEGER_RANGE_COPY,
    RUDA_INTEGER_RANGE_ADD_CHECKED,
    RUDA_INTEGER_RANGE_ADD_PROVEN,
    RUDA_INTEGER_RANGE_SUBTRACT_CHECKED,
    RUDA_INTEGER_RANGE_SUBTRACT_PROVEN,
    RUDA_INTEGER_RANGE_MULTIPLY_CHECKED,
    RUDA_INTEGER_RANGE_MULTIPLY_PROVEN,
    RUDA_INTEGER_RANGE_REMAINDER_CHECKED,
    RUDA_INTEGER_RANGE_CONVERT_TOTAL,
    RUDA_INTEGER_RANGE_ARRAY_LENGTH,
    RUDA_INTEGER_RANGE_LESS,
    RUDA_INTEGER_RANGE_LESS_EQUAL
} RudaIntegerRangeInstructionKind;

typedef struct RudaIntegerRangeInstruction {
    RudaIntegerRangeInstructionKind kind;
    uint32_t target;
    uint32_t first;
    uint32_t second;
    int64_t constant;
} RudaIntegerRangeInstruction;

typedef enum RudaIntegerRangeTerminatorKind {
    RUDA_INTEGER_RANGE_TERMINATOR_OTHER,
    RUDA_INTEGER_RANGE_TERMINATOR_GOTO,
    RUDA_INTEGER_RANGE_TERMINATOR_BRANCH
} RudaIntegerRangeTerminatorKind;

typedef struct RudaIntegerRangeTerminator {
    RudaIntegerRangeTerminatorKind kind;
    uint32_t condition;
    uint32_t target;
    uint32_t otherwise;
} RudaIntegerRangeTerminator;

typedef bool (*RudaIntegerRangeRegisterBounds)(
    const void *context,
    uint32_t register_id,
    RudaIntegerRange *range
);
typedef bool (*RudaIntegerRangeBlockLength)(
    const void *context,
    size_t block_index,
    size_t *instruction_count
);
typedef bool (*RudaIntegerRangeReadInstruction)(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    RudaIntegerRangeInstruction *instruction
);
typedef bool (*RudaIntegerRangeReadTerminator)(
    const void *context,
    size_t block_index,
    RudaIntegerRangeTerminator *terminator
);

typedef struct RudaIntegerRangeProgram {
    RudaAllocator allocator;
    const void *context;
    size_t block_count;
    size_t register_count;
    RudaIntegerRangeRegisterBounds register_bounds;
    RudaIntegerRangeBlockLength block_length;
    RudaIntegerRangeReadInstruction read_instruction;
    RudaIntegerRangeReadTerminator read_terminator;
} RudaIntegerRangeProgram;

typedef enum RudaIntegerRangeStatus {
    RUDA_INTEGER_RANGE_OK,
    RUDA_INTEGER_RANGE_INVALID,
    RUDA_INTEGER_RANGE_LIMIT_REACHED,
    RUDA_INTEGER_RANGE_ALLOCATION_FAILED
} RudaIntegerRangeStatus;

typedef struct RudaIntegerRangeAnalysis {
    RudaIntegerRangeProgram program;
    RudaIntegerRange *incoming;
    RudaIntegerRange *scratch;
    unsigned char *incoming_ready;
    size_t cell_count;
    size_t cell_bytes;
    size_t scratch_bytes;
    size_t ready_bytes;
} RudaIntegerRangeAnalysis;

RudaIntegerRangeStatus ruda_integer_range_analyze(
    const RudaIntegerRangeProgram *program,
    size_t work_limit,
    size_t *work_used,
    RudaIntegerRangeAnalysis *analysis
);
void ruda_integer_range_analysis_dispose(
    RudaIntegerRangeAnalysis *analysis
);
bool ruda_integer_range_begin_block(
    RudaIntegerRangeAnalysis *analysis,
    size_t block_index
);
bool ruda_integer_range_proves_instruction(
    const RudaIntegerRangeAnalysis *analysis,
    const RudaIntegerRangeInstruction *instruction
);
bool ruda_integer_range_advance(
    RudaIntegerRangeAnalysis *analysis,
    const RudaIntegerRangeInstruction *instruction
);

#endif
