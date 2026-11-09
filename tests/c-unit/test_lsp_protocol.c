/* LSP protocol owner: malformed wire input, Unicode JSON and allocation teardown. */
#include "test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/src/lsp/json.h"
#include "../../bootstrap/src/lsp/transport.h"
#include "mmay/platform/stdio.h"

typedef struct MarimayLspTestInput {
    const unsigned char *bytes;
    size_t length;
    size_t cursor;
    size_t chunk;
    size_t error_at;
    bool zero_read;
} MarimayLspTestInput;

static MarimayReadStatus mmay_lsp_test_read(void *context,
    unsigned char *destination, size_t capacity, size_t *read_count) {
    MarimayLspTestInput *input = context;
    size_t count = input->length - input->cursor;
    *read_count = 0;
    if (input->cursor >= input->error_at) return MMAY_READ_ERROR;
    if (input->zero_read) return MMAY_READ_OK;
    if (count == 0) return MMAY_READ_END;
    if (count > capacity) count = capacity;
    if (count > input->chunk) count = input->chunk;
    memcpy(destination, input->bytes + input->cursor, count);
    input->cursor += count;
    *read_count = count;
    return MMAY_READ_OK;
}

static MarimayReadStream mmay_lsp_test_stream(MarimayLspTestInput *input,
    const char *text, size_t chunk) {
    MarimayReadStream result;
    memset(input, 0, sizeof(*input));
    input->bytes = (const unsigned char *)text;
    input->length = strlen(text);
    input->chunk = chunk;
    input->error_at = SIZE_MAX;
    result.context = input;
    result.read = mmay_lsp_test_read;
    return result;
}

static void mmay_lsp_test_json_values(void) {
    static const char text[] =
        " {\"jsonrpc\":\"2.0\",\"m\\u0065thod\":\"textDocument/didOpen\","
        "\"array\":[null,true,false,{\"name\":\"a\\u0000b\\n\\uD83D\\uDE80\"}],"
        "\"minimum\":-9223372036854775808,\"maximum\":9223372036854775807,"
        "\"overflow\":9223372036854775808,\"decimal\":1.5,\"exponent\":1e2} \n";
    MarimayLspJson json = {0};
    MarimayOwnedBytes string = {0};
    MarimayTestBuffer buffer;
    size_t array;
    size_t nested;
    int64_t number = 0;
    MMAY_EXPECT(mmay_lsp_json_parse(&json, mmay_system_allocator(), text, strlen(text)));
    MMAY_EXPECT(mmay_lsp_json_equal(&json, mmay_lsp_json_get(&json, 0, "method"),
        "textDocument/didOpen"));
    MMAY_EXPECT(!mmay_lsp_json_equal(&json, mmay_lsp_json_get(&json, 0, "method"), "textDocument"));
    MMAY_EXPECT(mmay_lsp_json_get(&json, 0, "missing") == SIZE_MAX);
    array = mmay_lsp_json_get(&json, 0, "array");
    MMAY_EXPECT(mmay_lsp_json_token(&json, mmay_lsp_json_at(&json, array, 0))->kind
        == MMAY_LSP_JSON_NULL);
    MMAY_EXPECT(mmay_lsp_json_token(&json, mmay_lsp_json_at(&json, array, 1))->kind
        == MMAY_LSP_JSON_TRUE);
    MMAY_EXPECT(mmay_lsp_json_token(&json, mmay_lsp_json_at(&json, array, 2))->kind
        == MMAY_LSP_JSON_FALSE);
    MMAY_EXPECT(mmay_lsp_json_at(&json, array, 4) == SIZE_MAX);
    MMAY_EXPECT(mmay_lsp_json_at(&json, 0, 0) == SIZE_MAX);
    MMAY_EXPECT(mmay_lsp_json_get(&json, array, "name") == SIZE_MAX);
    nested = mmay_lsp_json_at(&json, array, 3);
    MMAY_EXPECT(mmay_lsp_json_string(&json, mmay_lsp_json_get(&json, nested, "name"), &string));
    MMAY_EXPECT(string.length == 8);
    MMAY_EXPECT(memcmp(string.data, "a\0b\n\xf0\x9f\x9a\x80", 8) == 0);
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_lsp_json_write_string(mmay_test_buffer_writer(&buffer), string.data, string.length));
    MMAY_EXPECT(strcmp(buffer.bytes, "\"a\\u0000b\\u000a\xf0\x9f\x9a\x80\"") == 0);
    MMAY_EXPECT(mmay_lsp_json_integer(&json, mmay_lsp_json_get(&json, 0, "minimum"), &number));
    MMAY_EXPECT(number == INT64_MIN);
    MMAY_EXPECT(mmay_lsp_json_integer(&json, mmay_lsp_json_get(&json, 0, "maximum"), &number));
    MMAY_EXPECT(number == INT64_MAX);
    MMAY_EXPECT(!mmay_lsp_json_integer(&json, mmay_lsp_json_get(&json, 0, "overflow"), &number));
    MMAY_EXPECT(!mmay_lsp_json_integer(&json, mmay_lsp_json_get(&json, 0, "decimal"), &number));
    MMAY_EXPECT(!mmay_lsp_json_integer(&json, mmay_lsp_json_get(&json, 0, "exponent"), &number));
    MMAY_EXPECT(!mmay_lsp_json_string(&json, array, &(MarimayOwnedBytes){0}));
    mmay_owned_bytes_dispose(&string);
    mmay_lsp_json_dispose(&json);
    MMAY_EXPECT(mmay_lsp_json_token(&json, 0) == NULL);
}

