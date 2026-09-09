#ifndef MMAY_BYTECODE_VM_IMAGE_H
#define MMAY_BYTECODE_VM_IMAGE_H

#include "mmay/bytecode/program.h"
#include "mmay/bytecode/vm.h"

typedef struct MarimayBytecodeImageOperation {
    uint64_t immediate;
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint32_t source_block;
    uint32_t source_instruction;
    uint8_t opcode;
    uint8_t fusion;
    uint16_t reserved;
} MarimayBytecodeImageOperation;

typedef enum MarimayBytecodeImageFusion {
    MMAY_BYTECODE_IMAGE_FUSION_NONE,
    MMAY_BYTECODE_IMAGE_FUSION_COMPARE_BRANCH,
    MMAY_BYTECODE_IMAGE_FUSION_INDUCTION,
    MMAY_BYTECODE_IMAGE_FUSION_ARRAY_READ_BRANCH,
    MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT,
    MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT_LOOP,
    MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_FOLLOW_LOOP,
    MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_COMPRESS_LOOP
} MarimayBytecodeImageFusion;

typedef enum MarimayBytecodeImageRunResult {
    MMAY_BYTECODE_IMAGE_RUN_CONTINUE,
    MMAY_BYTECODE_IMAGE_RUN_STOPPED
} MarimayBytecodeImageRunResult;

typedef struct MarimayBytecodeImageRecordArrayCache {
    MarimayManagedRecordView record;
    MarimayManagedArrayView array;
    bool record_ready;
    bool array_ready;
} MarimayBytecodeImageRecordArrayCache;

typedef struct MarimayBytecodeImageFunction {
    const MarimayBytecodeImageOperation *operations;
    const uint32_t *block_starts;
    size_t operation_count;
    size_t block_count;
    bool has_specialized_loop;
    bool borrows_caller_roots;
    MarimayBytecodeImageRecordArrayCache record_array_cache;
} MarimayBytecodeImageFunction;

typedef struct MarimayBytecodeExecutionImage {
    MarimayAllocator allocator;
    void *storage;
    size_t storage_bytes;
    MarimayBytecodeImageFunction *functions;
    size_t function_count;
    bool initialized;
} MarimayBytecodeExecutionImage;

typedef enum MarimayBytecodeImageStatus {
    MMAY_BYTECODE_IMAGE_OK,
    MMAY_BYTECODE_IMAGE_UNAVAILABLE,
    MMAY_BYTECODE_IMAGE_ALLOCATION_FAILED
} MarimayBytecodeImageStatus;

/* Builds only from an already verified program. UNAVAILABLE is a complete
 * fallback; an allocator failure remains observable to deterministic hosts. */
MarimayBytecodeImageStatus mmay_bytecode_execution_image_init(
    MarimayBytecodeExecutionImage *output,
    const MarimayBytecodeProgram *program, MarimayAllocator allocator);
void mmay_bytecode_execution_image_dispose(
    MarimayBytecodeExecutionImage *image);

const MarimayBytecodeImageOperation *mmay_bytecode_image_operation_at(
    const MarimayBytecodeImageFunction *function, size_t block,
    size_t instruction);

void mmay_bytecode_run_image_hot(MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *block, size_t *instruction,
    size_t *steps, MarimayBytecodeRunOptions options);

MarimayBytecodeImageRunResult mmay_bytecode_run_image_array_shift_loop(
    const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *pc, size_t *steps,
    MarimayBytecodeRunOptions options);

MarimayBytecodeImageRunResult mmay_bytecode_run_image_record_array_loop(
    const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *pc, size_t *steps,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeImageRecordArrayCache *cache);

#endif
