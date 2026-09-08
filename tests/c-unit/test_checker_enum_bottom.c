#include "test_checker_support.h"
#include "test_callable_engine_support.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/codec.h"

static const char bottom_source[] =
    "enum Choice<T>{Empty,Full(T),} enum Named<T>{Good(int),Bad{value:T},}"
    "enum Wrap<T>{Value(T),}"
    "fn remap(r:Result<int,never>):Result<int,Wrap<never>>{return r.mapError(Wrap.Value);}"
    "fn halt(x:int):never{panic(\"stop\");}"
    "fn mapped(x:Result<int,int>):Result<int,never>{return x.mapError(halt);}"
    "fn propagated():Result<int,never>{const x=mapped(Ok(7))?;return Ok(x+1);}"
    "fn failOnly(x:Result<never,int>):Result<never,string>{return x.mapError((v:int):string=>\"bad\");}"
    "fn diverges(x:Result<never,int>):never{const _=x.mapError(halt);}"
    "fn empty(x:Result<never,never>):never{match x {}; }"
    "fn main():int{const n:Option<never> = None;match n{None=>{},};"
    "const rr=remap(Ok(6));const _=match rr{Ok(x)=>x,Err(_)=>0,};"
    "const unit:Result<void,int> = Ok();const _=unit.mapError(halt);"
    "const c:Choice<never> = Choice.Empty;match c{Choice.Empty=>{},};"
    "const a:Named<never> = Named.Good(1);const j=match a{Named.Good(v)=>v,};"
    "const es:Result<never,string> = failOnly(Err(2));match es{Err(_)=>{},};"
    "const xs:[]Result<int,never> = [propagated()];return match xs[0]{Ok(v)=>v+j,};}";

static void bottom_allocations(const char *source, const char *code) {
    MarimayCheckerFixture fixture = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    bool completed = false;
    for (size_t failure = 0; failure < 16384 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/enum-bottom",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        completed = status == MMAY_CHECK_OK && failure >= state.attempts;
        if (status == MMAY_CHECK_OK) {
            MMAY_EXPECT(checked.diagnostics.length == (code == NULL ? 0U : 1U));
            if (code != NULL && checked.diagnostics.length == 1)
                MMAY_EXPECT(strcmp(mmay_check_diagnostic(&checked, 0)->code, code) == 0);
        } else MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

/* Hostile metadata cannot turn the bottom payload into a stored value. */
static void bottom_integrity(void) {
    MarimayCheckerFixture fixture = {0};
    MarimayCheckResult checked = {0};
    MarimayFailAllocator state;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture,
        "fn main():int{const r:Result<int,never> = Ok(3);return match r{Ok(x)=>x,};}"));
    MMAY_EXPECT(mmay_check_ast(&fixture.ast, "test/enum-bottom", allocator,
        mmay_check_default_options(), &checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length == 0 && checked.functions.length == 1);
    if (checked.functions.length != 1) goto cleanup;
    MarimayCheckedFunction *entry = mmay_vector_at(&checked.functions, 0);
    MarimayMirFunction *mir = &entry->mir;
    MarimayTypeId *type = mmay_vector_at(&mir->register_types, 0), saved = *type;
    *type = MMAY_TYPE_NEVER;
    MMAY_EXPECT(mmay_mir_verify(mir) == MMAY_MIR_VERIFY_INVALID_REGISTER);
    *type = saved;
    MarimayMirEnumConstruction *construction = mmay_vector_at(&mir->enum_constructions, 0);
    size_t variant = construction->variant;
    construction->variant = 1;
    MMAY_EXPECT(mmay_mir_verify(mir) != MMAY_MIR_VERIFY_OK);
    construction->variant = variant;
    /* Parameter type metadata alone cannot manufacture the missing payload. */
    MarimayMirEnumArgument *argument = mmay_vector_at(&mir->enum_arguments, construction->argument_start);
    MarimayTypeId argument_type = argument->type;
    size_t parameter_count = mir->parameter_count;
    mir->parameter_count = 1;
    *type = MMAY_TYPE_NEVER;
    argument->type = MMAY_TYPE_NEVER;
    construction->variant = 1;
    MMAY_EXPECT(mmay_mir_verify(mir) != MMAY_MIR_VERIFY_OK);
    mir->parameter_count = parameter_count;
    *type = saved;
    argument->type = argument_type;
    construction->variant = variant;
    MarimayBytecodeFunction body = {0};
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(mir, allocator,
        mmay_mir_bytecode_lower_default_options(), &body) == MMAY_MIR_BYTECODE_LOWER_OK);
    MarimayBytecodeEnumPayload *payload = mmay_vector_at(&body.enum_payloads, 1);
    MMAY_EXPECT(payload != NULL && payload->type == MMAY_BYTECODE_TYPE_NEVER);
    if (payload != NULL) {
        uint32_t layout = payload->layout;
        payload->layout = 0;
        MMAY_EXPECT(mmay_bytecode_verify(&body) != MMAY_BYTECODE_VERIFY_OK);
        payload->layout = layout;
        payload->type = MMAY_BYTECODE_TYPE_VOID;
        MMAY_EXPECT(mmay_bytecode_verify(&body) != MMAY_BYTECODE_VERIFY_OK);
        payload->type = MMAY_BYTECODE_TYPE_NEVER;
    }
    MarimayBytecodeType *register_type = mmay_vector_at(&body.register_types, 0);
    MarimayBytecodeType saved_type = *register_type;
    *register_type = MMAY_BYTECODE_TYPE_NEVER;
    MMAY_EXPECT(mmay_bytecode_verify(&body) != MMAY_BYTECODE_VERIFY_OK);
    *register_type = saved_type;
    MarimayBytecodeEnumConstruction *wire_construction = mmay_vector_at(&body.enum_constructions, 0);
    wire_construction->variant = 1;
    MMAY_EXPECT(mmay_bytecode_verify(&body) != MMAY_BYTECODE_VERIFY_OK);
    wire_construction->variant = 0;
    MMAY_EXPECT(mmay_bytecode_verify(&body) == MMAY_BYTECODE_VERIFY_OK);
    mmay_bytecode_function_dispose(&body);
cleanup:
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_checker_enum_bottom(void) {
    bottom_allocations(bottom_source, NULL);
    static const char *negative[] = {
        "fn main():void{const r:Result<int,never> = Err(2);}",
        "fn main():void{const r:Result<int,never> = Err();}",
        "fn bad(r:Result<int,never>):int{return match r{Ok(x)=>x,Err(e)=>1,};}",
        "fn bad(r:Option<never>):void{match r{Some(_)=>{},None=>{},};}",
        "enum N<T>{Good,Bad{v:T},}fn bad(r:N<never>):void{match r{N.Good=>{},N.Bad{v:_}=>{},};}",
        "fn bad(r:Result<int,never>):Result<int,string>{const x=r?;return Ok(x);}",
        "fn bad(r:Result<int,never>):int{return match r{};}"
    };
    const char *codes[] = {MMAY_DIAGNOSTIC_TYPE_MISMATCH, MMAY_DIAGNOSTIC_CALL_ARITY,
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN, MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH};
    for (size_t i = 0; i < sizeof(negative) / sizeof(negative[0]); i += 1)
        bottom_allocations(negative[i], codes[i]);
    bottom_integrity();
}

void mmay_test_enum_bottom_engines(void) {
    mmay_test_callable_engines(bottom_source, 9, 3, 3);
}
