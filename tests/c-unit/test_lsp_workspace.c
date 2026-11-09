#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "../../bootstrap/src/lsp/positions.h"

static const char lsp_test_source[] =
    "public fn answer(): int { return 40; }\n"
    "fn caller(): int { return answer() + 2; }\n";

static bool lsp_test_equal(const MarimayOwnedBytes *bytes, const char *text) {
    return bytes->length == strlen(text)
        && memcmp(bytes->data, text, bytes->length) == 0;
}

static void lsp_test_positions(void) {
    static const unsigned char text[] = "a\xc3\xa9\xf0\x9f\x98\x80z\r\nnext\n";
    size_t offset = 0, line = 0, character = 0;
    MMAY_EXPECT(lsp_position_to_offset(text, sizeof(text) - 1, 0, 4, &offset));
    MMAY_EXPECT(offset == 7);
    MMAY_EXPECT(lsp_offset_to_position(text, sizeof(text) - 1, offset, &line, &character));
    MMAY_EXPECT(line == 0 && character == 4);
    MMAY_EXPECT(!lsp_position_to_offset(text, sizeof(text) - 1, 0, 3, &offset));
    MMAY_EXPECT(!lsp_offset_to_position(text, sizeof(text) - 1, 4, &line, &character));
    MMAY_EXPECT(lsp_position_to_offset(text, sizeof(text) - 1, 1, 0, &offset));
    MMAY_EXPECT(offset == 10);
    MMAY_EXPECT(!lsp_offset_to_position(text, sizeof(text) - 1, 9, &line, &character));
    MMAY_EXPECT(lsp_offset_to_position(text, sizeof(text) - 1, 8, &line, &character));
    MMAY_EXPECT(line == 0 && character == 5);
    MMAY_EXPECT(lsp_position_to_offset(text, sizeof(text) - 1, 2, 0, &offset));
    MMAY_EXPECT(offset == sizeof(text) - 1);
    MMAY_EXPECT(!lsp_position_to_offset(text, sizeof(text) - 1, 3, 0, &offset));
    MMAY_EXPECT(!lsp_position_to_offset(text, sizeof(text) - 1, 1, 5, &offset));
    MMAY_EXPECT(lsp_position_to_offset(NULL, 0, 0, 0, &offset) && offset == 0);
    MMAY_EXPECT(lsp_offset_to_position(NULL, 0, 0, &line, &character));
    MMAY_EXPECT(line == 0 && character == 0);
    MMAY_EXPECT(!lsp_position_to_offset((const unsigned char *)"\xff", 1, 0, 1, &offset));
    MMAY_EXPECT(lsp_position_to_offset((const unsigned char *)"a\rb", 3, 1, 1, &offset));
    MMAY_EXPECT(offset == 3);
}

static void lsp_test_uri(void) {
    static const char *invalid[] = {
        "untitled:new.my", "https://host/file.my", "file://remote/file.my",
        "file:///a%00b.my", "file:///a%Q0.my", "file:///a%.my",
        "file:///a.my?query", "file:///a.my#fragment", "file:///a%FF.my",
        "file:///a\\b.my", "file:////remote/share/a.my"
    };
    MarimayOwnedBytes path = {0}, uri = {0};
    MarimayAllocator allocator = mmay_system_allocator();
    const char *encoded = "file:///tmp/space%20%23%25%3F%20%C3%A9.my";
    size_t index;
    MMAY_EXPECT(lsp_uri_to_path(allocator, encoded, strlen(encoded), &path) == LSP_OK);
    MMAY_EXPECT(lsp_test_equal(&path, "/tmp/space #%? \xc3\xa9.my"));
    MMAY_EXPECT(lsp_path_to_uri(allocator, (const char *)path.data,
        path.length, &uri) == LSP_OK);
    MMAY_EXPECT(lsp_test_equal(&uri, encoded));
    mmay_owned_bytes_dispose(&path);
    mmay_owned_bytes_dispose(&uri);
    encoded = "file://localhost/tmp/a.my";
    MMAY_EXPECT(lsp_uri_to_path(allocator, encoded, strlen(encoded), &path) == LSP_OK);
    MMAY_EXPECT(lsp_test_equal(&path, "/tmp/a.my"));
    mmay_owned_bytes_dispose(&path);
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MMAY_EXPECT(lsp_uri_to_path(allocator, invalid[index], strlen(invalid[index]),
            &path) == LSP_INVALID);
        MMAY_EXPECT(path.data == NULL);
    }
    MMAY_EXPECT(lsp_uri_to_path(allocator, "file:///a\0b.my", 14, &path) == LSP_INVALID);
    MMAY_EXPECT(lsp_path_to_uri(allocator, "relative.my", 11, &uri) == LSP_INVALID);
    MMAY_EXPECT(lsp_path_to_uri(allocator, "C:/space dir/a.my",
        strlen("C:/space dir/a.my"), &uri) == LSP_OK);
    MMAY_EXPECT(lsp_test_equal(&uri, "file:///C:/space%20dir/a.my"));
    mmay_owned_bytes_dispose(&uri);
}

