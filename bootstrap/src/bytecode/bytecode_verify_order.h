#ifndef MMAY_BYTECODE_VERIFY_ORDER_H
#define MMAY_BYTECODE_VERIFY_ORDER_H

#include "mmay/bytecode/bytecode.h"

/* Validated, nonempty CFG. Each borrowed buffer holds one element per block.
 * Produces reverse postorder from entry 0, or rejects unreachable blocks.
 * No allocation or recursion. Work is one unit per block and outgoing edge. */
MarimayBytecodeVerifyStatus mmay_bytecode_verify_order(
    const MarimayBytecodeFunction *function,
    unsigned char *visited,
    size_t *stack,
    uint32_t *order,
    size_t available_work,
    size_t *work_used
);

#endif
