#ifndef RUDA_CHECK_INTERPRET_H
#define RUDA_CHECK_INTERPRET_H

#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/check/result.h"
#include "ruda/mir/interpret.h"

typedef struct RudaCheckInterpretOptions {
    size_t max_steps_per_function;
    size_t max_total_steps;
    size_t max_call_depth;
    /*
     * Optional caller-owned managed-object lifetime. It is required when a
     * managed value crosses this API boundary as an argument or result. When
     * omitted, the interpreter owns a temporary execution for internal values.
     */
    RudaMirExecution *execution;
    /* Optional caller-owned runtime-panic observation for the MIR oracle. */
    RudaMirPanic *panic;
} RudaCheckInterpretOptions;

RudaCheckInterpretOptions ruda_check_interpret_default_options(void);
RudaMirInterpretStatus ruda_check_interpret_function(
    const RudaCheckResult *checked,
    size_t function_index,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaCheckInterpretOptions options,
    RudaMirValue *result
);

#endif
