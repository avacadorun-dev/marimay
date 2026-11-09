#include "test_support.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/platform/directory.h"
#include "mmay/session.h"
#include "../../bootstrap/src/lsp/json.h"
#include "../../bootstrap/src/lsp/positions.h"
#include "../../bootstrap/src/lsp/server.h"
#include "../../bootstrap/src/lsp/transport.h"

typedef struct MarimayLspTestInput {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    size_t fragment;
    size_t calls;
    size_t failure_index;
} MarimayLspTestInput;

static const char mmay_lsp_test_initialize[] =
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
static const char mmay_lsp_test_shutdown[] =
    "{\"jsonrpc\":\"2.0\",\"id\":90,\"method\":\"shutdown\"}";
static const char mmay_lsp_test_exit[] =
    "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}";

static MarimayReadStatus mmay_lsp_test_read(void *context,
    unsigned char *destination, size_t capacity, size_t *read_count) {
    MarimayLspTestInput *input = context;
    size_t count = input->length - input->offset;
    *read_count = 0;
    if (input->calls++ == input->failure_index) return MMAY_READ_ERROR;
    if (count == 0) return MMAY_READ_END;
    if (count > capacity) count = capacity;
    if (count > input->fragment) count = input->fragment;
    memcpy(destination, input->bytes + input->offset, count);
    input->offset += count;
    *read_count = count;
    return MMAY_READ_OK;
}

static int mmay_lsp_test_run(MarimayAllocator allocator,
    const MarimayTestBuffer *wire, size_t read_failure, size_t write_failure,
    MarimayTestBuffer *output, MarimayTestBuffer *error) {
    MarimaySession session;
    MarimayLspTestInput input = {
        (const unsigned char *)wire->bytes, wire->length, 0, 3, 0, read_failure
    };
    mmay_session_init(&session, allocator);
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    output->failure_index = write_failure;
    return mmay_lsp_run(&session,
        (MarimayReadStream){&input, mmay_lsp_test_read},
        mmay_test_buffer_writer(output), mmay_test_buffer_writer(error));
}

static void mmay_lsp_test_frame(MarimayTestBuffer *wire, const char *json) {
    MMAY_EXPECT(mmay_lsp_write_message(mmay_test_buffer_writer(wire), json,
        strlen(json)));
}

