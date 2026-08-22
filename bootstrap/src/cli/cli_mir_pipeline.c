#include "cli_internal.h"

#include "ruda/mir/range.h"

bool ruda_cli_optimize_checked_mir(RudaCompilerResult *compiler) {
    size_t index = 0;
    if (compiler == NULL || !compiler->initialized) {
        return false;
    }
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        RudaCheckedFunction *function = ruda_vector_at(
            &compiler->checked.functions, index
        );
        RudaMirRangeOptimizeStatus status;
        if (function == NULL || function->is_external) {
            continue;
        }
        status = ruda_mir_optimize_ranges(&function->mir);
        if (status == RUDA_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT
            || status == RUDA_MIR_RANGE_OPTIMIZE_INVALID_MIR) {
            return false;
        }
        /* Resource failure is a documented checked-MIR fallback. */
    }
    return true;
}
