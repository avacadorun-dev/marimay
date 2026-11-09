#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "../../bootstrap/src/lsp/diagnostics.h"

typedef struct LspDiagnosticFixture {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *main_source;
    const MarimaySourceSnapshot *other_source;
    const MarimaySourceSnapshot *manifest_source;
    const MarimaySourceSnapshot *standard_source;
    LspDocument document;
    LspAnalysis analysis;
    MarimayCompilerUnit units[3];
    MarimayLexDiagnostic lexical;
    MarimayParseDiagnostic parsed;
    MarimayCheckDiagnostic checked[2];
    MarimayManifestDiagnostic manifest;
} LspDiagnosticFixture;

static MarimayVector lsp_diagnostic_borrowed_vector(void *data, size_t length,
    size_t element_size) {
    MarimayVector vector = {0};
    vector.data = data;
    vector.length = length;
    vector.capacity = length;
    vector.element_size = element_size;
    return vector;
}

static bool lsp_diagnostic_fixture_init(LspDiagnosticFixture *fixture,
    MarimayAllocator allocator) {
    static const char source[] = "/* \xf0\x9f\x98\x80 */ fn main(): int { return 0; }\n";
    static const char other[] = "fn other(): int { return unknown; }\n";
    static const char manifest[] = "module \"example/diagnostics\"\nlanguage \"0.1\"\n";
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(&fixture->sources, allocator)) return false;
    if (mmay_source_manager_add(&fixture->sources, "/diagnostics/main.my",
            strlen("/diagnostics/main.my"), source, sizeof(source) - 1,
            &fixture->main_source) != MMAY_SOURCE_OK
        || mmay_source_manager_add(&fixture->sources, "/diagnostics/other % file.my",
            strlen("/diagnostics/other % file.my"), other, sizeof(other) - 1,
            &fixture->other_source) != MMAY_SOURCE_OK
        || mmay_source_manager_add(&fixture->sources, "/diagnostics/mmay.mod",
            strlen("/diagnostics/mmay.mod"), manifest, sizeof(manifest) - 1,
            &fixture->manifest_source) != MMAY_SOURCE_OK
        || mmay_source_manager_add(&fixture->sources, "std/io/io.my",
            strlen("std/io/io.my"), other, sizeof(other) - 1,
            &fixture->standard_source) != MMAY_SOURCE_OK) return false;
    fixture->document.path.data = (unsigned char *)mmay_source_path(fixture->main_source);
    fixture->document.path.length = mmay_source_path_length(fixture->main_source);
    fixture->units[0].snapshot = fixture->main_source;
    fixture->units[1].snapshot = fixture->other_source;
    fixture->units[2].snapshot = fixture->standard_source;
    fixture->analysis.result.initialized = true;
    fixture->analysis.result.units = lsp_diagnostic_borrowed_vector(fixture->units,
        3, sizeof(fixture->units[0]));
    fixture->lexical = (MarimayLexDiagnostic){"L0001", "invalid token", 0, 2};
    fixture->parsed = (MarimayParseDiagnostic){"P0001", "expected a function body", 3, 8};
    fixture->checked[0].code = "R0001";
    fixture->checked[0].message = "unresolved name: unknown";
    fixture->checked[0].snapshot = fixture->other_source;
    fixture->checked[0].severity = MMAY_SEVERITY_ERROR;
    fixture->checked[0].start = 25;
    fixture->checked[0].end = 32;
    fixture->checked[1] = fixture->checked[0];
    fixture->checked[1].snapshot = fixture->main_source;
    fixture->checked[1].message = "example warning";
    fixture->checked[1].severity = MMAY_SEVERITY_WARNING;
    fixture->checked[1].start = 11;
    fixture->checked[1].end = 13;
    fixture->analysis.result.checked.initialized = true;
    fixture->analysis.result.checked.diagnostics = lsp_diagnostic_borrowed_vector(
        fixture->checked, 1, sizeof(fixture->checked[0]));
    fixture->manifest = (MarimayManifestDiagnostic){"M0001",
        "unknown manifest directive", 0, 6};
    fixture->analysis.target.problem_source = fixture->manifest_source;
    return true;
}

static bool lsp_diagnostic_render(LspDiagnosticFixture *fixture,
    MarimayAllocator allocator, LspStatus status, MarimayTestBuffer *output);

static void lsp_diagnostic_source_view(LspDiagnosticFixture *fixture) {
    static const char hex[] = "0123456789abcdef";
    static const char root[] = "/diagnostics";
    const unsigned char *hash = mmay_source_sha256(fixture->standard_source);
    LspDocument saved_document = fixture->document;
    MarimayTestBuffer output;
    char path[256], digest[65];
    size_t index = 0;
    int length = 0;
    for (index = 0; index < 32; index += 1) {
        digest[index * 2] = hex[hash[index] >> 4];
        digest[index * 2 + 1] = hex[hash[index] & 15];
    }
    digest[64] = 0;
    length = snprintf(path, sizeof(path), "%s/.mmay/lsp/stdlib/%s/std/io/io.my", root, digest);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(path));
    fixture->analysis.target.root_directory.data = (unsigned char *)root;
    fixture->analysis.target.root_directory.length = sizeof(root) - 1;
    fixture->document.path.data = (unsigned char *)path;
    fixture->document.path.length = (size_t)length;
    fixture->document.text.data = (unsigned char *)mmay_source_bytes(fixture->standard_source);
    fixture->document.text.length = mmay_source_length(fixture->standard_source);
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, mmay_system_allocator(), LSP_OK, &output));
    MMAY_EXPECT(output.length == 0);
    fixture->document.text.length = 0;
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, mmay_system_allocator(), LSP_INVALID, &output));
    MMAY_EXPECT(strstr(output.bytes, "\"code\":\"lsp-source-view\"") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"severity\":3") != NULL);
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_write_document_diagnostics(mmay_test_buffer_writer(&output),
        mmay_system_allocator(), &fixture->document, NULL, LSP_INPUT_ERROR));
    MMAY_EXPECT(strstr(output.bytes, "Open a project source file") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"severity\":3") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "lsp-analysis") == NULL);
    fixture->document = saved_document;
}

