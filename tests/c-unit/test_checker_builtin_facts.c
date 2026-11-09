#include "test_checker_imports_support.h"

#include <stdio.h>
#include <string.h>

/* These cases use the real declaration-resolving primitive paths. Prelude
 * operations without a source declaration deliberately have no guessed target. */
#define CONVERT_CASES(X) \
    X(parseInt, "text: string", "Result<int, ParseError>", "\"1\"") \
    X(parseIntRadix, "text: string, radix: int", "Result<int, ParseError>", "\"1\", 10") \
    X(formatF32, "value: f32", "string", "value") \
    X(formatF64, "value: f64", "string", "1.5") \
    X(formatBool, "value: bool", "string", "true") \
    X(formatI8, "value: i8", "string", "1") \
    X(formatI16, "value: i16", "string", "1") \
    X(formatI32, "value: i32", "string", "1") \
    X(formatI64, "value: i64", "string", "1") \
    X(formatU8, "value: u8", "string", "1") \
    X(formatU16, "value: u16", "string", "1") \
    X(formatU32, "value: u32", "string", "1") \
    X(formatU64, "value: u64", "string", "1")

#define TEXT_CASES(X) \
    X(mmayByteLength, "text: string", "int", "text") \
    X(mmayRuneCount, "text: string", "int", "text") \
    X(mmayGraphemeCount, "text: string", "int", "text") \
    X(mmayIsRuneBoundary, "text: string, offset: int", "bool", "text, offset") \
    X(mmaySubstring, "text: string, start: int, end: int", "string", "text, start, end") \
    X(mmayDropPrefix, "text: string, offset: int", "string", "text, offset") \
    X(mmayTakePrefix, "text: string, offset: int", "string", "text, offset") \
    X(mmayContains, "text: string, needle: string", "bool", "text, needle") \
    X(mmayStartsWith, "text: string, prefix: string", "bool", "text, prefix") \
    X(mmayEndsWith, "text: string, suffix: string", "bool", "text, suffix") \
    X(mmayTrimAscii, "text: string", "string", "text") \
    X(mmaySplitCount, "text: string, separator: string", "int", "text, separator") \
    X(mmaySplit, "text: string, separator: string", "[]string", "text, separator") \
    X(mmayJoin, "parts: []string, separator: string", "string", "parts, separator") \
    X(mmayRunes, "text: string", "[]rune", "text") \
    X(mmayGraphemes, "text: string", "[]string", "text")

#define BYTES_CASES(X) \
    X(mmayEncodeUtf8, "text: string", "[]byte", "\"x\"") \
    X(mmayInvalidUtf8Offset, "values: []byte", "int", "[1]") \
    X(mmayDecodeUtf8, "values: []byte", "string", "[1]") \
    X(mmayGeneration, "values: []byte", "u64", "[1]") \
    X(mmayRequireGeneration, "values: []byte, generation: u64", "void", "[1], 0")

#define PATH_CASES(X) \
    X(mmayPathNormalize, "text: string", "Result<string, Error>", "\"x\"") \
    X(mmayPathIsAbsolute, "text: string", "bool", "\"x\"")

#define FS_CASES(X) \
    X(mmayFsOpen, "text: string", "Result<int, Error>", "\"x\"") \
    X(mmayFsRead, "handle: int, limit: int", "Result<[]byte, Error>", "1, 1") \
    X(mmayFsReadLine, "handle: int, limit: int", "Result<Option<string>, Error>", "1, 1") \
    X(mmayFsClose, "handle: int", "Result<void, Error>", "1") \
    X(mmayFsCloseBestEffort, "handle: int", "void", "1") \
    X(mmayFsReadAll, "text: string, limit: int", "Result<[]byte, Error>", "\"x\", 1") \
    X(mmayFsWriteAtomic, "text: string, values: []byte", "Result<void, Error>", "\"x\", [1]") \
    X(mmayFsMetadata, "text: string", "Result<Metadata, Error>", "\"x\"") \
    X(mmayFsReadDirectory, "text: string, limit: int", "Result<[]Entry, Error>", "\"x\", 1")

#define ENV_CASES(X) X(mmayEnvGet, "name: string", "Result<Option<string>, Error>", "\"X\"")
#define PROCESS_CASES(X) \
    X(mmayProcessCurrentDirectory, "", "Result<string, Error>", "") \
    X(mmayProcessExecutablePath, "", "Result<string, Error>", "")

#define DECLARE(name, parameters, result, arguments) \
    "public extern \"c\" fn " #name "(" parameters "): " result ";\n"
#define LOCAL_CALL(name, parameters, result, arguments) \
    "fn probe_" #name "(" parameters "): void { const _ = " #name "(" arguments "); }\n"
