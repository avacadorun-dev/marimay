#ifndef MMAY_BASE_INTEGER_RANGE_H
#define MMAY_BASE_INTEGER_RANGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"

#define MMAY_INTEGER_RANGE_NO_REGISTER UINT32_MAX

typedef struct MarimayIntegerRange {
    int64_t minimum;
    int64_t maximum;
} MarimayIntegerRange;

typedef enum MarimayIntegerRangeInstructionKind {
    MMAY_INTEGER_RANGE_OTHER,
    MMAY_INTEGER_RANGE_CONSTANT,
    MMAY_INTEGER_RANGE_COPY,
    MMAY_INTEGER_RANGE_ADD_CHECKED,
    MMAY_INTEGER_RANGE_ADD_PROVEN,
    MMAY_INTEGER_RANGE_SUBTRACT_CHECKED,
    MMAY_INTEGER_RANGE_SUBTRACT_PROVEN,
    MMAY_INTEGER_RANGE_MULTIPLY_CHECKED,
    MMAY_INTEGER_RANGE_MULTIPLY_PROVEN,
    MMAY_INTEGER_RANGE_REMAINDER_CHECKED,
    MMAY_INTEGER_RANGE_CONVERT_TOTAL,
    MMAY_INTEGER_RANGE_ARRAY_LENGTH,
    MMAY_INTEGER_RANGE_LESS,
    MMAY_INTEGER_RANGE_LESS_EQUAL
} MarimayIntegerRangeInstructionKind;

typedef struct MarimayIntegerRangeInstruction {
    MarimayIntegerRangeInstructionKind kind;
    uint32_t target;
    uint32_t first;
    uint32_t second;
    int64_t constant;
} MarimayIntegerRangeInstruction;

typedef enum MarimayIntegerRangeTerminatorKind {
    MMAY_INTEGER_RANGE_TERMINATOR_OTHER,
    MMAY_INTEGER_RANGE_TERMINATOR_GOTO,
    MMAY_INTEGER_RANGE_TERMINATOR_BRANCH
} MarimayIntegerRangeTerminatorKind;

typedef struct MarimayIntegerRangeTerminator {
    MarimayIntegerRangeTerminatorKind kind;
    uint32_t condition;
    uint32_t target;
    uint32_t otherwise;
} MarimayIntegerRangeTerminator;

typedef bool (*MarimayIntegerRangeRegisterBounds)(
    const void *context,
    uint32_t register_id,
    MarimayIntegerRange *range
);
typedef bool (*MarimayIntegerRangeBlockLength)(
    const void *context,
    size_t block_index,
    size_t *instruction_count
);
typedef bool (*MarimayIntegerRangeReadInstruction)(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    MarimayIntegerRangeInstruction *instruction
);
typedef bool (*MarimayIntegerRangeReadTerminator)(
    const void *context,
    size_t block_index,
    MarimayIntegerRangeTerminator *terminator
);

typedef struct MarimayIntegerRangeProgram {
    MarimayAllocator allocator;
    const void *context;
    size_t block_count;
    size_t register_count;
    MarimayIntegerRangeRegisterBounds register_bounds;
    MarimayIntegerRangeBlockLength block_length;
    MarimayIntegerRangeReadInstruction read_instruction;
    MarimayIntegerRangeReadTerminator read_terminator;
} MarimayIntegerRangeProgram;

typedef enum MarimayIntegerRangeStatus {
    MMAY_INTEGER_RANGE_OK,
    MMAY_INTEGER_RANGE_INVALID,
    MMAY_INTEGER_RANGE_LIMIT_REACHED,
    MMAY_INTEGER_RANGE_ALLOCATION_FAILED
} MarimayIntegerRangeStatus;

typedef struct MarimayIntegerRangeAnalysis {
    MarimayIntegerRangeProgram program;
    MarimayIntegerRange *incoming;
    MarimayIntegerRange *scratch;
    unsigned char *incoming_ready;
    size_t cell_count;
    size_t cell_bytes;
    size_t scratch_bytes;
    size_t ready_bytes;
} MarimayIntegerRangeAnalysis;

MarimayIntegerRangeStatus mmay_integer_range_analyze(
    const MarimayIntegerRangeProgram *program,
    size_t work_limit,
    size_t *work_used,
    MarimayIntegerRangeAnalysis *analysis
);
void mmay_integer_range_analysis_dispose(
    MarimayIntegerRangeAnalysis *analysis
);
bool mmay_integer_range_begin_block(
    MarimayIntegerRangeAnalysis *analysis,
    size_t block_index
);
bool mmay_integer_range_proves_instruction(
    const MarimayIntegerRangeAnalysis *analysis,
    const MarimayIntegerRangeInstruction *instruction
);
bool mmay_integer_range_advance(
    MarimayIntegerRangeAnalysis *analysis,
    const MarimayIntegerRangeInstruction *instruction
);

#endif