static void lsp_test_document_lifecycle(const char *uri) {
    LspWorkspace workspace = {0};
    const LspDocument *document;
    size_t index;
    char other_uri[128];
    MMAY_EXPECT(lsp_workspace_init(&workspace, mmay_system_allocator()));
    MMAY_EXPECT(lsp_workspace_open(&workspace, uri, strlen(uri), 7,
        lsp_test_source, sizeof(lsp_test_source) - 1) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_open(&workspace, uri, strlen(uri), 8, "", 0) == LSP_INVALID);
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 7, "", 0) == LSP_STALE);
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 6, "", 0) == LSP_STALE);
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 8,
        "\xff", 1) == LSP_INVALID);
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 8,
        "", 16U * 1024U * 1024U + 1U) == LSP_LIMIT);
    document = lsp_workspace_document(&workspace, uri, strlen(uri));
    MMAY_EXPECT(document != NULL && document->version == 7);
    MMAY_EXPECT(document != NULL && lsp_test_equal(&document->text, lsp_test_source));
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 9, "", 0) == LSP_OK);
    MMAY_EXPECT(workspace.total_source_bytes == 0);
    MMAY_EXPECT(lsp_workspace_close(&workspace, uri, strlen(uri)) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_close(&workspace, uri, strlen(uri)) == LSP_INVALID);
    for (index = 0; index < 64; index += 1) {
        int written = snprintf(other_uri, sizeof(other_uri), "file:///lsp-open-%zu.my", index);
        MMAY_EXPECT(written > 0 && (size_t)written < sizeof(other_uri));
        MMAY_EXPECT(lsp_workspace_open(&workspace, other_uri, (size_t)written,
            1, "", 0) == LSP_OK);
    }
    MMAY_EXPECT(lsp_workspace_open(&workspace, uri, strlen(uri), 1, "", 0) == LSP_LIMIT);
    MMAY_EXPECT(lsp_workspace_close(&workspace, "file:///lsp-open-0.my", 21) == LSP_OK);
    MMAY_EXPECT(workspace.documents.length == 63);
    MMAY_EXPECT(lsp_workspace_document(&workspace, "file:///lsp-open-1.my", 21) != NULL);
    lsp_workspace_dispose(&workspace);
}

static void lsp_test_analysis(const char *uri) {
    LspWorkspace workspace = {0};
    LspAnalysis analysis = {0};
    const LspDocument *document;
    const MarimaySourceSnapshot *snapshot;
    const MarimayCheckedSymbol *definition = NULL;
    const char *reference = strstr(lsp_test_source, "answer() +");
    MMAY_EXPECT(lsp_workspace_init(&workspace, mmay_system_allocator()));
    MMAY_EXPECT(lsp_workspace_open(&workspace, uri, strlen(uri), 1,
        lsp_test_source, sizeof(lsp_test_source) - 1) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_analyze(&workspace, uri, strlen(uri), &analysis) == LSP_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&analysis.result));
    document = lsp_workspace_document(&workspace, uri, strlen(uri));
    snapshot = lsp_analysis_snapshot(&analysis, (const char *)document->path.data,
        document->path.length);
    MMAY_EXPECT(snapshot != NULL);
    MMAY_EXPECT(mmay_compiler_service_definition_from_reference(&analysis.target.service,
        &analysis.result, snapshot, (size_t)(reference - lsp_test_source),
        &definition) == MMAY_COMPILER_OK);
    MMAY_EXPECT(definition != NULL && lsp_test_equal(&definition->name, "answer"));
    lsp_analysis_dispose(&analysis);
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 2,
        "fn broken(: int {", 17) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_analyze(&workspace, uri, strlen(uri), &analysis) == LSP_OK);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&analysis.result));
    lsp_analysis_dispose(&analysis);
    MMAY_EXPECT(lsp_workspace_change(&workspace, uri, strlen(uri), 3,
        "fn broken(): int { return missing; }",
        strlen("fn broken(): int { return missing; }")) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_analyze(&workspace, uri, strlen(uri), &analysis) == LSP_OK);
    MMAY_EXPECT(mmay_compiler_result_has_errors(&analysis.result));
    MMAY_EXPECT(analysis.result.checked.diagnostics.length != 0);
    lsp_analysis_dispose(&analysis);
    lsp_workspace_dispose(&workspace);
}

