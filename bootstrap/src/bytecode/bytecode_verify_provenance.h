#ifndef RUDA_BYTECODE_VERIFY_PROVENANCE_H
#define RUDA_BYTECODE_VERIFY_PROVENANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/bytecode/bytecode.h"

typedef struct RudaBytecodeProvenanceAnalysis
    RudaBytecodeProvenanceAnalysis;

/*
 * Owns the hostile-CFG must-analysis for checked array-index carriers and
 * checked enum-variant carriers. The verifier coordinator supplies a validated
 * predecessor table; this owner retains only block-entry facts and one mutable
 * instruction-walk cursor per enabled lattice.
 */
RudaBytecodeVerifyStatus ruda_bytecode_provenance_analyze(
    const RudaBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    bool check_array_indices,
    bool check_enum_variants,
    size_t available_work,
    size_t *work_used,
    RudaBytecodeProvenanceAnalysis **result
);

void ruda_bytecode_provenance_begin_block(
    RudaBytecodeProvenanceAnalysis *analysis,
    size_t block_index
);

RudaBytecodeVerifyStatus ruda_bytecode_provenance_validate_instruction(
    const RudaBytecodeProvenanceAnalysis *analysis,
    const RudaBytecodeInstruction *instruction
);

RudaBytecodeVerifyStatus ruda_bytecode_provenance_advance_instruction(
    RudaBytecodeProvenanceAnalysis *analysis,
    const RudaBytecodeInstruction *instruction,
    size_t available_work,
    size_t *work_used
);

void ruda_bytecode_provenance_dispose(
    RudaBytecodeProvenanceAnalysis *analysis
);

#endif
