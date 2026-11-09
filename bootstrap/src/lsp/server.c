#include "server.h"
#include "diagnostics.h"
#include "output.h"
#include "queries.h"
#include "transport.h"
#include "analysis_cache.h"
#include "positions.h"
#include "source_locations.h"

#include <string.h>
#include "mmay/build_config.h"
#include "mmay/cli.h"
#include "mmay/platform/stdio.h"

typedef struct LspServer {
    LspWorkspace workspace;
    MarimayWriter output;
    bool initialized;
    bool shutdown;
    bool exited;
    bool dynamic_watches;
    bool watches_registered;
} LspServer;

static bool lsp_register_watches(LspServer *server) {
    static const char request[] =
        "{\"jsonrpc\":\"2.0\",\"id\":\"mmay-watch-1\",\"method\":\"client/registerCapability\","
        "\"params\":{\"registrations\":[{\"id\":\"mmay-watched-sources\","
        "\"method\":\"workspace/didChangeWatchedFiles\",\"registerOptions\":{\"watchers\":["
        "{\"globPattern\":\"**/*.my\",\"kind\":7},"
        "{\"globPattern\":\"**/mmay.mod\",\"kind\":7},"
        "{\"globPattern\":\"**/mmay.work\",\"kind\":7}]}}]}}";
    if (!server->dynamic_watches || server->watches_registered) return true;
    server->watches_registered = true;
    return mmay_lsp_write_message(server->output, request, sizeof(request) - 1);
}

static bool lsp_watch_prefix(const char *path, size_t length,
    const char *root, size_t root_length) {
    while (root_length > 1 && root[root_length - 1] == '/') root_length -= 1;
    return root_length != 0 && length >= root_length
        && memcmp(path, root, root_length) == 0
        && (root[root_length - 1] == '/' || length == root_length || path[root_length] == '/');
}

static bool lsp_watch_ignored(const char *path, size_t length, size_t root_length) {
    size_t index = root_length, start;
    while (index < length) {
        if (path[index] == '/') { index += 1; continue; }
        start = index;
        while (index < length && path[index] != '/') index += 1;
        if ((index - start == 4 && memcmp(path + start, ".git", 4) == 0)
            || (index - start == 5 && memcmp(path + start, ".mmay", 5) == 0)) return true;
    }
    return false;
}

static const char *lsp_watch_root(const LspCachedAnalysis *entry, size_t *length) {
    const MarimayCliCheckTarget *target = &entry->analysis.target;
    const MarimaySourceSnapshot *manifest;
    const char *path;
    size_t parent;
    *length = 0;
    /* Failed analyses also retain discovery diagnostics until invalidation. */
    if (target->root_directory.length != 0) {
        *length = target->root_directory.length;
        return (const char *)target->root_directory.data;
    }
    /* A malformed manifest can stop discovery before root_directory is set. */
    manifest = target->manifest.snapshot != NULL
        ? target->manifest.snapshot : target->problem_source;
    if (manifest == NULL) return NULL;
    path = mmay_source_path(manifest);
    parent = mmay_source_path_length(manifest);
    while (parent != 0 && path[parent - 1] != '/') parent -= 1;
    size_t name_length = mmay_source_path_length(manifest) - parent;
    if (!((name_length == 8 && memcmp(path + parent, "mmay.mod", 8) == 0)
            || (name_length == 9 && memcmp(path + parent, "mmay.work", 9) == 0))) return NULL;
    *length = parent;
    return path;
}

