#include "test_checker_imports_support.h"

/* Includes escaping and transitive cells, shadowing, generic instantiations,
 * stored functions and two closures that observe the same mutable binding. */
static const char callable_source[] =
    "fn add(x:int):int{return x+1;}\n"
    "fn make<T>(x:T):()=>T{return ():T=>x;}\n"
    "fn counters():[]()=>int{let n=1; const inc=():int=>{n+=1;return n;};\n"
    " const read=():int=>n; return [inc,read];}\n"
    "fn main():int{const f:(int)=>int=add;const g=make(3);const a=counters();\n"
    " const factory=():()=>int=>{let n=4;return ():int=>{n+=1;return n;};};\n"
    " const nested=factory();let later:int;if true{later=2;}else{later=3;}\n"
    " const use=(b:bool):int=>{if b{return later;}return later+1;};\n"
    " return f(g())+a[0]()+a[1]()+nested()+use(true);}\n";

static void check_failures(const char *source, bool valid) {
    MarimayCheckerFixture fixture = {0};
    bool completed = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (size_t failure = 0; failure < 32768 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/callable",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK || (!valid && status == MMAY_CHECK_INCOMPLETE_LANGUAGE)) {
            completed = true;
            if (!valid && checked.diagnostics.length == 0) fprintf(stderr, "accepted invalid callable: %s\n",source);
            MMAY_EXPECT(valid ? checked.diagnostics.length == 0 : checked.diagnostics.length > 0);
            if (valid && checked.diagnostics.length != 0) {
                const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(&checked, 0);
                fprintf(stderr, "callable diagnostic: %s\n", diagnostic->message);
            }
            state.failure_index = SIZE_MAX;
            for (size_t i = 0; valid && i < checked.functions.length; i += 1)
                MMAY_EXPECT(mmay_mir_verify(&mmay_checked_function(&checked, i)->mir) == MMAY_MIR_VERIFY_OK);
        } else {
            if (status != MMAY_CHECK_ALLOCATION_FAILED)
                fprintf(stderr, "callable failure index %zu status %d\n", failure, (int)status);
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

static void resource_limits(void) {
    static const char limited[] = "fn main():int{let n=1;const f=():int=>{n+=1;return n;};return f();}";
    MarimayCheckerFixture fixture = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, limited));
    for (unsigned gate = 0; gate < 7; gate += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        MarimayCheckOptions options = mmay_check_default_options();
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        switch (gate) {
            case 0: options.max_functions = 1; break;
            case 1: options.mir_options.max_record_constructions = 1; break;
            case 2: options.mir_options.max_record_layouts = 1; break;
            case 3: options.mir_options.max_record_fields = 1; break;
            case 4: options.mir_options.max_registers = 2; break;
            case 5: options.mir_options.max_verify_work = 1; break;
            case 6: options.max_locals_per_function = 1; break;
        }
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/callable-limits",
            mmay_fail_allocator(&state), options, &checked);
        if (status != MMAY_CHECK_LIMIT_REACHED) fprintf(stderr,"callable limit %u status %d\n",gate,(int)status);
        MMAY_EXPECT(status == MMAY_CHECK_LIMIT_REACHED);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    bool completed = false;
    for (size_t extra = 1; extra < 32 && !completed; extra += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        MarimayCheckOptions options = mmay_check_default_options();
        options.type_options.max_types = MMAY_TYPE_FIRST_DYNAMIC + extra;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/callable-type-limits",
            mmay_fail_allocator(&state), options, &checked);
        completed = status == MMAY_CHECK_OK;
        MMAY_EXPECT(completed || status == MMAY_CHECK_LIMIT_REACHED);
        if (completed) MMAY_EXPECT(checked.diagnostics.length == 0);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

static void imported_unsafe_reference(void) {
    static const MarimayTestImportSource inputs[] = {
        {"test/lib", "lib/lib.my", "public unsafe fn raw(x:int):int{return x;}"},
        {"test/app", "app/main.my", "import * as lib from \"test/lib\";fn main():void{const f=lib.raw;}"}
    };
    MarimayCheckerImportFixture fixture = {0};
    bool completed = false;
    bool initialized = mmay_checker_import_fixture_init(&fixture, inputs, 2);
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    for (size_t failure = 0; failure < 8192 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_checker_import_fixture_check(&fixture, "test/app",
            mmay_fail_allocator(&state), mmay_check_graph_default_options(), &checked);
        completed = status == MMAY_CHECK_INCOMPLETE_LANGUAGE;
        MMAY_EXPECT(completed || status == MMAY_CHECK_ALLOCATION_FAILED);
        if (completed) MMAY_EXPECT(checked.diagnostics.length > 0);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_checker_function_values(void) {
    resource_limits();
    imported_unsafe_reference();
    check_failures("unsafe fn raw(x:int):int{return x;}fn main():void{const f=raw;}", false);
    check_failures(callable_source, true);
    check_failures("fn main():void{const f=(x:void):int=>1;}", false);
    check_failures("fn main():void{const f=(x:never):int=>1;}", false);
    check_failures("fn main():void{const f=(x:int):int=>x;const _=f<int>(1);}", false);
    check_failures("fn main():void{const f=(x):int=>x;}", false);
    check_failures("fn main():void{const f=(x:int)=>x;}", false);
    check_failures("fn main():void{const f=(x:int):bool=>x;}", false);
    check_failures("fn main():void{const f=(x:int):int=>x;const _=f();}", false);
    check_failures("fn main():void{const f=(x:int):int=>x;const _=f(true);}", false);
    check_failures("fn main():void{const f:(bool)=>int=(x:int):int=>x;}", false);
    check_failures("fn main():void{let x:int;const f=():int=>x;x=1;}", false);
    check_failures("fn main():void{const x=1;const f=():void=>{x=2;};}", false);
    check_failures("fn main():void{const f=(x:int,x:int):int=>x;}", false);
    check_failures("fn main():void{const f=():int=>{if true{return 1;}};}", false);
    check_failures("fn main():void{for x in 0..1{const f=():void=>{break;};}}", false);
    check_failures("fn id<T>(x:T):T{return x;}fn main():void{const f=id;}", false);
    check_failures("fn add(x:int):int{return x;}fn main():void{const f:(bool)=>int=add;}", false);
    check_failures("fn halt():never{panic(\"stop\");}fn use():int{const f=halt;f();}", true);
    check_failures("fn main():void{const f=():void=>{};f();}", true);
}
