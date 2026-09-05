#include "test_checker_imports_support.h"

/* This tiny cursor surface isolates checker allocation/typing, not JSON parsing.
 * The public typed-json process corpus executes the real shipped cursor in both engines. */
static const char mmay_json_signature_fixture[] =
    "public type Bytes { public size: int, }\n"
    "public type Limits { public size: int, }\n"
    "public type Decoder { public size: int, }\n"
    "public type Encoder { public size: int, }\n"
    "public enum Error { Bad, }\n"
    "public enum UnknownFields { Reject, Skip, }\n"
    "public fn defaultLimits(): Limits { return Limits { size: 1 }; }\n"
    "public fn newDecoder(input: Bytes, limits: Limits): Result<Decoder, Error> { return Ok(Decoder { size: 0 }); }\n"
    "public fn newEncoder(limits: Limits): Result<Encoder, Error> { return Ok(Encoder { size: 0 }); }\n"
    "public fn finishDecoder(value: Decoder): Result<void, Error> { return Ok(); }\n"
    "public fn finishEncoder(value: Encoder): Result<Bytes, Error> { return Ok(Bytes { size: 0 }); }\n"
    "public fn readInt(value: Decoder): Result<int, Error> { return Ok(1); }\n"
    "public fn readString(value: Decoder): Result<string, Error> { return Ok(\"\"); }\n"
    "public fn readBool(value: Decoder): Result<bool, Error> { return Ok(true); }\n"
    "public fn writeInt(value: Encoder, item: int): Result<void, Error> { return Ok(); }\n"
    "public fn writeString(value: Encoder, item: string): Result<void, Error> { return Ok(); }\n"
    "public fn writeBool(value: Encoder, item: bool): Result<void, Error> { return Ok(); }\n"
    "public fn beginArray(value: Decoder): Result<void, Error> { return Ok(); }\n"
    "public fn beginArray(value: Encoder): Result<void, Error> { return Ok(); }\n"
    "public fn endArray(value: Encoder): Result<void, Error> { return Ok(); }\n"
    "public fn nextElement(value: Decoder): Result<bool, Error> { return Ok(false); }\n"
    "public fn beginObject(value: Decoder): Result<void, Error> { return Ok(); }\n"
    "public fn beginObject(value: Encoder): Result<void, Error> { return Ok(); }\n"
    "public fn endObject(value: Encoder): Result<void, Error> { return Ok(); }\n"
    "public fn nextField(value: Decoder): Result<Option<string>, Error> { return Ok(None); }\n"
    "public fn writeKey(value: Encoder, name: string): Result<void, Error> { return Ok(); }\n"
    "public fn missingField(value: Decoder, name: string): Error { return Error.Bad; }\n"
    "public fn handleUnknown(value: Decoder, name: string, policy: UnknownFields): Result<void, Error> { return Err(Error.Bad); }\n";

static const char mmay_json_check_source[] =
    "import * as json from \"std/json\";\n"
    "import { decode as read, encode as write } from \"std/json\";\n"
    "type Box<T> { public value: T, }\n"
    "type Note { public id: int, public text: string, public tags: []string, }\n"
    "type Data { public notes: []Note, public empty: Box<[]bool>, }\n"
    "fn probe(input: json.Bytes): Result<json.Bytes, json.Error> {\n"
    " const data = read<Data>(input)?; return write(data);\n"
    "}\n"
    "fn limited(input: json.Bytes): Result<json.Bytes, json.Error> {\n"
    " const data = json.decodeWithLimits<Data>(input, json.defaultLimits())?;\n"
    " return json.encodeWithLimits<Data>(data, json.defaultLimits());\n"
    "}\n";

static bool mmay_json_check_fixture(MarimayCheckerImportFixture *fixture, const char *source) {
    const MarimayTestImportSource sources[] = {
        {"std/json", "std/json/json.my", mmay_json_signature_fixture},
        {"test/json", "test/json/main.my", source}
    };
    return mmay_checker_import_fixture_init(fixture, sources, 2);
}

static void mmay_json_checker_allocation_failures(void) {
    MarimayCheckerImportFixture fixture;
    MarimayFailAllocator state;
    MarimayCheckResult checked;
    MarimayCheckStatus status;
    size_t attempts = 0, index = 0;
    if (!mmay_json_check_fixture(&fixture, mmay_json_check_source)) {
        MMAY_EXPECT(false);
        return;
    }
    memset(&checked, 0, sizeof(checked));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    status = mmay_checker_import_fixture_check(&fixture, "test/json",
        mmay_fail_allocator(&state), mmay_check_graph_default_options(), &checked);
    MMAY_EXPECT(status == MMAY_CHECK_OK && checked.diagnostics.length == 0);
    if (status != MMAY_CHECK_OK || checked.diagnostics.length != 0) {
        mmay_check_result_dispose(&checked);
        mmay_checker_import_fixture_dispose(&fixture);
        return;
    }
    attempts = state.attempts;
    mmay_check_result_dispose(&checked);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index <= attempts; index += 1) {
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        status = mmay_checker_import_fixture_check(&fixture, "test/json",
            mmay_fail_allocator(&state), mmay_check_graph_default_options(), &checked);
        if (index < attempts) {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_OK && checked.diagnostics.length == 0);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (mmay_test_failure_count() != 0) break;
    }
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_json_checker_negative(void) {
    static const char *const sources[] = {
        "import * as json from \"std/json\"; type Secret { value: int, } fn f(input: json.Bytes): void { const _ = json.decode<Secret>(input); }",
        "import * as json from \"std/json\"; type Node { public next: []Node, } fn f(input: json.Bytes): void { const _ = json.decode<Node>(input); }",
        "import * as json from \"std/json\"; fn f(input: json.Bytes): void { const _ = json.decode<f64>(input); }",
        "import * as json from \"std/json\"; fn f(input: json.Bytes): void { const _ = json.decode<Option<int>>(input); }",
        "import * as json from \"std/json\"; fn f(input: json.Bytes): void { const _ = json.decode(input); }",
        "import * as json from \"std/json\"; fn f(input: json.Bytes): void { const _ = json.decode<int, bool>(input); }",
        "import * as json from \"std/json\"; fn f(): void { const _ = json.decode<int>(); }",
        "import * as json from \"std/json\"; fn f(): void { const _ = json.decode<int>(1); }",
        "import * as json from \"std/json\"; fn f(): void { const _ = json.encode<bool>(1); }",
        "import * as json from \"std/json\"; fn f(): void { const _ = json.encodeWithLimits(1, true); }"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); index += 1) {
        MarimayCheckerImportFixture fixture;
        MarimayCheckResult checked;
        MarimayFailAllocator allocator;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        if (!mmay_json_check_fixture(&fixture, sources[index])) {
            MMAY_EXPECT(false);
            continue;
        }
        mmay_fail_allocator_init(&allocator, mmay_system_allocator(), SIZE_MAX);
        status = mmay_checker_import_fixture_check(&fixture, "test/json",
            mmay_fail_allocator(&allocator), mmay_check_graph_default_options(), &checked);
        MMAY_EXPECT(status == MMAY_CHECK_OK);
        MMAY_EXPECT(checked.diagnostics.length != 0);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(allocator.live_allocations == 0 && allocator.live_bytes == 0);
        mmay_checker_import_fixture_dispose(&fixture);
    }
}

void mmay_test_checker_json(void) {
    mmay_json_checker_negative();
    mmay_json_checker_allocation_failures();
}