#define IMPORTED_CALL(name, parameters, result, arguments) \
    "fn probe_" #name "(" parameters "): " result " { return convert." #name "(" arguments "); }\n"
#define NAME(name, parameters, result, arguments) #name,

static const char convert_source[] =
    "public enum ParseError { Empty, InvalidDigit { offset: int, }, OutOfRange, InvalidRadix, }\n"
    CONVERT_CASES(DECLARE);
static const char convert_application[] =
    "import * as convert from \"std/convert\";\n"
    "import { ParseError } from \"std/convert\";\n"
    "import { parseInt as read, formatI64 as format, formatF32 as float } from \"std/convert\";\n"
    CONVERT_CASES(IMPORTED_CALL)
    "fn aliasParse(): Result<int, convert.ParseError> { return read(\"2\"); }\n"
    "fn aliasInteger(): string { return format(1); }\n"
    "fn aliasFloat(value: f32): string { return float(value); }\n"
    "fn shadow(read: (int)=>int): int { return read(1); }\n";
static const char text_source[] = "type TextOwner = string;\n"
    TEXT_CASES(DECLARE) TEXT_CASES(LOCAL_CALL);
static const char bytes_source[] = BYTES_CASES(DECLARE) BYTES_CASES(LOCAL_CALL);
static const char path_source[] =
    "public enum Error { InvalidCharacter, UnsupportedPrefix, EscapesRoot, TooLong { limit: int, }, }\n"
    PATH_CASES(DECLARE) PATH_CASES(LOCAL_CALL);
static const char fs_source[] =
    "public enum Error { CapabilityDenied { capability: string, path: string, },\n"
    " InvalidLimit { limit: int, }, LimitExceeded { limit: int, }, NotFound, NotFile,\n"
    " NotDirectory, Symlink, Closed, OpenFailed, ReadFailed, WriteFailed, SyncFailed,\n"
    " CloseFailed, ReplaceFailed, InvalidUtf8 { offset: int, },\n"
    " TooManyEntries { limit: int, }, DirectoryFailed, }\n"
    "public enum Kind { File, Directory, }\n"
    "public enum EntryKind { File, Directory, Symlink, Other, }\n"
    "public type Metadata { public kind: Kind, public size: int, }\n"
    "public type Entry { public name: string, public kind: EntryKind, }\n"
    FS_CASES(DECLARE) FS_CASES(LOCAL_CALL);
static const char env_source[] =
    "public enum Error { InvalidName, CapabilityDenied { name: string, },\n"
    " InvalidUtf8 { offset: int, }, LimitExceeded { limit: int, }, ReadFailed, }\n"
    ENV_CASES(DECLARE) ENV_CASES(LOCAL_CALL);
static const char process_source[] =
    "public enum Error { CurrentDirectoryUnavailable, ExecutablePathUnavailable,\n"
    " InvalidUtf8 { offset: int, }, InvalidPath, }\n"
    PROCESS_CASES(DECLARE) PROCESS_CASES(LOCAL_CALL);

static const char *const convert_names[] = {CONVERT_CASES(NAME)};
static const char *const text_names[] = {TEXT_CASES(NAME)};
static const char *const bytes_names[] = {BYTES_CASES(NAME)};
static const char *const path_names[] = {PATH_CASES(NAME)};
static const char *const fs_names[] = {FS_CASES(NAME)};
static const char *const env_names[] = {ENV_CASES(NAME)};
static const char *const process_names[] = {PROCESS_CASES(NAME)};

typedef struct BuiltinFamily {
    const char *package;
    const char *path;
    const char *source;
    const char *const *names;
    size_t count;
} BuiltinFamily;

#define FAMILY(name, package) {package, package "/" #name ".my", name##_source, \
    name##_names, sizeof(name##_names) / sizeof(name##_names[0])}
static const BuiltinFamily families[] = {
    FAMILY(convert, "std/convert"), FAMILY(text, "std/text"), FAMILY(bytes, "std/bytes"),
    FAMILY(path, "std/path"), FAMILY(fs, "std/fs"), FAMILY(env, "std/env"),
    FAMILY(process, "std/process"),
};

static const MarimayCheckedSymbol *builtin_definition(const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot, const char *source, const char *call) {
    const char *match = strstr(source, call);
    MMAY_EXPECT(match != NULL);
    return match == NULL ? NULL : mmay_checked_definition_from_reference(result,
        snapshot, (size_t)(match - source));
}

