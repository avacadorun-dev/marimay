#include "test_support.h"

#include <string.h>

#include "../../bootstrap/src/lsp/output.h"
#include "../../bootstrap/src/lsp/positions.h"
#include "../../bootstrap/src/lsp/rename.h"

static LspQueryStatus lsp_rename_test_run(MarimayAllocator allocator,
    const char *uri, const char *source, const char *needle,
    const char *new_name, bool prepare, size_t output_failure,
    MarimayTestBuffer *output) {
    LspWorkspace workspace = {0};
    LspOutput params;
    MarimayLspJson json = {0};
    MarimayWriter writer;
    const LspDocument *document;
    size_t line = 0, character = 0;
    const char *found = strstr(source, needle);
    LspQueryStatus status = LSP_QUERY_FAILED;
    bool params_initialized = false;
    mmay_test_buffer_init(output);
    output->failure_index = output_failure;
    if (found == NULL || !lsp_workspace_init(&workspace, allocator)) goto cleanup;
    if (lsp_workspace_open(&workspace, uri, strlen(uri), 77,
            source, strlen(source)) != LSP_OK) goto cleanup;
    if (!lsp_offset_to_position((const unsigned char *)source, strlen(source),
            (size_t)(found - source), &line, &character)) goto cleanup;
    params_initialized = lsp_output_init(&params, allocator);
    if (!params_initialized) goto cleanup;
    writer = lsp_output_writer(&params);
    if (!mmay_writer_write_cstr(writer, "{\"textDocument\":{\"uri\":")
        || !mmay_lsp_json_write_string(writer, uri, strlen(uri))
        || !mmay_writer_write_cstr(writer, "},\"position\":{\"line\":")
        || !lsp_write_size(writer, line)
        || !mmay_writer_write_cstr(writer, ",\"character\":")
        || !lsp_write_size(writer, character)
        || !mmay_writer_write_cstr(writer, "},\"newName\":")
        || !mmay_lsp_json_write_string(writer, new_name, strlen(new_name))
        || !mmay_writer_write_cstr(writer, "}")) goto cleanup;
    if (!mmay_lsp_json_parse(&json, allocator, params.bytes.data, params.bytes.length)) goto cleanup;
    status = lsp_rename(&workspace, &json, 0, prepare, mmay_test_buffer_writer(output));
    document = lsp_workspace_document(&workspace, uri, strlen(uri));
    MMAY_EXPECT(document != NULL && document->version == 77);
    MMAY_EXPECT(document != NULL && document->text.length == strlen(source)
        && memcmp(document->text.data, source, strlen(source)) == 0);
cleanup:
    mmay_lsp_json_dispose(&json);
    if (params_initialized) lsp_output_dispose(&params);
    lsp_workspace_dispose(&workspace);
    return status;
}

static LspQueryStatus lsp_rename_test_request(MarimayAllocator allocator,
    const char *uri, const char *source, const char *needle,
    const char *new_name, bool prepare, MarimayTestBuffer *output) {
    return lsp_rename_test_run(allocator, uri, source, needle,
        new_name, prepare, SIZE_MAX, output);
}

static size_t lsp_rename_test_count(const char *text, const char *needle) {
    size_t count = 0;
    size_t length = strlen(needle);
    const char *next = text;
    while ((next = strstr(next, needle)) != NULL) { count += 1; next += length; }
    return count;
}

static void lsp_rename_test_top_level(const char *uri) {
    static const char source[] =
        "fn answer(): int { return 42; }\n"
        "fn caller(): int { const _ = \"answer\"; /* answer */ return answer(); }\n";
    static const char *invalid[] = {"", "fn", "_", "bad name", "a.b", "1name"};
    MarimayTestBuffer output;
    size_t index;
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        source, "answer();", "result", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"documentChanges\":[") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"version\":77") != NULL);
    MMAY_EXPECT(lsp_rename_test_count(output.bytes, "\"newText\":\"result\"") == 2);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        source, "answer();", "", true, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"placeholder\":\"answer\"") != NULL);
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1)
        MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
            source, "answer();", invalid[index], false, &output) == LSP_QUERY_REJECTED);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        source, "answer();", "caller", false, &output) == LSP_QUERY_REJECTED);
}

static void lsp_rename_test_locals(const char *uri) {
    static const char shadowed[] =
        "fn main(): int { const value = 1; if true { const value = 2; const _ = value; } return value; }\n";
    static const char captures_target[] =
        "fn main(): int { const value = 1; if true { const other = 2; const _ = value + other; } return value; }\n";
    static const char captures_unchanged[] =
        "fn main(): int { const other = 1; if true { const value = 2; const _ = other + value; } return other; }\n";
    MarimayTestBuffer output;
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        shadowed, "value = 1", "outer", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(lsp_rename_test_count(output.bytes, "\"newText\":\"outer\"") == 2);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        captures_target, "value = 1", "other", false, &output) == LSP_QUERY_REJECTED);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        captures_unchanged, "value = 2", "other", false, &output) == LSP_QUERY_REJECTED);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        "fn same(value: int): int { return value; }\n", "value:",
        "parameter", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(lsp_rename_test_count(output.bytes, "\"newText\":\"parameter\"") == 2);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        "fn same(value: int): int { return value; }\n", ": int",
        "", true, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"placeholder\":\"value\"") != NULL);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        shadowed, "value = 1", "счётчик", false, &output) == LSP_QUERY_OK);
}

static const char lsp_rename_test_shorthand[] =
    "type Box { value: int, }\n"
    "fn main(): int { const value = 1; const box = Box { value }; return box.value + value; }\n";

