#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"
#include "test_callable_engine_support.h"
#include "mmay/mir/scalar_flow.h"
#include "../../bootstrap/src/native/c_emitter_internal.h"

static void fill_plan(bool prepare, bool zero) {
    const char *source = zero
        ? "fn fill(count:int):[]int{let values=arrayOf<int>();for i in 0..count{const _=push(values,-1);}return values;}"
        : "fn fill(count:int):[]int{let values=arrayOf<int>();for i in 1..count{const _=push(values,-1);}return values;}";
    MarimayCheckerFixture fixture = {0};
    MarimayFailAllocator state;
    MarimayCheckResult checked = {0};
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(&fixture.ast, "test/fill", mmay_fail_allocator(&state),
        mmay_check_default_options(), &checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length == 0 && checked.functions.length == 1);
    if (checked.functions.length == 1 && checked.diagnostics.length == 0) {
        MarimayCheckedFunction *function = mmay_vector_at(&checked.functions, 0);
        if (prepare) MMAY_EXPECT(mmay_mir_simplify_scalar_flow(&function->mir) == MMAY_MIR_SCALAR_FLOW_OK);
        const MarimayMirInstruction *push = NULL;
        for (size_t b = 0; b < function->mir.blocks.length; b += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(&function->mir.blocks, b);
            for (size_t i = 0; i < block->instructions.length; i += 1) {
                const MarimayMirInstruction *instruction = mmay_vector_at_const(&block->instructions, i);
                if (instruction->kind == MMAY_MIR_ARRAY_PUSH) push = instruction;
            }
        }
        MMAY_EXPECT(push != NULL);
        bool completed = false;
        for (size_t failure = 0; push != NULL && failure < 2048 && !completed; failure += 1) {
            MarimayMirRegister count = MMAY_MIR_NO_REGISTER, index = MMAY_MIR_NO_REGISTER;
            uint64_t bits = 0;
            MarimayMirBlockId exit = MMAY_MIR_NO_BLOCK;
            size_t live = state.live_allocations, bytes = state.live_bytes;
            state.attempts = 0;
            state.failure_index = failure;
            bool planned = mmay_native_c_array_fill_plan(&function->mir, push, &count, &index, &bits, &exit);
            completed = state.attempts <= failure;
            if (completed) MMAY_EXPECT(planned == zero);
            if (planned) MMAY_EXPECT(zero && bits == UINT64_MAX
                && count != MMAY_MIR_NO_REGISTER && index != MMAY_MIR_NO_REGISTER
                && exit != MMAY_MIR_NO_BLOCK);
            MMAY_EXPECT(state.live_allocations == live && state.live_bytes == bytes);
        }
        MMAY_EXPECT(completed);
        state.failure_index = SIZE_MAX;
    }
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_native_c_emitter_fill(void) {
    for (unsigned prepare = 0; prepare < 2; prepare += 1) {
        fill_plan(prepare != 0, true);
        fill_plan(prepare != 0, false);
    }
    static const char source[] =
        "fn main():int{let values=arrayOf<int>();for i in 0..20{const _=push(values,-1);}"
        "values[7]=3;return values[7]+values[19]+values.length();}";
    mmay_test_callable_engines_prepared(source, 22, 0, 0);
}
