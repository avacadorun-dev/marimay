#include "test_checker_support.h"

#include <string.h>

#include "mmay/check/checker.h"

static void mmay_test_checker_array_mutation_surface(void) {
    static const char source[] =
        "fn values(): void {\n"
        "  let raw = [1, 2];\n"
        "  let first: Option<int> = pop(raw);\n"
        "  let second: Option<int> = raw.pop();\n"
        "  let shortened: Result<void, ArrayTruncateError> = raw.truncate(0);\n"
        "  let reserved: Result<void, ArrayGrowthError> = reserve(raw, 4);\n"
        "  raw.push(3);\n"
        "  raw.clear();\n"
        "}\n"
        "type Stack {}\n"
        "fn pop(stack: Stack): int { return 1; }\n"
        "fn collision(stack: Stack): int { return stack.pop(); }\n"
        "fn iterate(values: []int): void { for const item of values { let copy = item; } }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    size_t index = 0;
    bool saw_pop = false;
    bool saw_truncate = false;
    bool saw_clear = false;
    bool saw_reserve = false;
    bool saw_push = false;
    bool saw_generation = false;
    bool saw_require_generation = false;
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(&fixture.ast, "test/checker-array-mutation",
        mmay_system_allocator(), mmay_check_default_options(), &checked)
        == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    for (index = 0; index < checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(&checked, index);
        size_t block_index = 0;
        for (block_index = 0; function != NULL && block_index < function->mir.blocks.length;
            block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(
                &function->mir.blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const MarimayMirInstruction *instruction = mmay_vector_at_const(
                    &block->instructions, instruction_index
                );
                saw_pop = saw_pop || (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_POP);
                saw_truncate = saw_truncate || (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_TRUNCATE);
                saw_clear = saw_clear || (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_CLEAR);
                saw_reserve = saw_reserve || (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_RESERVE);
                saw_push = saw_push || (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_PUSH);
                saw_generation = saw_generation || (instruction != NULL && instruction->kind == MMAY_MIR_ARRAY_GENERATION);
                saw_require_generation = saw_require_generation || (
                    instruction != NULL
                    && instruction->kind == MMAY_MIR_ARRAY_REQUIRE_GENERATION
                );
            }
        }
    }
    MMAY_EXPECT(saw_pop && saw_truncate && saw_clear && saw_reserve);
    MMAY_EXPECT(saw_push && saw_generation && saw_require_generation);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_checker_array_mutation_diagnostics(void) {
    mmay_checker_expect_one_diagnostic(
        "fn invalid(values: []int): void { values.truncate(true); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "true", 0
    );
    mmay_checker_expect_one_diagnostic(
        "fn invalid(values: []int): void { values.reserve(); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY, "()", 0
    );
}

static void mmay_test_checker_array_mutation_allocation_failures(void) {
    static const char source[] =
        "fn mutate(values: []int): void {\n"
        "  values.push(1); values.pop(); values.truncate(0);\n"
        "  values.reserve(8); values.clear();\n"
        "  for const value of values { let copy = value; }\n"
        "}\n";
    MarimayCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 2048; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_check_ast(
            &fixture.ast,
            "test/checker-array-mutation-allocation",
            mmay_fail_allocator(&state),
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(
                checked.initialized && checked.diagnostics.length == 0
            );
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_array_mutation(void) {
    mmay_test_checker_array_mutation_surface();
    mmay_test_checker_array_mutation_diagnostics();
    mmay_test_checker_array_mutation_allocation_failures();
}
