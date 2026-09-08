#include "test_checker_imports_support.h"

typedef struct EscapeCase { const char *source; size_t warnings; } EscapeCase;

/* Each one-warning case deliberately has the same exact closure text, making
 * the source byte range an independent, stable assertion. */
static const EscapeCase cases[] = {
    {"fn make():()=>int{let n=1;return ():int=>n;}", 1},
    {"fn main():int{let n=1;const f=():int=>n;const g=f;return g();}", 0},
    {"fn make():()=>int{const n=1;return ():int=>n;}", 0},
    {"fn make():()=>int{let n=1;const a=[():int=>n];return a[0];}", 1},
    {"fn main():int{let n=1;const a=[():int=>n];return a[0]();}", 0},
    {"fn make():Option<()=>int>{let n=1;const a=[():int=>n];return a.pop();}", 1},
    {"fn make():Option<()=>int>{let n=1;const a=[():int=>n];return a.removeAt(0);}", 1},
    {"fn make():[]()=>int{let n=1;const a=[():int=>n];return a.copy();}", 1},
    {"fn zero():int{return 0;}fn main():int{let f:()=>int=zero;if true{let n=1;f=():int=>n;}return f();}", 1},
    {"fn zero():int{return 0;}fn main():int{const a:[]()=>int=[zero];if true{let n=1;a[0]=():int=>n;}return a[0]();}", 1},
    {"fn main():int{let n=1;if true{let n=2;const f=():int=>n;const _=f();}return n;}", 0},
    {"fn use(f:()=>int):int{return f();}fn main():int{let n=1;return use(():int=>n);}", 1},
    {"fn use(f:()=>int):void{const _=f();}fn main():void{let n=1;defer use(():int=>n);}", 1},
    {"type Box{value:()=>int,other:()=>int,}fn z():int{return 0;}fn make():()=>int{let n=1;const b=Box{value:():int=>n,other:z};return b.other;}", 0},
    {"type Box{value:()=>int,}fn make():Box{let n=1;return Box{value:():int=>n};}", 1},
    {"type Box{mutable value:()=>int,}fn z():int{return 0;}fn main():int{const b=Box{value:z};if true{let n=1;b.value=():int=>n;}const f=b.value;return f();}", 1},
    {"fn make():Option<()=>int>{let n=1;return Some(():int=>n);}", 1},
    {"fn main():int{let n=1;const r:Option<()=>int> =Some(():int=>n);return match r{Some(f)=>f(),None=>0,};}", 0},
    {"fn make():()=>int{let n=1;const f=():int=>n;const g=():()=>int=>f;return g();}", 1},
    {"fn make<T>(v:T):()=>int{let n=1;return ():int=>n;}fn main():int{return make(1)()+make(true)();}", 1},
    {"fn make():()=>int{let n:int;n=1;return ():int=>n;}", 1},
    {"fn main():[]()=>int{const a:[]()=>int=[];for n in [1,2]{const _=a.push(():int=>n);}return a;}", 0},
    {"fn main():int{let n=1;const xs=[1,2];const _=xs.findIndex((x:int):bool=>x==n);return n;}", 0},
    {"fn main():int{let n=1;const r:Result<int,int> =Err(2);const _=r.mapError((x:int):int=>x+n);return n;}", 0},
    {"fn make():Map<string,()=>int>{let n=1;const m:Map<string,()=>int> =mapOf<string,()=>int>();const _=m.put(\"k\",():int=>n);return m;}", 1},
    {"fn make():Option<()=>int>{let n=1;const m:Map<string,()=>int> =mapOf<string,()=>int>();const _=m.put(\"k\",():int=>n);return m.get(\"k\");}", 1},
    {"type Node{mutable next:Option<Node>,callback:()=>int,}fn make():Node{let n=1;const a=Node{next:None,callback:():int=>n};a.next=Some(a);return a;}", 1},
    {"fn make():()=>int{let n=1;const m=mapOf<string,()=>int>();const _=m.put(\"k\",():int=>n);for entry in m{return match entry{(_,f)=>f,};}return ():int=>0;}", 1},
};

static void check_case(EscapeCase c) {
    MarimayCheckerFixture fixture = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, c.source));
    bool completed = false;
    for (size_t failure = 0; failure < 32768 && !completed; failure += 1) {
        MarimayCheckResult checked = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/escape",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        completed = status == MMAY_CHECK_OK && failure >= state.attempts;
        if (status == MMAY_CHECK_OK) {
            if (checked.error_count != 0 || checked.diagnostics.length != c.warnings)
                fprintf(stderr, "escape case expected %zu warnings: %s\n", c.warnings, c.source);
            MMAY_EXPECT(checked.error_count == 0 && checked.diagnostics.length == c.warnings);
            MMAY_EXPECT(checked.functions.length != 0);
            for (size_t i = 0; i < checked.diagnostics.length; i += 1) {
                const MarimayCheckDiagnostic *d = mmay_check_diagnostic(&checked, i);
                MMAY_EXPECT(d->severity == MMAY_SEVERITY_WARNING && strcmp(d->code, "T0020") == 0);
                const char *closure = strstr(c.source, "():int=>n");
                MMAY_EXPECT(closure != NULL);
                if (closure != NULL) {
                    MMAY_EXPECT(d->start == (size_t)(closure - c.source));
                    MMAY_EXPECT(d->end == d->start + strlen("():int=>n"));
                }
            }
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            if (status != MMAY_CHECK_ALLOCATION_FAILED) {
                fprintf(stderr, "escape case status %d: %s\n", (int)status, c.source);
                mmay_check_result_dispose(&checked);
                MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
                break;
            }
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

static void graph_warnings(void) {
    const MarimayTestImportSource sources[] = {
        {"test/lib", "lib/first.my", "public fn make():()=>int{let n=1;return ():int=>n;}"},
        {"test/lib", "lib/second.my", "public fn later():int{return 2;}"},
        {"test/app", "app/main.my", "import * as lib from \"test/lib\";fn main():int{return lib.make()()+lib.later();}"}
    };
    MarimayCheckerImportFixture fixture = {0};
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, sources, 3));
    bool completed = false;
    for (size_t failure = 0; failure < 32768 && !completed; failure += 1) {
        MarimayCheckResult checked = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_checker_import_fixture_check(&fixture, "test/app",
            mmay_fail_allocator(&state), mmay_check_graph_default_options(), &checked);
        completed = status == MMAY_CHECK_OK && failure >= state.attempts;
        if (status == MMAY_CHECK_OK) {
            MMAY_EXPECT(checked.error_count == 0 && checked.diagnostics.length == 1);
            MMAY_EXPECT(checked.functions.length == 4);
            if (checked.diagnostics.length == 1)
                MMAY_EXPECT(mmay_check_diagnostic(&checked, 0)->snapshot == fixture.files[0].ast.snapshot);
        } else MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void warning_limits(void) {
    MarimayCheckerFixture fixture = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, cases[0].source));
    for (size_t work = 1; work <= 512; work += 1) {
        MarimayCheckResult checked = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayCheckOptions options = mmay_check_default_options();
        options.mir_options.max_verify_work = work;
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/escape-limits",
            mmay_fail_allocator(&state), options, &checked);
        MMAY_EXPECT(status == MMAY_CHECK_OK || status == MMAY_CHECK_LIMIT_REACHED);
        if (status == MMAY_CHECK_OK) MMAY_EXPECT(checked.diagnostics.length == 1 && checked.error_count == 0);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_escape(void) {
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i += 1) check_case(cases[i]);
    graph_warnings();
    warning_limits();
}