static bool lsp_diagnostic_render(LspDiagnosticFixture *fixture,
    MarimayAllocator allocator, LspStatus status, MarimayTestBuffer *output) {
    return lsp_write_document_diagnostics(mmay_test_buffer_writer(output), allocator,
        &fixture->document, &fixture->analysis, status);
}

static void lsp_diagnostic_variants(LspDiagnosticFixture *fixture) {
    MarimayTestBuffer output;
    MarimayAllocator allocator = mmay_system_allocator();
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_OK, &output));
    MMAY_EXPECT(strstr(output.bytes, "\"code\":\"lsp-dependency\"") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "unresolved name: unknown") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "file:///diagnostics/other%20%25%20file.my") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"character\":25") != NULL);
    fixture->analysis.result.checked.diagnostics.length = 2;
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_OK, &output));
    MMAY_EXPECT(strstr(output.bytes, "example warning") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "lsp-dependency") != NULL);
    fixture->checked[1].severity = MMAY_SEVERITY_ERROR;
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_OK, &output));
    MMAY_EXPECT(strstr(output.bytes, "lsp-dependency") == NULL);
    fixture->analysis.result.checked.diagnostics.length = 0;
    fixture->units[1].parsed.initialized = true;
    fixture->units[1].parsed.diagnostics = lsp_diagnostic_borrowed_vector(
        &fixture->parsed, 1, sizeof(fixture->parsed));
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_OK, &output));
    MMAY_EXPECT(strstr(output.bytes, "expected a function body") != NULL);
    fixture->units[1].lexed.initialized = true;
    fixture->units[1].lexed.diagnostics = lsp_diagnostic_borrowed_vector(
        &fixture->lexical, 1, sizeof(fixture->lexical));
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_OK, &output));
    MMAY_EXPECT(strstr(output.bytes, "invalid token") != NULL);
    fixture->analysis.target.manifest.initialized = true;
    fixture->analysis.target.manifest.diagnostics = lsp_diagnostic_borrowed_vector(
        &fixture->manifest, 1, sizeof(fixture->manifest));
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_INPUT_ERROR, &output));
    MMAY_EXPECT(strstr(output.bytes, "unknown manifest directive") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "file:///diagnostics/mmay.mod") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "lsp-analysis") == NULL);
    fixture->analysis.target.problem_code = "M0031";
    fixture->analysis.target.problem_message = "missing module identity";
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, allocator, LSP_INPUT_ERROR, &output));
    MMAY_EXPECT(strstr(output.bytes, "missing module identity") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":\"M0031\"") != NULL);
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_write_document_diagnostics(mmay_test_buffer_writer(&output),
        allocator, &fixture->document, NULL, LSP_ALLOCATION_FAILED));
    MMAY_EXPECT(strstr(output.bytes, "could not allocate memory") != NULL);
}

static void lsp_diagnostic_failures(LspDiagnosticFixture *fixture) {
    MarimayFailAllocator state;
    MarimayTestBuffer output;
    size_t attempts = 0, writes = 0, index = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, mmay_fail_allocator(&state),
        LSP_OK, &output));
    attempts = state.attempts;
    writes = output.calls;
    MMAY_EXPECT(attempts > 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < attempts; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        mmay_test_buffer_init(&output);
        MMAY_EXPECT(!lsp_diagnostic_render(fixture, mmay_fail_allocator(&state),
            LSP_OK, &output));
        MMAY_EXPECT(state.attempts > index);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    for (index = 0; index < writes; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        mmay_test_buffer_init(&output);
        output.failure_index = index;
        MMAY_EXPECT(!lsp_diagnostic_render(fixture, mmay_fail_allocator(&state),
            LSP_OK, &output));
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    /* The manifest path has the same location allocation owner but a distinct route. */
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    mmay_test_buffer_init(&output);
    MMAY_EXPECT(lsp_diagnostic_render(fixture, mmay_fail_allocator(&state),
        LSP_INPUT_ERROR, &output));
    attempts = state.attempts;
    MMAY_EXPECT(attempts > 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < attempts; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        mmay_test_buffer_init(&output);
        MMAY_EXPECT(!lsp_diagnostic_render(fixture, mmay_fail_allocator(&state),
            LSP_INPUT_ERROR, &output));
        MMAY_EXPECT(state.attempts > index);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void mmay_test_lsp_diagnostics(void) {
    LspDiagnosticFixture fixture;
    MarimayFailAllocator state;
    bool ready = false;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    ready = lsp_diagnostic_fixture_init(&fixture, mmay_fail_allocator(&state));
    MMAY_EXPECT(ready);
    if (ready) {
        lsp_diagnostic_variants(&fixture);
        lsp_diagnostic_source_view(&fixture);
        lsp_diagnostic_failures(&fixture);
    }
    mmay_source_manager_dispose(&fixture.sources);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}
