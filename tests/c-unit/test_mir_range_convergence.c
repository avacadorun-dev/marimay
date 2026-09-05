#include "test_checker_support.h"

#include "mmay/mir/range.h"

/* These small real CFGs used to spend 100,000,000 work units narrowing
 * loop-carried temporary bounds. A work budget is deterministic across hosts;
 * a stopwatch assertion would conflate the regression with runner load. */
static const char *const mmay_range_convergence_sources[] = {
    "fn find(values: []int, target: int): int {"
    " for value in values { if value == target { return value; } }"
    " return -1; }",
    "fn copy(values: []int): []int { let result: []int = [];"
    " for value in values { const _ = result.push(value); } return result; }",
    "fn reverse(values: []int): int { let index = values.length();"
    " while index > 0 { index -= 1; if values[index] == 0 { return index; } }"
    " return -1; }",
    "fn nested(values: [][]int): int { for row in values {"
    " for value in row { if value == 0 { return value; } } } return -1; }"
};

static void mmay_test_range_convergence_case(const char *source) {
    MarimayCheckerFixture fixture;
    size_t attempts = 0;
    size_t iteration = 0;
    if (!mmay_checker_fixture_init(&fixture, source)) {
        MMAY_EXPECT(false);
        return;
    }
    do {
        MarimayFailAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckedFunction *function;
        MarimayMirRangeOptimizeStatus status;
        size_t start = 0;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&allocator, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_check_ast(&fixture.ast, "test/range-convergence",
            mmay_fail_allocator(&allocator), mmay_check_default_options(),
            &checked) == MMAY_CHECK_OK);
        MMAY_EXPECT(checked.diagnostics.length == 0 && checked.functions.length == 1);
        function = mmay_vector_at(&checked.functions, 0);
        if (function == NULL || checked.diagnostics.length != 0) {
            mmay_check_result_dispose(&checked);
            MMAY_EXPECT(allocator.live_allocations == 0 && allocator.live_bytes == 0);
            break;
        }
        function->mir.options.max_verify_work = 16384;
        start = allocator.attempts;
        if (iteration != 0) allocator.failure_index = start + iteration - 1;
        status = mmay_mir_optimize_ranges(&function->mir);
        if (iteration == 0) {
            attempts = allocator.attempts - start;
            MMAY_EXPECT(status == MMAY_MIR_RANGE_OPTIMIZE_OK);
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &function->mir, MMAY_MIR_ADD_PROVEN_I64)
                + mmay_test_mir_instruction_kind_count(
                    &function->mir, MMAY_MIR_SUBTRACT_PROVEN_I64) > 0);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_RANGE_OPTIMIZE_OK
                || status == MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED);
        }
        /* Optimizer failure must leave a valid checked/proven program, not
         * publish a partially reconstructed proof. */
        allocator.failure_index = SIZE_MAX;
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(allocator.live_allocations == 0);
        MMAY_EXPECT(allocator.live_bytes == 0);
        iteration += 1;
    } while (iteration <= attempts + 1);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_mir_range_convergence(void) {
    size_t index;
    for (index = 0; index < sizeof(mmay_range_convergence_sources)
        / sizeof(mmay_range_convergence_sources[0]); index += 1) {
        mmay_test_range_convergence_case(mmay_range_convergence_sources[index]);
    }
}
