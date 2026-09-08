#include "test_checker_support.h"

static void mmay_array_edit_check(const char *source, bool valid) {
    MarimayCheckerFixture fixture;
    bool completed = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (size_t failure = 0; failure < 8192 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_check_ast(&fixture.ast, "test/array-edit-checker",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            completed = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == (valid ? 0U : 1U));
            state.failure_index = SIZE_MAX;
            for (size_t index = 0; valid && index < checked.functions.length; index += 1) {
                const MarimayCheckedFunction *function = mmay_checked_function(&checked, index);
                MMAY_EXPECT(function != NULL);
                if (function != NULL) {
                    MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
                    for (size_t b = 0; b < function->mir.blocks.length; b += 1) {
                        const MarimayMirBlock *block = mmay_vector_at_const(&function->mir.blocks, b);
                        for (size_t n = 0; n < block->instructions.length; n += 1) {
                            const MarimayMirInstruction *op = mmay_vector_at_const(&block->instructions, n);
                            if (op->kind != MMAY_MIR_ARRAY_REMOVE_AT) continue;
                            const MarimayMirInstruction *before = n > 0
                                ? mmay_vector_at_const(&block->instructions, n - 1) : NULL;
                            const MarimayMirInstruction *after = mmay_vector_at_const(&block->instructions, n + 1);
                            MMAY_EXPECT(before != NULL && before->kind == MMAY_MIR_ENUM_NEW);
                            MMAY_EXPECT(after != NULL && after->kind == MMAY_MIR_COPY);
                            MMAY_EXPECT(n + 2 == block->instructions.length);
                            MMAY_EXPECT(block->terminator.kind == MMAY_MIR_GOTO);
                        }
                    }
                }
            }
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_array_edit(void) {
    mmay_array_edit_check(
        "type Bag {}\n"
        "fn copy(bag: Bag): int { return 7; }\n"
        "fn removeAt(bag: Bag, index: int): int { return index; }\n"
        "fn clone<T>(values: []T): []T { return copy(values); }\n"
        "fn remove<T>(values: []T): Option<T> { return values.removeAt(0); }\n"
        "fn main(): int { const bag = Bag {}; const values = [1, 2];\n"
        " const saved: []int = clone(values); const item: Option<int> = remove(saved);\n"
        " const other = values.copy(); const gone: Option<int> = removeAt(other, 1);\n"
        " return bag.copy() + copy(bag) + bag.removeAt(2) + removeAt(bag, 3); }\n", true);
    mmay_array_edit_check("fn main(values: []int): void { const x = values.copy(1); }\n", false);
    mmay_array_edit_check("fn main(values: []int): void { const x = copy(values, 1); }\n", false);
    mmay_array_edit_check("fn main(values: []int): void { const x = values.removeAt(); }\n", false);
    mmay_array_edit_check("fn main(values: []int): void { const x = removeAt(values); }\n", false);
    mmay_array_edit_check("fn main(values: []int): void { const x = values.removeAt(true); }\n", false);
    mmay_array_edit_check("fn main(values: []int): void { const x = values.copy<int>(); }\n", false);
    mmay_array_edit_check("fn main(values: []int): void { const x = removeAt<int>(values, 0); }\n", false);
    mmay_array_edit_check("fn main(values: []int): []bool { return values.copy(); }\n", false);
    mmay_array_edit_check("fn main(values: []int): int { return values.removeAt(0); }\n", false);
    mmay_array_edit_check("fn main(): int { return 3.removeAt(0); }\n", false);
    mmay_array_edit_check("fn main(values: []int): never { const x = values.removeAt(panic(\"stop\")); }\n", true);
}