static bool lsp_watch_relevant(const LspWorkspace *workspace,
    const char *path, size_t length) {
    size_t index, document_index;
    if (lsp_source_is_view(path, length)) return false;
    for (index = 0; index < LSP_ANALYSIS_CACHE_CAPACITY; index += 1) {
        const LspCachedAnalysis *entry = &workspace->analyses[index];
        size_t root_length;
        const char *root = lsp_watch_root(entry, &root_length);
        if (lsp_watch_prefix(path, length, root, root_length)
            && !lsp_watch_ignored(path, length, root_length)) return true;
    }
    /* Before discovery succeeds, changes beside an open file can repair its
     * package. A recorded module root is wider and remains authoritative. */
    for (document_index = 0; document_index < workspace->documents.length; document_index += 1) {
        const LspDocument *document = mmay_vector_at_const(&workspace->documents, document_index);
        const char *source = (const char *)document->path.data;
        size_t parent = document->path.length;
        bool known_root = false;
        if (lsp_source_is_view(source, parent)) continue;
        for (index = 0; index < LSP_ANALYSIS_CACHE_CAPACITY; index += 1) {
            const LspCachedAnalysis *entry = &workspace->analyses[index];
            size_t root_length;
            const char *root = lsp_watch_root(entry, &root_length);
            if (lsp_watch_prefix(source, parent, root, root_length)) { known_root = true; break; }
        }
        if (known_root) continue;
        while (parent != 0 && source[parent - 1] != '/') parent -= 1;
        if (lsp_watch_prefix(path, length, source, parent)
            && !lsp_watch_ignored(path, length, parent)) return true;
    }
    return false;
}

static bool lsp_workspace_changed(LspServer *server, const MarimayLspJson *json,
    size_t method, size_t params) {
    bool changed = false;
    if (mmay_lsp_json_equal(json, method, "workspace/didChangeConfiguration")) {
        const MarimayLspJsonToken *parameters = mmay_lsp_json_token(json, params);
        changed = parameters != NULL && parameters->kind == MMAY_LSP_JSON_OBJECT;
    } else {
        size_t changes = mmay_lsp_json_get(json, params, "changes"), index;
        const MarimayLspJsonToken *array = mmay_lsp_json_token(json, changes);
        if (array == NULL || array->kind != MMAY_LSP_JSON_ARRAY) return true;
        for (index = 0; ; index += 1) {
            size_t item = mmay_lsp_json_at(json, changes, index);
            MarimayOwnedBytes uri = {0}, path = {0};
            int64_t type = 0;
            LspStatus status;
            if (item == SIZE_MAX) break;
            const MarimayLspJsonToken *token = mmay_lsp_json_token(json,
                mmay_lsp_json_get(json, item, "uri"));
            if (token == NULL || token->kind != MMAY_LSP_JSON_STRING
                || !mmay_lsp_json_integer(json, mmay_lsp_json_get(json, item, "type"), &type)
                || type < 1 || type > 3) return true;
            if (!mmay_lsp_json_string(json, mmay_lsp_json_get(json, item, "uri"), &uri))
                return false;
            status = lsp_uri_to_path(server->workspace.allocator,
                (const char *)uri.data, uri.length, &path);
            if (status == LSP_OK && lsp_watch_relevant(&server->workspace,
                    (const char *)path.data, path.length))
                changed = true;
            mmay_owned_bytes_dispose(&path);
            mmay_owned_bytes_dispose(&uri);
            if (status != LSP_OK) return status != LSP_ALLOCATION_FAILED;
        }
    }
    if (!changed) return true;
    lsp_workspace_invalidate(&server->workspace);
    return lsp_publish_diagnostics(&server->workspace, server->output);
}

static bool lsp_response_prefix(MarimayWriter writer,
    const MarimayLspJson *json, size_t id) {
    const MarimayLspJsonToken *token = mmay_lsp_json_token(json, id);
    return mmay_writer_write_cstr(writer, "{\"jsonrpc\":\"2.0\",\"id\":")
        && (token != NULL
            ? mmay_writer_write(writer, (const char *)json->source + token->start,
                token->end - token->start)
            : mmay_writer_write_cstr(writer, "null"));
}

static bool lsp_error(LspServer *server, const MarimayLspJson *json,
    size_t id, int64_t code, const char *message) {
    LspOutput buffer;
    bool ok = lsp_output_init(&buffer, server->workspace.allocator);
    MarimayWriter writer = lsp_output_writer(&buffer);
    if (ok) ok = lsp_response_prefix(writer, json, id)
        && mmay_writer_write_cstr(writer, ",\"error\":{\"code\":")
        && lsp_write_integer(writer, code)
        && mmay_writer_write_cstr(writer, ",\"message\":")
        && mmay_lsp_json_write_string(writer, message, strlen(message))
        && mmay_writer_write_cstr(writer, "}}")
        && mmay_lsp_write_message(server->output, buffer.bytes.data, buffer.bytes.length);
    lsp_output_dispose(&buffer);
    return ok;
}

