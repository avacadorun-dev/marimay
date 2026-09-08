#include "test_checker_support.h"

static void mapper_check(const char *source, bool valid) {
    MarimayCheckerFixture fixture = {0};
    bool complete = false;
    bool initialized = mmay_checker_fixture_init(&fixture, source);
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    for (size_t failure = 0; failure < 16384 && !complete; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/mapper-values",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        complete = status == MMAY_CHECK_OK || (!valid && status == MMAY_CHECK_INCOMPLETE_LANGUAGE);
        if (!complete && status != MMAY_CHECK_ALLOCATION_FAILED) {
            fprintf(stderr, "mapper allocation index %zu status %d: %s\n", failure, (int)status, source);
            MMAY_EXPECT(false);
            mmay_check_result_dispose(&checked);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
            break;
        }
        MMAY_EXPECT(complete || status == MMAY_CHECK_ALLOCATION_FAILED);
        if (complete) {
            MMAY_EXPECT(valid ? checked.error_count == 0 : checked.error_count > 0);
            state.failure_index = SIZE_MAX;
            for (size_t i = 0; valid && i < checked.functions.length; i += 1)
                MMAY_EXPECT(mmay_mir_verify(&mmay_checked_function(&checked, i)->mir) == MMAY_MIR_VERIFY_OK);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(complete);
    mmay_checker_fixture_dispose(&fixture);
}

static void mapper_limits(void) {
    MarimayCheckerFixture fixture = {0};
    const char *source = "fn touch():int{return 1;}fn main():void{const _=touch();"
        "const r:Result<int,int> = Err(2);const f=(e:int):int=>e;const _=r.mapError(f);}";
    bool initialized = mmay_checker_fixture_init(&fixture, source), completed = false;
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    for (size_t limit = 0; limit < 68 && !completed; limit += 1) {
        MarimayCheckOptions options = mmay_check_default_options();
        if (limit == 0) options.mir_options.max_calls = 1;
        else if (limit == 1) options.mir_options.max_call_arguments = 1;
        else if (limit == 2) options.mir_options.max_blocks = 3;
        else options.type_options.max_types = MMAY_TYPE_FIRST_DYNAMIC + limit - 2;
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/mapper-limits",
            mmay_fail_allocator(&state), options, &checked);
        completed = limit > 2 && status == MMAY_CHECK_OK;
        if (!completed && status != MMAY_CHECK_LIMIT_REACHED)
            fprintf(stderr, "mapper limit %zu status %d\n", limit, (int)status);
        MMAY_EXPECT(completed || status == MMAY_CHECK_LIMIT_REACHED);
        if (completed) MMAY_EXPECT(checked.diagnostics.length == 0);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_mapper_values(void) {
    mapper_limits();
    mapper_check("fn main():void{const r:Result<int,int> = Ok(3);const f=(x:int):never=>panic(\"stop\");const _=r.mapError(f);}", true);
    mapper_check("fn map<T,E,F>(r:Result<T,E>,f:(E)=>F):Result<T,F>{return r.mapError(f);}"
        "fn main():void{let n=0;const r:Result<int,int> = Err(3);"
        "const f=(x:int):int=>{n+=1;return x+n;};const _=map(r,f);"
        "const a=[f];const _=r.mapError(a[0]);const v:Result<int,void> = r.mapError((x:int):void=>{});"
        "const c:Result<int,int> = r.mapError((x)=>x+n);}", true);
    mapper_check("fn main():never{const r:Result<int,int> = Ok(3);const _=r.mapError(panic(\"factory\"));}", true);
    static const char *invalid[] = {
        "fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError();}",
        "fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError((x:int):int=>x,1);}",
        "fn main():void{const r:Result<int,int> = Ok(3);const f=(x:bool):int=>1;const _=r.mapError(f);}",
        "fn main():void{const r:Result<int,int> = Ok(3);const f=(x:int,y:int):int=>x;const _=r.mapError(f);}",
        "fn main():void{const r:Result<int,int> = Ok(3);const f=():int=>1;const _=r.mapError(f);}",
        "fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError(1);}",
        "fn main():void{const r:Result<int,int> = Ok(3);let f:(int)=>int;const _=r.mapError(f);}",
        "fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError((x)=>{return x;});}",
        "fn main():void{const r:Result<int,int> = Ok(3);const f=(x:int):int=>x;const y:Result<int,bool> = r.mapError(f);}",
        "fn main():void{let n:int;const r:Result<int,int> = Ok(3);const y:Result<int,int> = r.mapError((x)=>x+n);}",
        "unsafe fn mapper(x:int):int{return x;}fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError(mapper);}",
        "extern \"c\" fn mapper(x:int):int;fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError(mapper);}",
        "fn mapper<T>(x:T):T{return x;}fn main():void{const r:Result<int,int> = Ok(3);const _=r.mapError(mapper);}"
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i += 1) mapper_check(invalid[i], false);
}
