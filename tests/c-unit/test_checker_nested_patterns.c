#include "test_checker_support.h"

/* D092 applies to every pattern spelling before locals or MIR are built. */
static void mmay_void_pattern_diagnostics(void) {
    static const char *sources[] = {
        "fn f(r:Result<void,int>):int{return match r{Ok(item)=>0,Err(_)=>1,};}",
        "fn f(r:Result<int,void>):int{return match r{Ok(_)=>0,Err(item)=>1,};}",
        "fn f(r:Option<void>):int{return match r{Some(item)=>0,None=>1,};}",
        "fn f(r:Result<void,int>):int{return match r{Ok(_ as item)=>0,Err(_)=>1,};}",
        "fn f(r:Option<Result<void,int>>):int{return match r{Some(Ok(item))=>0,Some(Err(_))=>1,None=>2,};}",
        "enum Box<T>{Value{item:T,},}fn f(r:Box<void>):int{return match r{Value{item}=>0,};}",
        "enum Box<T>{Value{v:T,},}fn f(r:Box<void>):int{return match r{Value{v:item}=>0,};}",
    };
    for (size_t sample = 0; sample < sizeof(sources) / sizeof(sources[0]); sample += 1) {
        const char *source = sources[sample];
        mmay_checker_expect_one_diagnostic(source, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "item", sample == 5 ? 1 : 0);
        MarimayCheckerFixture fixture = {0};
        bool initialized = mmay_checker_fixture_init(&fixture, source);
        MMAY_EXPECT(initialized);
        if (!initialized) continue;
        bool complete = false;
        for (size_t failure = 0; failure < 4096 && !complete; failure += 1) {
            MarimayFailAllocator state;
            MarimayCheckResult checked = {0};
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/void-pattern",
                mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
            complete = status == MMAY_CHECK_OK;
            MMAY_EXPECT(complete || status == MMAY_CHECK_ALLOCATION_FAILED);
            if (complete) {
                const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(&checked, 0);
                MMAY_EXPECT(checked.diagnostics.length == 1);
                MMAY_EXPECT(diagnostic != NULL && strcmp(diagnostic->code, "T0002") == 0);
            }
            mmay_check_result_dispose(&checked);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(complete);
        mmay_checker_fixture_dispose(&fixture);
    }
}

void mmay_test_checker_nested_patterns(void) {
    mmay_void_pattern_diagnostics();
    static const char source[] =
        "type Leaf { value: int, }\n"
        "type Wrap { leaf: Leaf, }\n"
        "enum Inner { Some(int), None, }\n"
        "enum Outer { Box(Inner), Empty, }\n"
        "fn nested(value: Wrap): int {\n"
        "  return match value { Wrap { leaf: Leaf { value } } => value, };\n"
        "}\n";
    static const char enum_source[] =
        "enum Inner { Some(int), None, }\n"
        "enum Outer { Box(Inner), Empty, }\n"
        "fn nested(value: Outer): int {\n"
        "  return match value {\n"
        "    Box(Some(item)) if item > 0 => item,\n"
        "    Box(Some(_)) => 0,\n"
        "    Box(_) => 0,\n"
        "    Empty => 0,\n"
        "  };\n"
        "}\n";
    static const char void_payload_source[] =
        "enum Failure { Bad, }\n"
        "fn classify(value: Result<void, Failure>): int {\n"
        "  return match value { Ok(_) => 1, Err(_) => 2, };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *nested = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/nested-patterns", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        nested = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(nested != NULL && mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_RECORD_GET
        ) == 2);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, enum_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/nested-enum-patterns", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        nested = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(nested != NULL && mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_ENUM_IS_VARIANT
        ) == 5);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, void_payload_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/void-payload-pattern", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        nested = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(nested != NULL && mmay_mir_verify(&nested->mir)
            == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(nested != NULL && mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_ENUM_GET_PAYLOAD
        ) == 1);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}