static LspStatus lsp_test_allocated_flow(MarimayAllocator allocator,
    const char *uri, bool analysis_enabled) {
    LspWorkspace workspace = {0};
    LspAnalysis analysis = {0};
    MarimayOwnedBytes path = {0}, encoded = {0};
    LspStatus status = LSP_ALLOCATION_FAILED;
    if (!lsp_workspace_init(&workspace, allocator)) goto cleanup;
    status = lsp_workspace_open(&workspace, uri, strlen(uri), 1,
        lsp_test_source, sizeof(lsp_test_source) - 1);
    if (status != LSP_OK) goto cleanup;
    status = lsp_workspace_change(&workspace, uri, strlen(uri), 2,
        lsp_test_source, sizeof(lsp_test_source) - 1);
    if (status != LSP_OK) goto cleanup;
    status = lsp_uri_to_path(allocator, uri, strlen(uri), &path);
    if (status != LSP_OK) goto cleanup;
    status = lsp_path_to_uri(allocator, (const char *)path.data, path.length, &encoded);
    if (status != LSP_OK) goto cleanup;
    if (analysis_enabled) status = lsp_workspace_analyze(&workspace, uri, strlen(uri), &analysis);
cleanup:
    mmay_owned_bytes_dispose(&path);
    mmay_owned_bytes_dispose(&encoded);
    lsp_analysis_dispose(&analysis);
    lsp_workspace_dispose(&workspace);
    return status;
}

