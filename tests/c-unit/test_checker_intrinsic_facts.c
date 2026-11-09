#include "test_checker_imports_support.h"

#include <string.h>

/* Stream calls lower directly to IO instructions, but retain the declaration
 * identity selected by the ordinary imported-call resolver. */
static const char stream_source[] =
    "public enum Error {\n"
    " CapabilityDenied { capability: string, }, ReadFailed, WriteFailed,\n"
    " FlushFailed, InvalidUtf8, LineTooLong { limit: int, }, InputTooLong { limit: int, },\n"
    "}\n"
    "public extern \"c\" fn print(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprint(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprintln(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn readLine(limit: int): Result<Option<string>, Error>;\n"
    "public extern \"c\" fn flushStdout(): Result<void, Error>;\n"
    "public extern \"c\" fn flushStderr(): Result<void, Error>;\n"
    "extern \"c\" fn mmayReadBytes(limit: int): Result<[]byte, Error>;\n"
    "extern \"c\" fn mmayWriteBytes(values: []byte): Result<void, Error>;\n"
    "extern \"c\" fn mmayEwriteBytes(values: []byte): Result<void, Error>;\n"
    "public fn readBinary(): Result<[]byte, Error> { return mmayReadBytes(4); }\n"
    "public fn writeBinary(values: []byte): Result<void, Error> { return mmayWriteBytes(values); }\n"
    "public fn ewriteBinary(values: []byte): Result<void, Error> { return mmayEwriteBytes(values); }\n";

static const char application_source[] =
    "import * as io from \"std/io\";\n"
    "import { println as say } from \"std/io\";\n"
    "import { println as other } from \"example/other\";\n"
    "fn calls(): void {\n"
    " const _ = io.print(\"x\"); const _ = io.println(\"x\");\n"
    " const _ = io.eprint(\"x\"); const _ = io.eprintln(\"x\");\n"
    " const _ = io.readLine(1); const _ = io.flushStdout(); const _ = io.flushStderr();\n"
    " const _ = say(\"x\"); const _ = other(1);\n"
    "}\n"
    "fn shadow(say: (int)=>int): int { return say(1); }\n";

static const MarimayTestImportSource sources[] = {
    {"std/io", "std/io/io.my", stream_source},
    {"example/other", "other/other.my", "public fn println(value: int): int { return value; }\n"},
    {"example/app", "app/main.my", application_source},
};

static const MarimayCheckedSymbol *definition(const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot, const char *source, const char *needle) {
    const char *match = strstr(source, needle);
    MMAY_EXPECT(match != NULL);
    return match == NULL ? NULL : mmay_checked_definition_from_reference(result,
        snapshot, (size_t)(match - source));
}

static void expect_target(const MarimayCheckedSymbol *symbol, const char *path,
    const char *name) {
    MMAY_EXPECT(symbol != NULL);
    if (symbol == NULL) return;
    MMAY_EXPECT(symbol->kind == MMAY_CHECKED_SYMBOL_FUNCTION);
    MMAY_EXPECT(strcmp(mmay_source_path(symbol->snapshot), path) == 0);
    MMAY_EXPECT(symbol->name.length == strlen(name));
    MMAY_EXPECT(memcmp(symbol->name.data, name, strlen(name)) == 0);
    const char *source = (const char *)mmay_source_bytes(symbol->snapshot);
    MMAY_EXPECT(symbol->end - symbol->start == strlen(name));
    MMAY_EXPECT(memcmp(source + symbol->start, name, strlen(name)) == 0);
}

static void stream_definitions(void) {
    static const char *const names[] = {
        "print", "println", "eprint", "eprintln", "readLine", "flushStdout", "flushStderr",
    };
    MarimayCheckerImportFixture fixture = {0};
    MarimayCheckResult result = {0};
    bool initialized = mmay_checker_import_fixture_init(&fixture, sources, 3);
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    MMAY_EXPECT(mmay_checker_import_fixture_check(&fixture, "example/app",
        mmay_system_allocator(), mmay_check_graph_default_options(), &result) == MMAY_CHECK_OK);
    MMAY_EXPECT(result.error_count == 0);
    const MarimaySourceSnapshot *application = fixture.files[2].ast.snapshot;
    const MarimaySourceSnapshot *stream = fixture.files[0].ast.snapshot;
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        char needle[32];
        int length = snprintf(needle, sizeof(needle), "%s(", names[index]);
        MMAY_EXPECT(length > 0 && (size_t)length < sizeof(needle));
        expect_target(definition(&result, application, application_source, needle),
            "std/io/io.my", names[index]);
    }
    expect_target(definition(&result, application, application_source, "say(\"x\")"),
        "std/io/io.my", "println");
    expect_target(definition(&result, application, application_source, "other(1)"),
        "other/other.my", "println");
    const MarimayCheckedSymbol *shadow = definition(&result, application,
        application_source, "say(1)");
    MMAY_EXPECT(shadow != NULL && shadow->kind == MMAY_CHECKED_SYMBOL_PARAMETER);
    expect_target(definition(&result, stream, stream_source, "mmayReadBytes(4)"),
        "std/io/io.my", "mmayReadBytes");
    expect_target(definition(&result, stream, stream_source, "mmayWriteBytes(values)"),
        "std/io/io.my", "mmayWriteBytes");
    expect_target(definition(&result, stream, stream_source, "mmayEwriteBytes(values)"),
        "std/io/io.my", "mmayEwriteBytes");
    mmay_check_result_dispose(&result);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void stream_allocation_failures(void) {
    MarimayCheckerImportFixture fixture = {0};
    bool complete = false;
    bool initialized = mmay_checker_import_fixture_init(&fixture, sources, 3);
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    for (size_t failure = 0; failure < 32768 && !complete; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult result = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_checker_import_fixture_check(&fixture,
            "example/app", mmay_fail_allocator(&state), mmay_check_graph_default_options(), &result);
        if (status == MMAY_CHECK_OK) {
            complete = true;
            MMAY_EXPECT(result.error_count == 0);
            expect_target(definition(&result, fixture.files[2].ast.snapshot,
                application_source, "say(\"x\")"), "std/io/io.my", "println");
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        mmay_check_result_dispose(&result);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(complete);
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_checker_intrinsic_facts(void) {
    stream_definitions();
    stream_allocation_failures();
}
