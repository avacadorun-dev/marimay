#include "cli_internal.h"

#include "mmay/mir/range.h"

bool mmay_cli_optimize_checked_mir(MarimayCompilerResult *compiler) {
    size_t index = 0;
    if (compiler == NULL || !compiler->initialized) {
        return false;
    }
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        MarimayCheckedFunction *function = mmay_vector_at(
            &compiler->checked.functions, index
        );
        MarimayMirRangeOptimizeStatus status;
        if (function == NULL || function->is_external) {
            continue;
        }
        status = mmay_mir_optimize_ranges(&function->mir);
        if (status == MMAY_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT
            || status == MMAY_MIR_RANGE_OPTIMIZE_INVALID_MIR) {
            return false;
        }
        /* Resource failure is a documented checked-MIR fallback. */
    }
    return true;
}
