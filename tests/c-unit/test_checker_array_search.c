#include "test_checker_support.h"

static void search_check(const char *source, bool valid) {
    MarimayCheckerFixture fixture = {0};
    bool initialized = mmay_checker_fixture_init(&fixture, source), completed = false;
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    for (size_t failure = 0; failure < 16384 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/array-search",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        completed = status == MMAY_CHECK_OK || (!valid && status == MMAY_CHECK_INCOMPLETE_LANGUAGE);
        MMAY_EXPECT(completed || status == MMAY_CHECK_ALLOCATION_FAILED);
        if (completed) {
            MMAY_EXPECT(valid ? checked.diagnostics.length == 0 : checked.diagnostics.length > 0);
            state.failure_index = SIZE_MAX;
            for (size_t i = 0; valid && i < checked.functions.length; i += 1)
                MMAY_EXPECT(mmay_mir_verify(&mmay_checked_function(&checked, i)->mir) == MMAY_MIR_VERIFY_OK);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

static void search_limits(void) {
    MarimayCheckerFixture fixture = {0};
    const char *source = "fn touch():int{return 1;}fn main():void{const _=touch();const a=[1,2];const _=a.findIndex((x:int):bool=>x==2);}";
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (unsigned limit = 0; limit < 4; limit += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        MarimayCheckOptions options = mmay_check_default_options();
        if (limit == 0) options.mir_options.max_calls = 1;
        if (limit == 1) options.mir_options.max_call_arguments = 1;
        if (limit == 2) options.mir_options.max_blocks = 3;
        if (limit == 3) options.mir_options.max_registers = 4;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/search-limits", mmay_fail_allocator(&state),
            options, &checked);
        if (status != MMAY_CHECK_LIMIT_REACHED) fprintf(stderr, "search limit %u status %d\n", limit, (int)status);
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
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/search-type-limits",
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

void mmay_test_checker_array_search(void) {
    search_limits();
    search_check("fn predicate(x:int):bool{return x==2;}fn search<T>(a:[]T,p:(T)=>bool):Option<int>{return findIndex(a,p);}"
        "fn main():void{const a=[[1],[2]];let n=0;const _=a.findIndex((x:[]int):bool=>{n+=1;return x[0]==n;});"
        "const _=search([1,2],predicate);const empty:[]int=[];const _=empty.findIndex(predicate);}", true);
    search_check("fn main(a:[]int):void{const _=a.findIndex();}", false);
    search_check("fn main(a:[]int):void{const _=findIndex(a);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex((x:int):bool=>true,1);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex<int>((x:int):bool=>true);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex((x:bool):bool=>x);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex((x:int):int=>x);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex((x):bool=>true);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex((x:int)=>true);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex((x:int,y:int):bool=>true);}", false);
    search_check("fn main(a:[]int):void{const _=a.findIndex(3);}", false);
    search_check("fn main(a:[]int):int{return a.findIndex((x:int):bool=>true);}", false);
    search_check("fn main(a:[]int):void{const findIndex=3;const _=findIndex(a,(x:int):bool=>true);}", false);
    search_check("fn main(a:[]int):never{const _=a.findIndex(panic(\"stop\"));}", true);
}
