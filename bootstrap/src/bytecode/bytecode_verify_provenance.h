#ifndef MMAY_BYTECODE_VERIFY_PROVENANCE_H
#define MMAY_BYTECODE_VERIFY_PROVENANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/bytecode/bytecode.h"

typedef struct MarimayBytecodeProvenanceAnalysis
    MarimayBytecodeProvenanceAnalysis;

/*
 * Owns the hostile-CFG must-analysis for checked array-index carriers and
 * checked enum-variant carriers. The verifier coordinator supplies a validated
 * predecessor table and reverse postorder; this owner retains only block-entry facts and one mutable
 * instruction-walk cursor per enabled lattice.
 */
MarimayBytecodeVerifyStatus mmay_bytecode_provenance_analyze(
    const MarimayBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    const uint32_t *order,
    bool check_array_indices,
    bool check_enum_variants,
    size_t available_work,
    size_t *work_used,
    MarimayBytecodeProvenanceAnalysis **result
);

void mmay_bytecode_provenance_begin_block(
    MarimayBytecodeProvenanceAnalysis *analysis,
    size_t block_index
);

MarimayBytecodeVerifyStatus mmay_bytecode_provenance_validate_instruction(
    const MarimayBytecodeProvenanceAnalysis *analysis,
    const MarimayBytecodeInstruction *instruction
);

MarimayBytecodeVerifyStatus mmay_bytecode_provenance_advance_instruction(
    MarimayBytecodeProvenanceAnalysis *analysis,
    const MarimayBytecodeInstruction *instruction,
    size_t available_work,
    size_t *work_used
);

void mmay_bytecode_provenance_dispose(
    MarimayBytecodeProvenanceAnalysis *analysis
);

#endif