static bool lsp_result(LspServer *server, const MarimayLspJson *json,
    size_t id, const char *result) {
    LspOutput buffer;
    bool ok = lsp_output_init(&buffer, server->workspace.allocator);
    MarimayWriter writer = lsp_output_writer(&buffer);
    if (ok) ok = lsp_response_prefix(writer, json, id)
        && mmay_writer_write_cstr(writer, ",\"result\":")
        && mmay_writer_write_cstr(writer, result)
        && mmay_writer_write_cstr(writer, "}")
        && mmay_lsp_write_message(server->output, buffer.bytes.data, buffer.bytes.length);
    lsp_output_dispose(&buffer);
    return ok;
}

static bool lsp_sync(LspServer *server, const MarimayLspJson *json,
    size_t method, size_t params) {
    MarimayOwnedBytes uri = {0}, text = {0};
    size_t document = mmay_lsp_json_get(json, params, "textDocument");
    bool open = mmay_lsp_json_equal(json, method, "textDocument/didOpen");
    bool change = mmay_lsp_json_equal(json, method, "textDocument/didChange");
    bool close = mmay_lsp_json_equal(json, method, "textDocument/didClose");
    LspStatus status = LSP_INVALID;
    int64_t version = 0;
    bool ok = true;
    if (!open && !change && !close
        && !mmay_lsp_json_equal(json, method, "textDocument/didSave")) return true;
    {
        size_t value = mmay_lsp_json_get(json, document, "uri");
        const MarimayLspJsonToken *token = mmay_lsp_json_token(json, value);
        if (token == NULL || token->kind != MMAY_LSP_JSON_STRING) goto cleanup;
        if (!mmay_lsp_json_string(json, value, &uri)) { ok = false; goto cleanup; }
    }
    if (close) {
        status = lsp_workspace_close(&server->workspace,
            (const char *)uri.data, uri.length);
        if (status == LSP_OK) ok = lsp_clear_diagnostics(server->workspace.allocator,
            uri.data, uri.length, server->output)
            && lsp_publish_diagnostics(&server->workspace, server->output);
        goto cleanup;
    }
    if (!open && !change) {
        lsp_workspace_invalidate(&server->workspace);
        ok = lsp_publish_diagnostics(&server->workspace, server->output);
        goto cleanup;
    }
    if (!mmay_lsp_json_integer(json, mmay_lsp_json_get(json, document, "version"), &version)) goto cleanup;
    if (open) {
        size_t value = mmay_lsp_json_get(json, document, "text");
        const MarimayLspJsonToken *token = mmay_lsp_json_token(json, value);
        if (token == NULL || token->kind != MMAY_LSP_JSON_STRING) goto cleanup;
        if (!mmay_lsp_json_string(json, value, &text)) { ok = false; goto cleanup; }
        status = lsp_workspace_open(&server->workspace, (const char *)uri.data,
            uri.length, version, text.data, text.length);
    } else {
        size_t changes = mmay_lsp_json_get(json, params, "contentChanges");
        size_t index = 0, last = SIZE_MAX;
        const MarimayLspJsonToken *array = mmay_lsp_json_token(json, changes);
        if (array == NULL || array->kind != MMAY_LSP_JSON_ARRAY) goto cleanup;
        for (index = 0; ; index += 1) {
            size_t item = mmay_lsp_json_at(json, changes, index);
            if (item == SIZE_MAX) break;
            if (mmay_lsp_json_get(json, item, "range") != SIZE_MAX) goto cleanup;
            last = mmay_lsp_json_get(json, item, "text");
            if (last == SIZE_MAX) goto cleanup;
        }
        {
            const MarimayLspJsonToken *token = mmay_lsp_json_token(json, last);
            if (token == NULL || token->kind != MMAY_LSP_JSON_STRING) goto cleanup;
            if (!mmay_lsp_json_string(json, last, &text)) { ok = false; goto cleanup; }
        }
        status = lsp_workspace_change(&server->workspace, (const char *)uri.data,
            uri.length, version, text.data, text.length);
    }
    if (status == LSP_OK) ok = lsp_publish_diagnostics(&server->workspace, server->output);
    else if (status == LSP_ALLOCATION_FAILED) ok = false;
cleanup:
    mmay_owned_bytes_dispose(&uri);
    mmay_owned_bytes_dispose(&text);
    return ok;
}