static void lsp_rename_test_fields(const char *uri) {
    static const char separate[] =
        "type First { value: int, }\n"
        "type Second { value: int, }\n"
        "fn main(): int { const a = First { value: 1 }; const b = Second { value: 2 }; return a.value + b.value; }\n";
    static const char duplicate[] =
        "type Pair { first: int, second: int, }\n"
        "fn main(): int { const pair = Pair { first: 1, second: 2 }; return pair.first; }\n";
    static const char pattern[] =
        "type Box { value: int, }\n"
        "fn read(box: Box): int { return match box { Box { value } => value, }; }\n";
    MarimayTestBuffer output;
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        separate, "value: int", "amount", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(lsp_rename_test_count(output.bytes, "\"newText\":\"amount\"") == 3);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        separate, ": int", "", true, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"placeholder\":\"value\"") != NULL);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        "type Box { поле: int, }\n", ": int", "", true, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"placeholder\":\"поле\"") != NULL);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        "type Box { value  : int, }\n", " : int", "", true,
        &output) == LSP_QUERY_REJECTED);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        lsp_rename_test_shorthand, "value: int", "amount", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"newText\":\"amount: value\"") != NULL);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        lsp_rename_test_shorthand, "value = 1", "payload", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"newText\":\"value: payload\"") != NULL);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        lsp_rename_test_shorthand, "value: int", "value", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strcmp(output.bytes, "{\"documentChanges\":[]}") == 0);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        duplicate, "first:", "second", false, &output) == LSP_QUERY_REJECTED);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        pattern, "value: int", "amount", false, &output) == LSP_QUERY_OK);
    MMAY_EXPECT(strstr(output.bytes, "\"newText\":\"amount: value\"") != NULL);
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        pattern, "value }", "payload", true, &output) == LSP_QUERY_REJECTED);
}

static void lsp_rename_test_output_failures(const char *uri) {
    MarimayFailAllocator state;
    MarimayTestBuffer output;
    size_t index, calls;
    MMAY_EXPECT(lsp_rename_test_request(mmay_system_allocator(), uri,
        lsp_rename_test_shorthand, "value: int", "amount", false, &output) == LSP_QUERY_OK);
    calls = output.calls;
    for (index = 0; index < calls; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(lsp_rename_test_run(mmay_fail_allocator(&state), uri,
            lsp_rename_test_shorthand, "value: int", "amount", false,
            index, &output) == LSP_QUERY_FAILED);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

static void lsp_rename_test_failures(const char *uri) {
    static const char capture[] =
        "fn main(): int { const other = 1; if true { const value = 2; const _ = other + value; } return other; }\n";
    const char *sources[] = {lsp_rename_test_shorthand, capture};
    const char *needles[] = {"value: int", "value = 2"};
    const char *names[] = {"amount", "other"};
    LspQueryStatus expected[] = {LSP_QUERY_OK, LSP_QUERY_REJECTED};
    size_t scenario, index, attempts;
    MarimayFailAllocator state;
    MarimayTestBuffer output;
    for (scenario = 0; scenario < 2; scenario += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(lsp_rename_test_request(mmay_fail_allocator(&state), uri,
            sources[scenario], needles[scenario], names[scenario], false, &output) == expected[scenario]);
        attempts = state.attempts;
        MMAY_EXPECT(attempts != 0 && state.live_allocations == 0 && state.live_bytes == 0);
        for (index = 0; index < attempts; index += 1) {
            mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
            (void)lsp_rename_test_request(mmay_fail_allocator(&state), uri,
                sources[scenario], needles[scenario], names[scenario], false, &output);
            MMAY_EXPECT(state.attempts > index);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
    }
}

static bool lsp_rename_test_uri(MarimayOwnedBytes *uri) {
    MarimayOwnedBytes current = {0}, path = {0};
    LspOutput full_path;
    bool initialized = false;
    bool ok = false;
    if (mmay_directory_current(mmay_system_allocator(), 32768, &current) != MMAY_FILE_OK) {
        MMAY_EXPECT(false);
        return false;
    }
    initialized = lsp_output_init(&full_path, mmay_system_allocator());
    if (!initialized || !mmay_writer_write(lsp_output_writer(&full_path),
            (const char *)current.data, current.length)
        || !mmay_writer_write_cstr(lsp_output_writer(&full_path),
            "/lsp-rename-nonexistent/script.my")
        || !mmay_owned_bytes_copy(&path, mmay_system_allocator(),
            full_path.bytes.data, full_path.bytes.length)
        || lsp_path_to_uri(mmay_system_allocator(), (const char *)path.data,
            path.length, uri) != LSP_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    ok = true;
cleanup:
    if (initialized) lsp_output_dispose(&full_path);
    mmay_owned_bytes_dispose(&current);
    mmay_owned_bytes_dispose(&path);
    return ok;
}

void mmay_test_lsp_rename_semantics(void) {
    MarimayOwnedBytes uri = {0};
    if (!lsp_rename_test_uri(&uri)) return;
    lsp_rename_test_top_level((const char *)uri.data);
    lsp_rename_test_locals((const char *)uri.data);
    lsp_rename_test_fields((const char *)uri.data);
    lsp_rename_test_output_failures((const char *)uri.data);
    mmay_owned_bytes_dispose(&uri);
}

void mmay_test_lsp_rename(void) {
    MarimayOwnedBytes uri = {0};
    mmay_test_lsp_rename_semantics();
    if (!lsp_rename_test_uri(&uri)) return;
    lsp_rename_test_failures((const char *)uri.data);
    mmay_owned_bytes_dispose(&uri);
}