static void mmay_lsp_test_json_rejections(void) {
    static const char *const invalid[] = {
        "", " ", "{}{}", "[1,]", "{\"a\":1,}", "{\"a\" 1}", "{1:2}",
        "[+1]", "[01]", "[-01]", "[-]", "[.5]", "[1.]", "[1e]", "[1e+]",
        "[nan]", "[truefalse]", "[True]", "\"unterminated", "\"\\x00\"",
        "\"\\u123\"", "\"\\ud800\"", "\"\\udc00\"", "\"\\ud800\\u0041\"",
        "\"\n\"", "\"\xc0\xaf\"", "{\"a\":1,\"\\u0061\":2}", "{\"a\":[]"
    };
    size_t index;
    char depth[163];
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MarimayLspJson json = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(!mmay_lsp_json_parse(&json, mmay_fail_allocator(&state),
            invalid[index], strlen(invalid[index])));
        mmay_lsp_json_dispose(&json);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    memset(depth, '[', 80);
    depth[80] = '0';
    memset(depth + 81, ']', 80);
    depth[161] = 0;
    {
        MarimayLspJson json = {0};
        MMAY_EXPECT(!mmay_lsp_json_parse(&json, mmay_system_allocator(), depth, 161));
        mmay_lsp_json_dispose(&json);
    }
    {
        MarimayLspJson json = {0};
        MarimayOwnedBytes empty = {0};
        MMAY_EXPECT(mmay_lsp_json_parse(&json, mmay_system_allocator(), "\"\"", 2));
        MMAY_EXPECT(mmay_lsp_json_string(&json, 0, &empty));
        MMAY_EXPECT(empty.length == 0 && empty.data != NULL);
        MMAY_EXPECT(mmay_lsp_json_equal(&json, 0, ""));
        mmay_owned_bytes_dispose(&empty);
        mmay_lsp_json_dispose(&json);
    }
}