static void mmay_lsp_test_begin(MarimayTestBuffer *wire) {
    mmay_test_buffer_init(wire);
    mmay_lsp_test_frame(wire, mmay_lsp_test_initialize);
    mmay_lsp_test_frame(wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");
}

static void mmay_lsp_test_end(MarimayTestBuffer *wire) {
    mmay_lsp_test_frame(wire, mmay_lsp_test_shutdown);
    mmay_lsp_test_frame(wire, mmay_lsp_test_exit);
}

static void mmay_lsp_test_sync(MarimayTestBuffer *wire, const char *uri,
    const char *method, int version, const char *source) {
    MarimayTestBuffer body;
    MarimayWriter writer;
    char number[32];
    bool open = strcmp(method, "textDocument/didOpen") == 0;
    mmay_test_buffer_init(&body);
    writer = mmay_test_buffer_writer(&body);
    MMAY_EXPECT(mmay_writer_write_cstr(writer, "{\"jsonrpc\":\"2.0\",\"method\":"));
    MMAY_EXPECT(mmay_lsp_json_write_string(writer, method, strlen(method)));
    MMAY_EXPECT(mmay_writer_write_cstr(writer, ",\"params\":{\"textDocument\":{\"uri\":"));
    MMAY_EXPECT(mmay_lsp_json_write_string(writer, uri, strlen(uri)));
    if (source == NULL) {
        MMAY_EXPECT(mmay_writer_write_cstr(writer, "}}}"));
    } else {
        (void)snprintf(number, sizeof(number), ",\"version\":%d", version);
        MMAY_EXPECT(mmay_writer_write_cstr(writer, number));
        MMAY_EXPECT(mmay_writer_write_cstr(writer, open
            ? ",\"languageId\":\"marimay\",\"text\":"
            : "},\"contentChanges\":[{\"text\":"));
        MMAY_EXPECT(mmay_lsp_json_write_string(writer, source, strlen(source)));
        MMAY_EXPECT(mmay_writer_write_cstr(writer, open ? "}}}" : "}]}}"));
    }
    mmay_lsp_test_frame(wire, body.bytes);
}

static void mmay_lsp_test_query(MarimayTestBuffer *wire, const char *uri,
    const char *method, int identifier, size_t line, size_t character) {
    MarimayTestBuffer body;
    MarimayWriter writer;
    char number[256];
    mmay_test_buffer_init(&body);
    writer = mmay_test_buffer_writer(&body);
    (void)snprintf(number, sizeof(number), "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":",
        identifier);
    MMAY_EXPECT(mmay_writer_write_cstr(writer, number));
    MMAY_EXPECT(mmay_lsp_json_write_string(writer, method, strlen(method)));
    MMAY_EXPECT(mmay_writer_write_cstr(writer, ",\"params\":{\"textDocument\":{\"uri\":"));
    MMAY_EXPECT(mmay_lsp_json_write_string(writer, uri, strlen(uri)));
    (void)snprintf(number, sizeof(number),
        "},\"position\":{\"line\":%zu,\"character\":%zu},"
        "\"context\":{\"includeDeclaration\":true},"
        "\"options\":{\"tabSize\":2,\"insertSpaces\":true}}}", line, character);
    MMAY_EXPECT(mmay_writer_write_cstr(writer, number));
    mmay_lsp_test_frame(wire, body.bytes);
}

static bool mmay_lsp_test_uri(MarimayOwnedBytes *uri) {
    MarimayOwnedBytes current = {0};
    char path[32768];
    int length;
    bool ok = mmay_directory_current(mmay_system_allocator(), sizeof(path), &current)
        == MMAY_FILE_OK;
    if (ok) {
        length = snprintf(path, sizeof(path), "%s/__mmay_lsp_memory_only__/source.my",
            (const char *)current.data);
        ok = length > 0 && (size_t)length < sizeof(path)
            && lsp_path_to_uri(mmay_system_allocator(), path, (size_t)length, uri) == LSP_OK;
    }
    mmay_owned_bytes_dispose(&current);
    return ok;
}

static void mmay_lsp_test_methods(const char *uri) {
    static const char source[] =
        "fn answer():int{return 42;}\n"
        "fn main():int{return answer();}\n";
    MarimayTestBuffer wire, output, error;
    mmay_lsp_test_begin(&wire);
    mmay_lsp_test_sync(&wire, uri, "textDocument/didOpen", 1, source);
    mmay_lsp_test_query(&wire, uri, "textDocument/definition", 2, 1, 21);
    mmay_lsp_test_query(&wire, uri, "textDocument/hover", 3, 1, 21);
    mmay_lsp_test_query(&wire, uri, "textDocument/references", 4, 1, 21);
    mmay_lsp_test_query(&wire, uri, "textDocument/documentSymbol", 5, 0, 0);
    mmay_lsp_test_query(&wire, uri, "textDocument/formatting", 6, 0, 0);
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"workspace/symbol\","
        "\"params\":{\"query\":\"ans\"}}");
    mmay_lsp_test_end(&wire);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(strstr(output.bytes, "\"positionEncoding\":\"utf-16\"") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"version\":1,\"diagnostics\":[]") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"id\":2,\"result\":{\"uri\":") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"id\":3,\"result\":{\"contents\":") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"id\":4,\"result\":[{\"uri\":") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"name\":\"answer\"") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"id\":7,\"result\":[{\"name\":\"answer\"") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"newText\":\"fn answer(): int {") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"id\":90,\"result\":null") != NULL);
}

static void mmay_lsp_test_errors(const char *uri, MarimayTestBuffer *wire) {
    MarimayTestBuffer output, error;
    mmay_test_buffer_init(wire);
    mmay_lsp_test_frame(wire, "{\"jsonrpc\":");
    mmay_lsp_test_frame(wire,
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"textDocument/hover\"}");
    mmay_lsp_test_frame(wire, mmay_lsp_test_initialize);
    mmay_lsp_test_frame(wire,
        "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"unknown/method\"}");
    mmay_lsp_test_frame(wire,
        "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"textDocument/hover\",\"params\":{}}");
    mmay_lsp_test_frame(wire, "{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":false}");
    mmay_lsp_test_sync(wire, uri, "textDocument/didOpen", 1,
        "fn value(): int { return missing; }\n");
    mmay_lsp_test_sync(wire, uri, "textDocument/didChange", 2,
        "fn value(): int { return 1; }\n");
    mmay_lsp_test_sync(wire, uri, "textDocument/didClose", 0, NULL);
    mmay_lsp_test_end(wire);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":-32700") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":-32002") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":-32601") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":-32602") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":-32600") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"code\":\"R0001\"") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"version\":2,\"diagnostics\":[]") != NULL);
}

static bool mmay_lsp_test_flush_failure(void *context) {
    bool *flushed = context;
    *flushed = true;
    return false;
}

static bool mmay_lsp_test_discard(void *context, const char *bytes, size_t length) {
    (void)context;
    (void)bytes;
    (void)length;
    return true;
}