static bool lsp_request(LspServer *server, const MarimayLspJson *json,
    size_t method, size_t id, size_t params) {
    LspOutput buffer;
    MarimayWriter writer;
    LspQueryStatus status;
    bool ok;
    if (mmay_lsp_json_equal(json, method, "initialize")) {
        const MarimayLspJsonToken *parameters = mmay_lsp_json_token(json, params);
        if (parameters == NULL || parameters->kind != MMAY_LSP_JSON_OBJECT)
            return lsp_error(server, json, id, -32602, "Invalid initialize params");
        if (server->initialized) return lsp_error(server, json, id, -32600,
            "Server already initialized");
        size_t capabilities = mmay_lsp_json_get(json, params, "capabilities");
        size_t workspace = mmay_lsp_json_get(json, capabilities, "workspace");
        size_t watched = mmay_lsp_json_get(json, workspace, "didChangeWatchedFiles");
        const MarimayLspJsonToken *dynamic = mmay_lsp_json_token(json,
            mmay_lsp_json_get(json, watched, "dynamicRegistration"));
        server->dynamic_watches = dynamic != NULL && dynamic->kind == MMAY_LSP_JSON_TRUE;
        server->initialized = true;
        return lsp_result(server, json, id,
            "{\"capabilities\":{\"positionEncoding\":\"utf-16\","
            "\"textDocumentSync\":{\"openClose\":true,\"change\":1,\"save\":{\"includeText\":false}},"
            "\"definitionProvider\":true,\"hoverProvider\":true,\"referencesProvider\":true,"
            "\"completionProvider\":{\"triggerCharacters\":[\".\"],\"resolveProvider\":false},"
            "\"renameProvider\":{\"prepareProvider\":true},"
            "\"documentSymbolProvider\":true,\"workspaceSymbolProvider\":true,"
            "\"documentFormattingProvider\":true},\"serverInfo\":{\"name\":\"mmay\","
            "\"version\":\"" MMAY_COMPILER_VERSION "\"}}");
    }
    if (!server->initialized) return lsp_error(server, json, id, -32002,
        "Server not initialized");
    if (server->shutdown) return lsp_error(server, json, id, -32600,
        "Server has shut down");
    if (mmay_lsp_json_equal(json, method, "shutdown")) {
        server->shutdown = true;
        return lsp_result(server, json, id, "null");
    }
    if (!lsp_output_init(&buffer, server->workspace.allocator)) return false;
    writer = lsp_output_writer(&buffer);
    ok = lsp_response_prefix(writer, json, id)
        && mmay_writer_write_cstr(writer, ",\"result\":");
    status = ok ? lsp_query(&server->workspace, json, method, params, writer)
        : LSP_QUERY_FAILED;
    if (status == LSP_QUERY_OK) ok = mmay_writer_write_cstr(writer, "}")
        && mmay_lsp_write_message(server->output, buffer.bytes.data, buffer.bytes.length);
    lsp_output_dispose(&buffer);
    if (status == LSP_QUERY_UNKNOWN) return lsp_error(server, json, id, -32601, "Method not found");
    if (status == LSP_QUERY_INVALID) return lsp_error(server, json, id, -32602, "Invalid params");
    if (status == LSP_QUERY_FAILED) return lsp_error(server, json, id, -32603, "Analysis failed");
    if (status == LSP_QUERY_REJECTED) return lsp_error(server, json, id, -32803,
        "Rename would change name resolution or cannot be proven safe");
    return ok;
}

