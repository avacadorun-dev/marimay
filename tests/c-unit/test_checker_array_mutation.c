#include "test_checker_support.h"

#include <string.h>

#include "ruda/check/checker.h"

static void ruda_test_checker_array_mutation_surface(void) {
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
        "fn iterate(values: Array<int>): void { for const item of values { let copy = item; } }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    size_t index = 0;
    bool saw_pop = false;
    bool saw_truncate = false;
    bool saw_clear = false;
    bool saw_reserve = false;
    bool saw_push = false;
    bool saw_generation = false;
    bool saw_require_generation = false;
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(&fixture.ast, "test/checker-array-mutation",
        ruda_system_allocator(), ruda_check_default_options(), &checked)
        == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    for (index = 0; index < checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(&checked, index);
        size_t block_index = 0;
        for (block_index = 0; function != NULL && block_index < function->mir.blocks.length;
            block_index += 1) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->mir.blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const RudaMirInstruction *instruction = ruda_vector_at_const(
                    &block->instructions, instruction_index
                );
                saw_pop = saw_pop || (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_POP);
                saw_truncate = saw_truncate || (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_TRUNCATE);
                saw_clear = saw_clear || (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_CLEAR);
                saw_reserve = saw_reserve || (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_RESERVE);
                saw_push = saw_push || (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_PUSH);
                saw_generation = saw_generation || (instruction != NULL && instruction->kind == RUDA_MIR_ARRAY_GENERATION);
                saw_require_generation = saw_require_generation || (
                    instruction != NULL
                    && instruction->kind == RUDA_MIR_ARRAY_REQUIRE_GENERATION
                );
            }
        }
    }
    RUDA_EXPECT(saw_pop && saw_truncate && saw_clear && saw_reserve);
    RUDA_EXPECT(saw_push && saw_generation && saw_require_generation);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_array_mutation_diagnostics(void) {
    ruda_checker_expect_one_diagnostic(
        "fn invalid(values: Array<int>): void { values.truncate(true); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH, "true", 0
    );
    ruda_checker_expect_one_diagnostic(
        "fn invalid(values: Array<int>): void { values.reserve(); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY, "()", 0
    );
}

static void ruda_test_checker_array_mutation_allocation_failures(void) {
    static const char source[] =
        "fn mutate(values: Array<int>): void {\n"
        "  values.push(1); values.pop(); values.truncate(0);\n"
        "  values.reserve(8); values.clear();\n"
        "  for const value of values { let copy = value; }\n"
        "}\n";
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 2048; failure_index += 1) {
        RudaFailAllocator state;
        RudaCheckResult checked;
        RudaCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_check_ast(
            &fixture.ast,
            "test/checker-array-mutation-allocation",
            ruda_fail_allocator(&state),
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(
                checked.initialized && checked.diagnostics.length == 0
            );
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_array_mutation(void) {
    ruda_test_checker_array_mutation_surface();
    ruda_test_checker_array_mutation_diagnostics();
    ruda_test_checker_array_mutation_allocation_failures();
}