static void mmay_lsp_test_stream_failures(const MarimayTestBuffer *transcript) {
    MarimayTestBuffer wire = *transcript, output, error;
    size_t calls, index;
    MarimaySession session;
    MarimayFailAllocator state;
    bool flushed = false;
    MarimayLspTestInput input;
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == MMAY_EXIT_SUCCESS);
    calls = output.calls;
    for (index = 0; index < calls; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_lsp_test_run(mmay_fail_allocator(&state), &wire, SIZE_MAX,
            index, &output, &error) != MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, 0,
        SIZE_MAX, &output, &error) != MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length != 0);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, 35,
        SIZE_MAX, &output, &error) != MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length != 0);
    mmay_session_init(&session, mmay_system_allocator());
    input = (MarimayLspTestInput){(const unsigned char *)wire.bytes,
        wire.length, 0, 2, 0, SIZE_MAX};
    MMAY_EXPECT(mmay_lsp_run(&session,
        (MarimayReadStream){&input, mmay_lsp_test_read},
        (MarimayWriter){&flushed, mmay_lsp_test_discard, mmay_lsp_test_flush_failure},
        mmay_test_buffer_writer(&error)) != MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(flushed);
    /* The final truncated exit still leaves a checked open document to release. */
    wire.length -= 1;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_fail_allocator(&state), &wire, SIZE_MAX,
        SIZE_MAX, &output, &error) != MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    mmay_test_buffer_init(&wire);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == MMAY_EXIT_SUCCESS);
    mmay_lsp_test_frame(&wire, mmay_lsp_test_exit);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == 1);
}

static void mmay_lsp_test_allocation_failures(const MarimayTestBuffer *wire) {
    MarimayFailAllocator state;
    MarimayTestBuffer output, error;
    size_t reachable, index;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_fail_allocator(&state), wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == MMAY_EXIT_SUCCESS);
    reachable = state.attempts;
    MMAY_EXPECT(reachable != 0 && reachable != SIZE_MAX);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < reachable; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        (void)mmay_lsp_test_run(mmay_fail_allocator(&state), wire, SIZE_MAX,
            SIZE_MAX, &output, &error);
        MMAY_EXPECT(state.attempts > index);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

static void mmay_lsp_test_watches(void) {
    MarimayTestBuffer wire, output, error;
    mmay_test_buffer_init(&wire);
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
        "\"capabilities\":{\"workspace\":{\"didChangeWatchedFiles\":{\"dynamicRegistration\":true}}}}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"id\":\"mmay-watch-1\",\"result\":null}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{"
        "\"changes\":[{\"uri\":\"file:///watch%20source.my\",\"type\":1}]}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{"
        "\"changes\":[{\"uri\":\"file:///module/.mmay/lsp/stdlib/hash/std/io/io.my\",\"type\":1}]}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{"
        "\"changes\":[{\"uri\":\"file:///bad%XX.my\",\"type\":2}]}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeWatchedFiles\",\"params\":{"
        "\"changes\":[{\"uri\":\"file:///watch.my\",\"type\":99}]}}");
    mmay_lsp_test_frame(&wire,
        "{\"jsonrpc\":\"2.0\",\"method\":\"workspace/didChangeConfiguration\",\"params\":{\"settings\":{}}}");
    mmay_lsp_test_end(&wire);
    MMAY_EXPECT(mmay_lsp_test_run(mmay_system_allocator(), &wire, SIZE_MAX,
        SIZE_MAX, &output, &error) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    const char *registration = strstr(output.bytes, "\"method\":\"client/registerCapability\"");
    MMAY_EXPECT(registration != NULL);
    if (registration != NULL)
        MMAY_EXPECT(strstr(registration + 1, "\"method\":\"client/registerCapability\"") == NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"globPattern\":\"**/*.my\",\"kind\":7") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"id\":90,\"result\":null") != NULL);
    mmay_lsp_test_allocation_failures(&wire);
}

void mmay_test_lsp_server(void) {
    MarimayOwnedBytes uri = {0};
    MarimayTestBuffer valid, error, invalid;
    bool ready = mmay_lsp_test_uri(&uri);
    MMAY_EXPECT(ready);
    if (!ready) return;
    mmay_lsp_test_methods((const char *)uri.data);
    mmay_lsp_test_errors((const char *)uri.data, &error);
    mmay_lsp_test_begin(&valid);
    mmay_lsp_test_sync(&valid, (const char *)uri.data, "textDocument/didOpen", 1,
        "fn value(): int { return 1; }\n");
    mmay_lsp_test_query(&valid, (const char *)uri.data, "textDocument/hover", 2, 0, 3);
    mmay_lsp_test_end(&valid);
    mmay_lsp_test_stream_failures(&valid);
    mmay_lsp_test_begin(&invalid);
    mmay_lsp_test_sync(&invalid, (const char *)uri.data, "textDocument/didOpen", 1,
        "fn value(): int { return missing; }\n");
    mmay_lsp_test_end(&invalid);
    mmay_lsp_test_allocation_failures(&valid);
    mmay_lsp_test_allocation_failures(&invalid);
    mmay_lsp_test_watches();
    mmay_owned_bytes_dispose(&uri);
}
