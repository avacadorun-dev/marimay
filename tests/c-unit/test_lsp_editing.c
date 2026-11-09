#include "test_support.h"

#include <stdio.h>
#include <string.h>
#include "mmay/platform/directory.h"
#include "../../bootstrap/src/lsp/completion.h"
#include "../../bootstrap/src/lsp/positions.h"

static bool editing_uri(MarimayOwnedBytes *uri) {
    MarimayOwnedBytes current = {0};
    char path[32768];
    int count = 0;
    bool ok = mmay_directory_current(mmay_system_allocator(), sizeof(path), &current) == MMAY_FILE_OK;
    if (ok) {
        count = snprintf(path, sizeof(path), "%s/__mmay_lsp_editing_only__/source.my",
            (const char *)current.data);
        ok = count > 0 && (size_t)count < sizeof(path)
            && lsp_path_to_uri(mmay_system_allocator(), path, (size_t)count, uri) == LSP_OK;
    }
    mmay_owned_bytes_dispose(&current);
    return ok;
}

static bool editing_params(const MarimayOwnedBytes *uri, const char *source,
    size_t offset, MarimayTestBuffer *request) {
    size_t line = 0, character = 0;
    MarimayWriter writer;
    char position[128];
    int count;
    mmay_test_buffer_init(request);
    writer = mmay_test_buffer_writer(request);
    if (!lsp_offset_to_position((const unsigned char *)source, strlen(source),
            offset, &line, &character)) return false;
    count = snprintf(position, sizeof(position),
        "},\"position\":{\"line\":%zu,\"character\":%zu}}", line, character);
    return count > 0 && (size_t)count < sizeof(position)
        && mmay_writer_write_cstr(writer, "{\"textDocument\":{\"uri\":")
        && mmay_lsp_json_write_string(writer, uri->data, uri->length)
        && mmay_writer_write(writer, position, (size_t)count);
}

static LspQueryStatus editing_complete(MarimayAllocator allocator,
    const MarimayOwnedBytes *uri, const char *source, size_t offset,
    MarimayTestBuffer *output) {
    LspWorkspace workspace = {0};
    MarimayLspJson json = {0};
    MarimayTestBuffer request;
    LspQueryStatus status = LSP_QUERY_FAILED;
    mmay_test_buffer_init(output);
    if (!editing_params(uri, source, offset, &request)
        || !lsp_workspace_init(&workspace, allocator)
        || lsp_workspace_open(&workspace, (const char *)uri->data, uri->length,
            1, source, strlen(source)) != LSP_OK
        || !mmay_lsp_json_parse(&json, allocator, request.bytes, request.length)) goto cleanup;
    status = lsp_completion(&workspace, &json, 0, mmay_test_buffer_writer(output));
cleanup:
    mmay_lsp_json_dispose(&json);
    lsp_workspace_dispose(&workspace);
    return status;
}

static void editing_expect(const MarimayOwnedBytes *uri, const char *source,
    const char *cursor, const char *present, const char *absent) {
    MarimayTestBuffer output;
    const char *at = strstr(source, cursor);
    MMAY_EXPECT(at != NULL);
    if (at == NULL) return;
    MMAY_EXPECT(editing_complete(mmay_system_allocator(), uri, source,
        (size_t)(at - source) + strlen(cursor), &output) == LSP_QUERY_OK);
    if (present != NULL) MMAY_EXPECT(strstr(output.bytes, present) != NULL);
    if (absent != NULL) MMAY_EXPECT(strstr(output.bytes, absent) == NULL);
}

static void editing_candidates(const MarimayOwnedBytes *uri) {
    editing_expect(uri,
        "fn demo(seed: int): int { const value = seed; return val; }\n",
        "return val", "\"label\":\"value\"", "\"label\":\"seed\"");
    editing_expect(uri,
        "fn demo(seed: int): int { const value = see; const future = 1; return future; }\n",
        "value = see", "\"label\":\"seed\"", "\"label\":\"future\"");
    editing_expect(uri,
        "fn demo(): string { const item = 1; { const item = \"x\"; return it; } }\n",
        "return it", "\"label\":\"item\",\"kind\":6,\"detail\":\"string\"", NULL);
    editing_expect(uri,
        "fn demo(): int { if true { const gone = 1; } return go; }\n",
        "return go", NULL, "\"label\":\"gone\"");
    editing_expect(uri,
        "type User { public name: string, age: int, }\n"
        "type Other { ghost: int, }\n"
        "fn string(user: User): string { return \"user\"; }\n"
        "fn demo(user: User): int { return user.; }\n",
        "return user.", "\"label\":\"age\"", "\"label\":\"ghost\"");
    editing_expect(uri,
        "type Address { public street: string, }\n"
        "type User { public address: Address, }\n"
        "fn demo(user: User): string { return user.address.st; }\n",
        "return user.address.st", "\"label\":\"street\"", "\"label\":\"address\"");
    editing_expect(uri,
        "type User { mutable age: int, }\n"
        "fn demo(user: User): int { user.ag = 3; return user.age; }\n",
        "{ user.ag", "\"label\":\"age\"", NULL);
    editing_expect(uri,
        "type User { name: string, }\n"
        "fn string(user: User): string { return \"user\"; }\n"
        "fn demo(user: User): string { return user.str(); }\n",
        "return user.str", "\"label\":\"string\"", NULL);
    editing_expect(uri,
        "fn demo(): string { return \"value\"; }\n",
        "\"val", "\"items\":[]", "\"label\"");
    editing_expect(uri,
        "fn demo(): int { /* value */ return 1; }\n",
        "/* val", "\"items\":[]", "\"label\"");
    editing_expect(uri,
        "/* 😀 */ fn demo(): int { const значение = 1; return зна; }\n",
        "return зна", "\"label\":\"значение\"", NULL);
}

static void editing_allocation_failures(const MarimayOwnedBytes *uri) {
    static const char source[] = "fn demo(): int { const value = 1; return val; }\n";
    MarimayFailAllocator state;
    MarimayTestBuffer output;
    size_t offset = (size_t)(strstr(source, "return val") - source) + strlen("return val");
    size_t count, index;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(editing_complete(mmay_fail_allocator(&state), uri, source,
        offset, &output) == LSP_QUERY_OK);
    count = state.attempts;
    MMAY_EXPECT(count > 0 && state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < count; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        (void)editing_complete(mmay_fail_allocator(&state), uri, source, offset, &output);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void mmay_test_lsp_editing(void) {
    MarimayOwnedBytes uri = {0};
    bool ready = editing_uri(&uri);
    MMAY_EXPECT(ready);
    if (!ready) return;
    editing_candidates(&uri);
    editing_allocation_failures(&uri);
    mmay_owned_bytes_dispose(&uri);
}
