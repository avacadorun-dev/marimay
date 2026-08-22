#ifndef MMAY_CHECK_INTERPRET_H
#define MMAY_CHECK_INTERPRET_H

#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/check/result.h"
#include "mmay/mir/interpret.h"

typedef struct MarimayCheckInterpretOptions {
    size_t max_steps_per_function;
    size_t max_total_steps;
    size_t max_call_depth;
    /*
     * Optional caller-owned managed-object lifetime. It is required when a
     * managed value crosses this API boundary as an argument or result. When
     * omitted, the interpreter owns a temporary execution for internal values.
     */
    MarimayMirExecution *execution;
    const MarimayIoStdout *stdout_sink;
    /* Optional caller-owned runtime-panic observation for the MIR oracle. */
    MarimayMirPanic *panic;
} MarimayCheckInterpretOptions;

MarimayCheckInterpretOptions mmay_check_interpret_default_options(void);
MarimayMirInterpretStatus mmay_check_interpret_function(
    const MarimayCheckResult *checked,
    size_t function_index,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayCheckInterpretOptions options,
    MarimayMirValue *result
);

#endif