static void mmay_lsp_test_json_allocations(void) {
    static const char text[] =
        "{\"a\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],"
        "\"b\":{\"text\":\"\\ud83d\\ude80 Unicode \\u0430\"}}";
    size_t failure;
    bool completed = false;
    for (failure = 0; failure < 64; failure += 1) {
        MarimayFailAllocator state;
        MarimayLspJson json = {0};
        MarimayOwnedBytes decoded = {0};
        bool success;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        success = mmay_lsp_json_parse(&json, mmay_fail_allocator(&state), text, strlen(text));
        if (success) success = mmay_lsp_json_string(&json,
            mmay_lsp_json_get(&json, mmay_lsp_json_get(&json, 0, "b"), "text"), &decoded);
        mmay_owned_bytes_dispose(&decoded);
        mmay_lsp_json_dispose(&json);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (success) {
            MMAY_EXPECT(state.attempts <= failure);
            completed = true;
            break;
        }
        MMAY_EXPECT(state.attempts > failure);
    }
    MMAY_EXPECT(completed);
}

static void mmay_lsp_test_frames(void) {
    static const char frames[] = "content-length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\nX-Protocol2: value\r\n\r\n{}"
        "Content-Length: 3\r\n\r\n[1]";
    size_t chunk;
    for (chunk = 1; chunk <= 7; chunk += 1) {
        MarimayLspTestInput input;
        MarimayReadStream stream = mmay_lsp_test_stream(&input, frames, chunk);
        MarimayOwnedBytes body = {0};
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_system_allocator(), &body) == MMAY_LSP_READ_OK);
        MMAY_EXPECT(body.length == 2 && memcmp(body.data, "{}", 2) == 0);
        mmay_owned_bytes_dispose(&body);
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_system_allocator(), &body) == MMAY_LSP_READ_OK);
        MMAY_EXPECT(body.length == 3 && memcmp(body.data, "[1]", 3) == 0);
        mmay_owned_bytes_dispose(&body);
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_system_allocator(), &body) == MMAY_LSP_READ_END);
    }
    {
        MarimayLspTestInput input;
        MarimayReadStream stream = mmay_lsp_test_stream(&input, "Content-Length: 0 \t\r\n\r\n", 1);
        MarimayOwnedBytes body = {0};
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_system_allocator(), &body) == MMAY_LSP_READ_OK);
        MMAY_EXPECT(body.length == 0 && body.data != NULL);
        mmay_owned_bytes_dispose(&body);
    }
}

