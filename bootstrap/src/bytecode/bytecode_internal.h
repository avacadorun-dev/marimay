#ifndef RUDA_BYTECODE_INTERNAL_H
#define RUDA_BYTECODE_INTERNAL_H

#include <stddef.h>

#include "ruda/bytecode/bytecode.h"

/* Internal entry point for composing a function verifier into a shared budget. */
RudaBytecodeVerifyStatus ruda_bytecode_verify_with_work_limit(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);

#endif