static void lsp_test_allocations(const char *uri, bool analysis_enabled) {
    MarimayFailAllocator state;
    size_t attempts, index;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(lsp_test_allocated_flow(mmay_fail_allocator(&state), uri,
        analysis_enabled) == LSP_OK);
    attempts = state.attempts;
    MMAY_EXPECT(attempts != 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index <= attempts; index += 1) {
        LspStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        status = lsp_test_allocated_flow(mmay_fail_allocator(&state), uri, analysis_enabled);
        MMAY_EXPECT(status == LSP_OK || status == LSP_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
}

static bool lsp_test_write(const char *path, const char *text) {
    FILE *file = mmay_test_open_file(path, "wb");
    bool ok;
    if (file == NULL) return false;
    ok = fwrite(text, 1, strlen(text), file) == strlen(text);
    return fclose(file) == 0 && ok;
}

static void lsp_test_module(const MarimayOwnedBytes *current) {
    static const char manifest[] = "module \"lsp/test\"\nlanguage \"0.1\"\n";
    static const char caller[] = "fn caller(): int { return answer(); }\n";
    static const char imported[] =
        "import { answer } from \"lsp/test\";\n"
        "fn nested(): int { return answer(); }\n";
    char root[32768], manifest_path[32768], caller_path[32768], definition_path[32768];
    char nested_directory[32768], new_path[32768];
    MarimayOwnedBytes uri = {0}, new_uri = {0};
    MarimayAllocator allocator = mmay_system_allocator();
    LspWorkspace workspace = {0};
    LspAnalysis analysis = {0};
    const MarimaySourceSnapshot *snapshot;
    const MarimayCheckedSymbol *definition = NULL;
    size_t path_index;
    int length = snprintf(root, sizeof(root), "%s/lsp-workspace-fixture", current->data);
    bool created;
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(root));
    if (length <= 0 || (size_t)length >= sizeof(root)) return;
    for (path_index = 0; path_index < (size_t)length; path_index += 1)
        if (root[path_index] == '\\') root[path_index] = '/';
    created = mmay_directory_create_exclusive(allocator, root, strlen(root)) == MMAY_FILE_OK;
    MMAY_EXPECT(created);
    if (!created) return;
    length = snprintf(manifest_path, sizeof(manifest_path), "%s/mmay.mod", root);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(manifest_path));
    length = snprintf(caller_path, sizeof(caller_path), "%s/main.my", root);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(caller_path));
    length = snprintf(definition_path, sizeof(definition_path), "%s/value.my", root);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(definition_path));
    length = snprintf(nested_directory, sizeof(nested_directory), "%s/nested", root);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(nested_directory));
    length = snprintf(new_path, sizeof(new_path), "%s/new.my", nested_directory);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(new_path));
    MMAY_EXPECT(lsp_test_write(manifest_path, manifest));
    MMAY_EXPECT(lsp_test_write(caller_path, caller));
    MMAY_EXPECT(lsp_test_write(definition_path, "public fn answer(): int { return 40; }\n"));
    MMAY_EXPECT(mmay_directory_create_exclusive(allocator, nested_directory,
        strlen(nested_directory)) == MMAY_FILE_OK);
    MMAY_EXPECT(lsp_path_to_uri(allocator, caller_path, strlen(caller_path), &uri) == LSP_OK);
    MMAY_EXPECT(lsp_path_to_uri(allocator, new_path, strlen(new_path), &new_uri) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_init(&workspace, allocator));
    MMAY_EXPECT(lsp_workspace_open(&workspace, (const char *)uri.data, uri.length,
        1, caller, strlen(caller)) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_analyze(&workspace, (const char *)uri.data,
        uri.length, &analysis) == LSP_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&analysis.result));
    snapshot = lsp_analysis_snapshot(&analysis, caller_path, strlen(caller_path));
    MMAY_EXPECT(snapshot != NULL);
    MMAY_EXPECT(mmay_compiler_service_definition_from_reference(&analysis.target.service,
        &analysis.result, snapshot, (size_t)(strstr(caller, "answer") - caller),
        &definition) == MMAY_COMPILER_OK);
    MMAY_EXPECT(definition != NULL && strcmp(mmay_source_path(definition->snapshot),
        definition_path) == 0);
    lsp_analysis_dispose(&analysis);
    MMAY_EXPECT(lsp_workspace_open(&workspace, (const char *)new_uri.data, new_uri.length,
        1, imported, strlen(imported)) == LSP_OK);
    MMAY_EXPECT(lsp_workspace_analyze(&workspace, (const char *)new_uri.data,
        new_uri.length, &analysis) == LSP_OK);
    MMAY_EXPECT(!mmay_compiler_result_has_errors(&analysis.result));
    MMAY_EXPECT(lsp_analysis_snapshot(&analysis, new_path, strlen(new_path)) != NULL);
    lsp_analysis_dispose(&analysis);
    MMAY_EXPECT(lsp_test_write(manifest_path, "module \"unterminated\n"));
    MMAY_EXPECT(lsp_workspace_analyze(&workspace, (const char *)uri.data,
        uri.length, &analysis) == LSP_INPUT_ERROR);
    MMAY_EXPECT(analysis.target_status == MMAY_CLI_CHECK_TARGET_SOURCE_INVALID);
    MMAY_EXPECT(analysis.target.manifest.diagnostics.length != 0);
    lsp_analysis_dispose(&analysis);
    MMAY_EXPECT(lsp_test_write(manifest_path, manifest));
    lsp_workspace_dispose(&workspace);
    lsp_test_allocations((const char *)uri.data, true);
    lsp_test_allocations((const char *)new_uri.data, true);
    mmay_owned_bytes_dispose(&uri);
    mmay_owned_bytes_dispose(&new_uri);
    MMAY_EXPECT(mmay_test_remove_file(caller_path) == 0);
    MMAY_EXPECT(mmay_test_remove_file(definition_path) == 0);
    MMAY_EXPECT(mmay_test_remove_file(manifest_path) == 0);
    MMAY_EXPECT(mmay_directory_remove_empty(allocator, nested_directory,
        strlen(nested_directory)) == MMAY_FILE_OK);
    MMAY_EXPECT(mmay_directory_remove_empty(allocator, root, strlen(root)) == MMAY_FILE_OK);
}

void mmay_test_lsp_workspace(void) {
    MarimayOwnedBytes current = {0}, uri = {0};
    MarimayVector path = {0};
    static const char suffix[] = "/lsp-missing-directory/new.my";
    MMAY_EXPECT(mmay_directory_current(mmay_system_allocator(), 32768, &current) == MMAY_FILE_OK);
    MMAY_EXPECT(mmay_vector_init(&path, mmay_system_allocator(), 1));
    MMAY_EXPECT(mmay_vector_append(&path, current.data, current.length));
    MMAY_EXPECT(mmay_vector_append(&path, suffix, sizeof(suffix) - 1));
    MMAY_EXPECT(lsp_path_to_uri(mmay_system_allocator(), path.data, path.length, &uri) == LSP_OK);
    lsp_test_positions();
    lsp_test_uri();
    lsp_test_document_lifecycle((const char *)uri.data);
    lsp_test_analysis((const char *)uri.data);
    lsp_test_allocations((const char *)uri.data, false);
    lsp_test_allocations((const char *)uri.data, true);
    lsp_test_module(&current);
    mmay_owned_bytes_dispose(&current);
    mmay_owned_bytes_dispose(&uri);
    mmay_vector_dispose(&path);
}