static void builtin_expect(const MarimayCheckedSymbol *symbol,
    const BuiltinFamily *family, const char *name) {
    MMAY_EXPECT(symbol != NULL);
    if (symbol == NULL) return;
    MMAY_EXPECT(symbol->kind == MMAY_CHECKED_SYMBOL_FUNCTION);
    MMAY_EXPECT(strcmp(mmay_source_path(symbol->snapshot), family->path) == 0);
    MMAY_EXPECT(symbol->name.length == strlen(name)
        && memcmp(symbol->name.data, name, strlen(name)) == 0);
}

static bool builtin_fixture(MarimayCheckerImportFixture *fixture,
    const BuiltinFamily *family, bool imported) {
    MarimayTestImportSource sources[] = {
        {family->package, family->path, family->source},
        {"example/app", "app/main.my", convert_application},
    };
    return mmay_checker_import_fixture_init(fixture, sources, imported ? 2 : 1);
}

static bool builtin_definitions(const BuiltinFamily *family, bool imported) {
    MarimayCheckerImportFixture fixture = {0};
    MarimayCheckResult result = {0};
    bool initialized = builtin_fixture(&fixture, family, imported);
    MMAY_EXPECT(initialized);
    if (!initialized) return false;
    MarimayCheckStatus status = mmay_checker_import_fixture_check(&fixture,
        imported ? "example/app" : family->package, mmay_system_allocator(),
        mmay_check_graph_default_options(), &result);
    MMAY_EXPECT(status == MMAY_CHECK_OK && result.error_count == 0);
    if (status != MMAY_CHECK_OK || result.error_count != 0) {
        fprintf(stderr, "builtin fact fixture %s: status %d, errors %zu\n",
            family->package, (int)status, result.error_count);
        for (size_t index = 0; index < result.diagnostics.length && index < 8; index += 1) {
            const MarimayCheckDiagnostic *diagnostic = mmay_vector_at_const(&result.diagnostics, index);
            fprintf(stderr, "%s\n", diagnostic->message);
        }
        mmay_check_result_dispose(&result);
        mmay_checker_import_fixture_dispose(&fixture);
        return false;
    }
    const char *source = imported ? convert_application : family->source;
    const MarimaySourceSnapshot *snapshot = fixture.files[imported ? 1 : 0].ast.snapshot;
    for (size_t index = 0; index < family->count; index += 1) {
        char call[128];
        int length = snprintf(call, sizeof(call), "%s%s%s(",
            imported ? "return " : "const _ = ", imported ? "convert." : "", family->names[index]);
        MMAY_EXPECT(length > 0 && (size_t)length < sizeof(call));
        const char *match = strstr(source, call);
        MMAY_EXPECT(match != NULL);
        if (match == NULL) continue;
        size_t offset = (size_t)(match - source) + strlen(imported ? "return " : "const _ = ")
            + (imported ? strlen("convert.") : 0);
        builtin_expect(mmay_checked_definition_from_reference(&result, snapshot, offset),
            family, family->names[index]);
    }
    if (imported) {
        builtin_expect(builtin_definition(&result, snapshot, source, "read(\"2\")"), family, "parseInt");
        builtin_expect(builtin_definition(&result, snapshot, source, "format(1)"), family, "formatI64");
        builtin_expect(builtin_definition(&result, snapshot, source, "float(value)"), family, "formatF32");
        const MarimayCheckedSymbol *shadow = builtin_definition(&result, snapshot, source, "read(1)");
        MMAY_EXPECT(shadow != NULL && shadow->kind == MMAY_CHECKED_SYMBOL_PARAMETER);
    }
    mmay_check_result_dispose(&result);
    mmay_checker_import_fixture_dispose(&fixture);
    return true;
}

static void builtin_allocation_failures(const BuiltinFamily *family, bool imported) {
    MarimayCheckerImportFixture fixture = {0};
    bool complete = false;
    bool initialized = builtin_fixture(&fixture, family, imported);
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    for (size_t failure = 0; failure < 32768 && !complete; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult result = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_checker_import_fixture_check(&fixture,
            imported ? "example/app" : family->package, mmay_fail_allocator(&state),
            mmay_check_graph_default_options(), &result);
        if (status == MMAY_CHECK_OK) {
            complete = true;
            MMAY_EXPECT(result.error_count == 0);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        mmay_check_result_dispose(&result);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (status != MMAY_CHECK_OK && status != MMAY_CHECK_ALLOCATION_FAILED) break;
    }
    MMAY_EXPECT(complete);
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_checker_builtin_facts(void) {
    for (size_t index = 0; index < sizeof(families) / sizeof(families[0]); index += 1) {
        if (builtin_definitions(&families[index], index == 0))
            builtin_allocation_failures(&families[index], index == 0);
    }
}