static bool lsp_dispatch(LspServer *server, const MarimayLspJson *json) {
    const MarimayLspJsonToken *root = mmay_lsp_json_token(json, 0);
    size_t id = mmay_lsp_json_get(json, 0, "id");
    size_t method = mmay_lsp_json_get(json, 0, "method");
    size_t params = mmay_lsp_json_get(json, 0, "params");
    const MarimayLspJsonToken *id_token = mmay_lsp_json_token(json, id);
    const MarimayLspJsonToken *method_token = mmay_lsp_json_token(json, method);
    int64_t number = 0;
    bool valid_id = id_token == NULL || id_token->kind == MMAY_LSP_JSON_STRING
        || id_token->kind == MMAY_LSP_JSON_NULL
        || mmay_lsp_json_integer(json, id, &number);
    if (root == NULL || root->kind != MMAY_LSP_JSON_OBJECT || !valid_id
        || !mmay_lsp_json_equal(json, mmay_lsp_json_get(json, 0, "jsonrpc"), "2.0"))
        return lsp_error(server, json, SIZE_MAX, -32600, "Invalid Request");
    if (method_token == NULL) {
        /* Client replies, including watch-registration replies, have no method. */
        bool result = mmay_lsp_json_get(json, 0, "result") != SIZE_MAX;
        bool error = mmay_lsp_json_get(json, 0, "error") != SIZE_MAX;
        return id_token != NULL && result != error ? true
            : lsp_error(server, json, SIZE_MAX, -32600, "Invalid Request");
    }
    if (method_token->kind != MMAY_LSP_JSON_STRING)
        return lsp_error(server, json, SIZE_MAX, -32600, "Invalid Request");
    if (id_token != NULL) return lsp_request(server, json, method, id, params);
    if (mmay_lsp_json_equal(json, method, "exit")) {
        server->exited = true;
        return true;
    }
    if (!server->initialized || server->shutdown) return true;
    if (mmay_lsp_json_equal(json, method, "initialized")) return lsp_register_watches(server);
    if (mmay_lsp_json_equal(json, method, "workspace/didChangeWatchedFiles")
        || mmay_lsp_json_equal(json, method, "workspace/didChangeConfiguration"))
        return lsp_workspace_changed(server, json, method, params);
    return lsp_sync(server, json, method, params);
}

int mmay_lsp_run(MarimaySession *session, MarimayReadStream input,
    MarimayWriter output, MarimayWriter error) {
    LspServer server = {0};
    int result = MMAY_EXIT_SUCCESS;
    FILE *input_file = mmay_read_stream_file(input);
    FILE *output_file = mmay_writer_file(output);
    server.output = output;
    if ((input_file != NULL && !mmay_platform_stdio_binary(input_file))
        || (output_file != NULL && !mmay_platform_stdio_binary(output_file)))
        return MMAY_EXIT_INTERNAL;
    if (!lsp_workspace_init(&server.workspace, session->allocator)) return MMAY_EXIT_INTERNAL;
    while (!server.exited) {
        MarimayOwnedBytes message = {0};
        MarimayLspJson json = {0};
        MarimayLspReadStatus status = mmay_lsp_read_message(input, session->allocator, &message);
        bool ok;
        if (status == MMAY_LSP_READ_END) break;
        if (status != MMAY_LSP_READ_OK) {
            mmay_owned_bytes_dispose(&message);
            (void)mmay_writer_write_cstr(error, "mmay lsp: invalid or unavailable input frame\n");
            result = MMAY_EXIT_INTERNAL;
            break;
        }
        ok = mmay_lsp_json_parse(&json, session->allocator, message.data, message.length)
            ? lsp_dispatch(&server, &json)
            : lsp_error(&server, &json, SIZE_MAX, -32700, "Parse error");
        mmay_lsp_json_dispose(&json);
        mmay_owned_bytes_dispose(&message);
        if (!ok) {
            result = MMAY_EXIT_INTERNAL;
            break;
        }
    }
    if (server.exited && !server.shutdown) result = 1;
    lsp_workspace_dispose(&server.workspace);
    return result;
}
