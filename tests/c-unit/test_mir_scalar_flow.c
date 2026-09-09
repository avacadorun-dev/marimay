#include "test_checker_support.h"
#include "test_callable_engine_support.h"
#include "mmay/mir/scalar_flow.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/codec.h"
#include "mmay/bytecode/vm.h"

#include <limits.h>

static MarimayCheckedFunction *flow_checked(MarimayCheckerFixture *fixture,
    MarimayCheckResult *checked, MarimayAllocator allocator) {
    MMAY_EXPECT(mmay_check_ast(&fixture->ast, "test/scalar-flow", allocator,
        mmay_check_default_options(), checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked->diagnostics.length == 0 && checked->functions.length == 1);
    return checked->diagnostics.length == 0 && checked->functions.length == 1
        ? mmay_vector_at(&checked->functions, 0) : NULL;
}

typedef struct FlowObservation {
    MarimayMirInterpretStatus mir_status;
    int64_t mir_value;
    MarimayBytecodeRunStatus vm_status;
    int64_t vm_value;
    MarimayBytecodeTrap trap;
} FlowObservation;

static FlowObservation flow_observe(const MarimayMirFunction *function, int64_t input) {
    FlowObservation observed = {0};
    MarimayMirValue argument = {0}, result = {0};
    argument.type = MMAY_TYPE_I64;
    argument.as.i64 = input;
    result.type = MMAY_TYPE_I64;
    result.as.i64 = 123456;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    options.max_steps = 10000;
    observed.mir_status = mmay_mir_interpret(function, &argument, 1,
        mmay_system_allocator(), options, &result);
    observed.mir_value = result.as.i64;
    MarimayBytecodeFunction lowered = {0};
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered) == MMAY_MIR_BYTECODE_LOWER_OK);
    MarimayBytecodeValue vm_argument = {0}, vm_result = {0};
    vm_argument.type = MMAY_BYTECODE_TYPE_I64;
    vm_argument.as.i64 = input;
    vm_result.type = MMAY_BYTECODE_TYPE_I64;
    vm_result.as.i64 = 123456;
    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
    run.max_steps = 10000;
    run.trap = &observed.trap;
    observed.vm_status = mmay_bytecode_run(&lowered, &vm_argument, 1,
        mmay_system_allocator(), run, &vm_result);
    observed.vm_value = vm_result.as.i64;
    mmay_bytecode_function_dispose(&lowered);
    return observed;
}

static void flow_semantics(const char *source, bool diverges, bool shrinks) {
    static const int64_t inputs[] = {INT64_MIN, INT64_MAX, -17, -1, 0, 1, 17};
    MarimayCheckerFixture fixture = {0};
    MarimayCheckResult checked = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MarimayCheckedFunction *function = flow_checked(&fixture, &checked, mmay_system_allocator());
    if (function != NULL) {
        FlowObservation before[sizeof(inputs) / sizeof(inputs[0])];
        for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i += 1) before[i] = flow_observe(&function->mir, inputs[i]);
        size_t instructions = function->mir.instruction_count;
        MMAY_EXPECT(mmay_mir_simplify_scalar_flow(&function->mir) == MMAY_MIR_SCALAR_FLOW_OK);
        if (shrinks) MMAY_EXPECT(function->mir.instruction_count < instructions);
        for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i += 1) {
            FlowObservation after = flow_observe(&function->mir, inputs[i]);
            MMAY_EXPECT(before[i].mir_status == after.mir_status);
            MMAY_EXPECT(before[i].vm_status == after.vm_status);
            MMAY_EXPECT(before[i].mir_value == after.mir_value);
            MMAY_EXPECT(before[i].vm_value == after.vm_value);
            if (!diverges) {
                MMAY_EXPECT(before[i].trap.has_span == after.trap.has_span);
                if (after.trap.has_span) {
                    MMAY_EXPECT(before[i].trap.span.start == after.trap.span.start);
                    MMAY_EXPECT(before[i].trap.span.end == after.trap.span.end);
                }
            } else {
                MMAY_EXPECT(after.mir_status == MMAY_MIR_INTERPRET_STEP_LIMIT);
                MMAY_EXPECT(after.vm_status == MMAY_BYTECODE_RUN_STEP_LIMIT);
            }
        }
    }
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

static void flow_encoded(const MarimayMirFunction *function, MarimayOwnedBytes *encoded) {
    MarimayBytecodeFunction lowered = {0};
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &lowered) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(&lowered, mmay_system_allocator(),
        encoded) == MMAY_BYTECODE_CODEC_OK);
    mmay_bytecode_function_dispose(&lowered);
}