static void mmay_lsp_test_frame_rejections(void) {
    static const char *const invalid[] = {
        "C", "Content-Length: 2\r\n\r", "Content-Length: 2\n\n{}", "\r\n\r\n",
        "Content-Type: application/json\r\n\r\n{}", "Content-Length: \r\n\r\n",
        "Content-Length: -1\r\n\r\n", "Content-Length: 1x\r\n\r\n",
        "Content-Length: 16777217\r\n\r\n", "Content-Length: 999999999999999999999999999\r\n\r\n",
        "Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}", "Content-Length 2\r\n\r\n{}",
        "Content-Length: 2\r\n\r\n{", "Content-Length: 2\r\nX: \x7f\r\n\r\n{}"
    };
    size_t index;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MarimayLspTestInput input;
        MarimayReadStream stream = mmay_lsp_test_stream(&input, invalid[index], 3);
        MarimayOwnedBytes body = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_fail_allocator(&state), &body) == MMAY_LSP_READ_INVALID);
        mmay_owned_bytes_dispose(&body);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    for (index = 0; index < 3; index += 1) {
        MarimayLspTestInput input;
        MarimayReadStream stream = mmay_lsp_test_stream(&input, "Content-Length: 2\r\n\r\n{}", 1);
        MarimayOwnedBytes body = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        if (index == 0) input.error_at = 0;
        if (index == 1) input.error_at = input.length - 1;
        if (index == 2) input.zero_read = true;
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_fail_allocator(&state), &body) == MMAY_LSP_READ_IO_ERROR);
        mmay_owned_bytes_dispose(&body);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    {
        char header[8194];
        MarimayLspTestInput input;
        MarimayReadStream stream;
        MarimayOwnedBytes body = {0};
        memset(header, 'A', sizeof(header) - 1);
        header[sizeof(header) - 1] = 0;
        stream = mmay_lsp_test_stream(&input, header, 1);
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_system_allocator(), &body) == MMAY_LSP_READ_INVALID);
        mmay_owned_bytes_dispose(&body);
    }
    for (index = 0; index < 2; index += 1) {
        MarimayLspTestInput input;
        MarimayReadStream stream = mmay_lsp_test_stream(&input, "Content-Length: 2\r\n\r\n{}", 1);
        MarimayOwnedBytes body = {0};
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        MMAY_EXPECT(mmay_lsp_read_message(stream, mmay_fail_allocator(&state), &body)
            == (index == 0 ? MMAY_LSP_READ_ALLOCATION_FAILED : MMAY_LSP_READ_OK));
        mmay_owned_bytes_dispose(&body);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

static bool mmay_lsp_test_flush_failure(void *context) {
    (void)context;
    return false;
}

static void mmay_lsp_test_writers(void) {
    MarimayTestBuffer buffer;
    size_t index;
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_lsp_write_message(mmay_test_buffer_writer(&buffer), "{}", 2));
    MMAY_EXPECT(strcmp(buffer.bytes, "Content-Length: 2\r\n\r\n{}") == 0);
    for (index = 0; index < 2; index += 1) {
        mmay_test_buffer_init(&buffer);
        buffer.failure_index = index;
        MMAY_EXPECT(!mmay_lsp_write_message(mmay_test_buffer_writer(&buffer), "{}", 2));
    }
    {
        MarimayWriter writer;
        mmay_test_buffer_init(&buffer);
        writer = mmay_test_buffer_writer(&buffer);
        writer.flush = mmay_lsp_test_flush_failure;
        MMAY_EXPECT(!mmay_lsp_write_message(writer, "{}", 2));
    }
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_lsp_json_write_string(mmay_test_buffer_writer(&buffer), "\"\\\t\b", 4));
    MMAY_EXPECT(strcmp(buffer.bytes, "\"\\\"\\\\\\u0009\\u0008\"") == 0);
    for (index = 0; index < buffer.calls; index += 1) {
        MarimayTestBuffer failing;
        mmay_test_buffer_init(&failing);
        failing.failure_index = index;
        MMAY_EXPECT(!mmay_lsp_json_write_string(mmay_test_buffer_writer(&failing), "\"\\\t\b", 4));
    }
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(!mmay_lsp_json_write_string(mmay_test_buffer_writer(&buffer), "\xed\xa0\x80", 3));
    MMAY_EXPECT(buffer.length == 0);
    MMAY_EXPECT(mmay_lsp_json_write_string(mmay_test_buffer_writer(&buffer), NULL, 0));
    MMAY_EXPECT(strcmp(buffer.bytes, "\"\"") == 0);
}

void mmay_test_lsp_protocol(void) {
    MMAY_EXPECT(!mmay_platform_stdio_binary(NULL));
    {
        FILE *stream = tmpfile();
        MMAY_EXPECT(stream != NULL);
        if (stream != NULL) {
            MMAY_EXPECT(mmay_platform_stdio_binary(stream));
            MMAY_EXPECT(fwrite("\r\n\x1a", 1, 3, stream) == 3);
            MMAY_EXPECT(fflush(stream) == 0);
            rewind(stream);
            MMAY_EXPECT(fgetc(stream) == '\r');
            MMAY_EXPECT(fgetc(stream) == '\n');
            MMAY_EXPECT(fgetc(stream) == 0x1a);
            MMAY_EXPECT(fgetc(stream) == EOF);
            MMAY_EXPECT(fclose(stream) == 0);
        }
    }
    mmay_lsp_test_json_values();
    mmay_lsp_test_json_rejections();
    mmay_lsp_test_json_allocations();
    mmay_lsp_test_frames();
    mmay_lsp_test_frame_rejections();
    mmay_lsp_test_writers();
}
