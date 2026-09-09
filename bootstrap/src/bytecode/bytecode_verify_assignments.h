#ifndef MMAY_BYTECODE_VERIFY_ASSIGNMENTS_H
#define MMAY_BYTECODE_VERIFY_ASSIGNMENTS_H

#include "mmay/bytecode/bytecode.h"

/* The coordinator has validated the CFG, computed reverse postorder, and allocated the fact matrices and
 * scratch storage. This owner computes definite assignment at every block
 * entry. All storage is borrowed; pending has one size_t per block. */
MarimayBytecodeVerifyStatus mmay_bytecode_verify_assignments(
    const MarimayBytecodeFunction *function,
    const size_t *predecessor_offsets,
    const uint32_t *predecessors,
    const uint32_t *order,
    unsigned char *incoming,
    unsigned char *outgoing,
    unsigned char *scratch,
    size_t *pending,
    size_t available_work,
    size_t *work_used
);

#endif