static void flow_failures(void) {
    const char *source = "fn choose(input:int):int{let value=0;"
        "for i in 0..8{if i<4&&(input<0||i==6){value+=i;}else{value+=1;}}"
        "return value;}";
    MarimayCheckerFixture fixture = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    bool completed = false;
    for (size_t failure = 0; failure < 1024 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayCheckedFunction *function = flow_checked(&fixture, &checked, mmay_fail_allocator(&state));
        if (function != NULL) {
            MarimayOwnedBytes before = {0}, after = {0};
            flow_encoded(&function->mir, &before);
            MarimayMirFunction original = function->mir;
            state.failure_index = state.attempts + failure;
            MarimayMirScalarFlowStatus status = mmay_mir_simplify_scalar_flow(&function->mir);
            completed = status == MMAY_MIR_SCALAR_FLOW_OK;
            MMAY_EXPECT(completed || status == MMAY_MIR_SCALAR_FLOW_ALLOCATION_FAILED);
            state.failure_index = SIZE_MAX;
            if (!completed) {
                MMAY_EXPECT(memcmp(&original, &function->mir, sizeof(original)) == 0);
                flow_encoded(&function->mir, &after);
                MMAY_EXPECT(before.length == after.length
                    && memcmp(before.data, after.data, before.length) == 0);
            }
            MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
            mmay_owned_bytes_dispose(&after);
            mmay_owned_bytes_dispose(&before);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    /* Sweep through verifier rejection and mid-transformation exhaustion. */
    bool exhausted_after_verification = false, work_completed = false;
    for (size_t budget = 1; budget <= 131072 && !work_completed; budget += 128) {
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayCheckResult checked = {0};
        MarimayCheckedFunction *function = flow_checked(&fixture, &checked, mmay_fail_allocator(&state));
        if (function != NULL) {
            MarimayOwnedBytes before = {0}, after = {0};
            flow_encoded(&function->mir, &before);
            function->mir.options.max_verify_work = budget;
            bool input_valid = mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK;
            MarimayMirFunction original = function->mir;
            MarimayMirScalarFlowStatus status = mmay_mir_simplify_scalar_flow(&function->mir);
            work_completed = status == MMAY_MIR_SCALAR_FLOW_OK;
            MMAY_EXPECT(work_completed || status == MMAY_MIR_SCALAR_FLOW_LIMIT_REACHED);
            if (!work_completed) {
                exhausted_after_verification |= input_valid;
                MMAY_EXPECT(memcmp(&original, &function->mir, sizeof(original)) == 0);
                function->mir.options.max_verify_work = mmay_mir_function_default_options().max_verify_work;
                flow_encoded(&function->mir, &after);
                MMAY_EXPECT(before.length == after.length
                    && memcmp(before.data, after.data, before.length) == 0);
            }
            mmay_owned_bytes_dispose(&after);
            mmay_owned_bytes_dispose(&before);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(exhausted_after_verification && work_completed);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_mir_scalar_flow(void) {
    flow_semantics("fn f(input:int):int{let x=input;const saved=x;x=7;"
        "let total=0;for i in 0..12{let value=i;const old=value;value+=10;"
        "if i%2==0&&(i<9||i==10){total+=i;}else{total+=1;}"
        "total+=old;total+=value;}if saved<0{return total+x;}return total;}", false, true);
    flow_semantics("fn f(input:int):int{const unused=input+1;return 5;}", false, false);
    flow_semantics("fn f(input:int):int{const unused=1/input;return 5;}", false, false);
    flow_semantics("fn f(input:int):int{const unused=input/-1;return 5;}", false, false);
    flow_semantics("fn f(input:int):int{const unused=9223372036854775807+input;return 5;}", false, false);
    flow_semantics("fn f(input:int):int{let flag=input<0;const old=flag;"
        "flag=!flag;if old&&flag{return 1;}if old{return 2;}return 3;}", false, true);
    flow_semantics("fn f(input:int):int{while true{const unused=7;}return input;}", true, false);
    flow_failures();
    static const char source[] =
        "type Box{mutable value:int}fn add(box:Box,value:int):void{box.value+=value;}"
        "fn main():int{let x=3;const old=x;x=7;let sum=0;"
        "for i in 0..12{if i%2==0&&(i<9||i==10){sum+=i;}else{sum+=1;}"
        "let value=i;const snapshot=value;value+=10;sum+=snapshot;sum+=value;}"
        "const box=Box{value:1};{defer add(box,19);const unused=42;}"
        "const nested=[[old]];const names=[\"abc\"];const alias=nested[0];"
        "alias[0]+=box.value;const f=(value:int):int=>value+old;"
        "return sum+alias[0]+f(2)+names.length();}";
    mmay_test_callable_engines_prepared(source, 317, 1, 1);
}
