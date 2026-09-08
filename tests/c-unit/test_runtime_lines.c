#include "test_support.h"
#include "mmay/runtime/line_reader.h"
#include "../../runtime/common/fs_handles_api.h"
#include <string.h>

typedef struct LineHost {
    const unsigned char *data;
    size_t length, offset, chunk, reads, closes;
    size_t fail_read;
    bool fail_open, fail_close, final_chunk, zero_progress, oversized;
} LineHost;

static MarimayFileStatus line_open(void *opaque, MarimayAllocator allocator,
    const char *path, size_t length, MarimayFileHandle *handle) {
    LineHost *host = opaque;
    (void)allocator; (void)path; (void)length;
    if (host->fail_open) return MMAY_FILE_IO_FAILED;
    host->offset = 0; *handle = 17; return MMAY_FILE_OK;
}

static MarimayFileStatus line_read(void *opaque, MarimayFileHandle handle,
    unsigned char *bytes, size_t capacity, size_t *count, bool *end) {
    LineHost *host = opaque;
    (void)handle;
    if (host->reads++ == host->fail_read) return MMAY_FILE_IO_FAILED;
    size_t available = host->length - host->offset;
    if (available > capacity) available = capacity;
    if (available > host->chunk) available = host->chunk;
    if (available != 0) memcpy(bytes, host->data + host->offset, available);
    host->offset += available;
    *count = host->oversized ? capacity + 1 : host->zero_progress ? 0 : available;
    *end = !host->zero_progress && (available == 0 || (host->final_chunk && host->offset == host->length));
    return MMAY_FILE_OK;
}

static MarimayFileStatus line_close(void *opaque, MarimayFileHandle handle) {
    LineHost *host = opaque;
    (void)handle; host->closes += 1;
    return host->fail_close ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayIoTransferStatus line_stdin(void *opaque, unsigned char *bytes,
    size_t capacity, size_t *count) {
    bool end = false;
    MarimayFileStatus status = line_read(opaque, 17, bytes, capacity, count, &end);
    if (status != MMAY_FILE_OK) return MMAY_IO_TRANSFER_FAILED;
    return *count == 0 && end ? MMAY_IO_TRANSFER_END : MMAY_IO_TRANSFER_OK;
}

static MarimayFsStatus line_setup(MarimayFsContext *context, LineHost *host,
    MarimayAllocator allocator, int64_t *handle) {
    context->allocator = allocator;
    context->read_ops = (MarimayFileReadOps){host, line_open, line_read, line_close};
    context->max_open_files = 1; context->initialized = true;
    if (!mmay_fs_handles_init(context)) return MMAY_FS_ALLOCATION_FAILED;
    const MarimayOwnedBytes path = {.data = (unsigned char *)"authorized", .length = 10};
    return mmay_fs_handles_open(context, &path, handle);
}

static void line_case(const unsigned char *data, size_t length, size_t limit,
    const unsigned char *expected, size_t expected_length, MarimayFsStatus expected_status,
    size_t bad_offset) {
    for (unsigned engine = 0; engine < 2; engine += 1) {
        bool complete = false;
        for (size_t failure = 0; failure < 256 && !complete; failure += 1) {
            MarimayFailAllocator state;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            MarimayAllocator allocator = mmay_fail_allocator(&state);
            LineHost host = {.data = data, .length = length, .chunk = 4096, .fail_read = SIZE_MAX, .final_chunk = true};
            MarimayFsContext context = {0};
            MarimayOwnedBytes line = {0};
            bool eof = false;
            size_t offset = 0;
            int64_t handle = 0;
            MarimayFsStatus status;
            if (engine == 0) {
                status = line_setup(&context, &host, allocator, &handle);
                if (status == MMAY_FS_OK) status = mmay_fs_read_line(&context, handle, (int64_t)limit, &line, &eof, &offset);
            } else {
                MarimayIoInput input = {true, &host, line_stdin};
                MarimayIoStatus raw = mmay_read_utf8_line(&input, allocator, limit, &line, &eof, &offset);
                status = raw == MMAY_IO_OK ? MMAY_FS_OK : raw == MMAY_IO_LINE_TOO_LONG ? MMAY_FS_LIMIT_EXCEEDED
                    : raw == MMAY_IO_INVALID_UTF8 ? MMAY_FS_INVALID_UTF8 : raw == MMAY_IO_ALLOCATION_FAILED
                    ? MMAY_FS_ALLOCATION_FAILED : MMAY_FS_READ_FAILED;
            }
            complete = status == expected_status;
            MMAY_EXPECT(complete || status == MMAY_FS_ALLOCATION_FAILED);
            if (complete && status == MMAY_FS_OK) {
                MMAY_EXPECT(line.length == expected_length && eof == (length == 0));
                MMAY_EXPECT(expected_length == 0 || memcmp(line.data, expected, expected_length) == 0);
            } else if (status != MMAY_FS_OK) MMAY_EXPECT(line.data == NULL && line.length == 0);
            if (complete && status == MMAY_FS_INVALID_UTF8) MMAY_EXPECT(offset == bad_offset);
            mmay_owned_bytes_dispose(&line);
            mmay_fs_handles_dispose(&context);
            if (engine == 0 && handle != 0) MMAY_EXPECT(host.closes == 1);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(complete);
    }
}

static void line_cursor(void) {
    static const unsigned char data[] = "a\r\nb\nc\r";
    for (size_t chunk = 1; chunk <= 8; chunk += 1) {
        bool complete = false;
        for (size_t failure = 0; failure < 64 && !complete; failure += 1) {
            MarimayFailAllocator state;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            LineHost host = {.data = data, .length = sizeof(data) - 1, .chunk = chunk, .fail_read = SIZE_MAX, .final_chunk = true};
            MarimayFsContext context = {0};
            MarimayOwnedBytes line = {0}, remainder = {0};
            int64_t handle = 0;
            bool eof = false;
            MarimayFsStatus status = line_setup(&context, &host, mmay_fail_allocator(&state), &handle);
            if (status == MMAY_FS_OK) status = mmay_fs_read_line(&context, handle, 1, &line, &eof, NULL);
            if (status == MMAY_FS_OK) {
                MMAY_EXPECT(!eof && line.length == 1 && line.data[0] == 'a');
                mmay_owned_bytes_dispose(&line);
                /* The same handle is the alias: read must retain read-ahead bytes. */
                status = mmay_fs_read(&context, handle, 4, &remainder);
            }
            if (status == MMAY_FS_OK) {
                MMAY_EXPECT(remainder.length == 4 && memcmp(remainder.data, "b\nc\r", 4) == 0);
                status = mmay_fs_read_line(&context, handle, 0, &line, &eof, NULL);
            }
            complete = status == MMAY_FS_OK;
            MMAY_EXPECT(complete || status == MMAY_FS_ALLOCATION_FAILED);
            if (complete) {
                MMAY_EXPECT(eof && line.length == 0);
                MMAY_EXPECT(mmay_fs_close(&context, handle) == MMAY_FS_OK);
                MMAY_EXPECT(mmay_fs_close(&context, handle) == MMAY_FS_OK && host.closes == 1);
                mmay_owned_bytes_dispose(&line);
                MMAY_EXPECT(mmay_fs_read_line(&context, handle, 0, &line, &eof, NULL) == MMAY_FS_CLOSED);
                MMAY_EXPECT(mmay_fs_read_line(&context, handle, -1, &line, &eof, NULL) == MMAY_FS_INVALID_LIMIT);
            }
            mmay_owned_bytes_dispose(&line); mmay_owned_bytes_dispose(&remainder);
            mmay_fs_handles_dispose(&context);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(complete);
    }
}

static void line_host_errors(void) {
    for (unsigned scenario = 0; scenario < 5; scenario += 1) {
        bool complete = false;
        for (size_t failure = 0; failure < 64 && !complete; failure += 1) {
            MarimayFailAllocator state;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            LineHost host = {.data = (const unsigned char *)"a\n", .length = 2, .chunk = 1,
                .fail_read = scenario == 1 ? 1 : SIZE_MAX, .fail_open = scenario == 0,
                .zero_progress = scenario == 2, .oversized = scenario == 3, .fail_close = scenario == 4};
            MarimayFsContext context = {0}; MarimayOwnedBytes line = {0}; int64_t handle = 0; bool eof = false;
            MarimayFsStatus status = line_setup(&context, &host, mmay_fail_allocator(&state), &handle);
            if (status == MMAY_FS_OK) status = mmay_fs_read_line(&context, handle, 8, &line, &eof, NULL);
            MarimayFsStatus expected = scenario == 0 ? MMAY_FS_OPEN_FAILED : scenario == 4 ? MMAY_FS_OK : MMAY_FS_READ_FAILED;
            complete = status == expected;
            MMAY_EXPECT(complete || status == MMAY_FS_ALLOCATION_FAILED);
            if (complete && scenario == 4) MMAY_EXPECT(mmay_fs_close(&context, handle) == MMAY_FS_CLOSE_FAILED);
            mmay_owned_bytes_dispose(&line); mmay_fs_handles_dispose(&context);
            if (complete) MMAY_EXPECT(host.closes == (scenario == 0 ? 0U : 1U));
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(complete);
    }
}

static void line_reuse(void) {
    bool complete = false;
    for (size_t failure = 0; failure < 64 && !complete; failure += 1) {
        MarimayFailAllocator state;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        LineHost host = {.data = (const unsigned char *)"a\nb\n", .length = 4, .chunk = 4096,
            .fail_read = SIZE_MAX, .final_chunk = true};
        MarimayFsContext context = {0}; MarimayOwnedBytes line = {0}; int64_t handle = 0; bool eof = false;
        MarimayFsStatus status = line_setup(&context, &host, mmay_fail_allocator(&state), &handle);
        if (status == MMAY_FS_OK) status = mmay_fs_read_line(&context, handle, 1, &line, &eof, NULL);
        if (status == MMAY_FS_OK) {
            MMAY_EXPECT(line.length == 1 && line.data[0] == 'a');
            MMAY_EXPECT(mmay_fs_close(&context, handle) == MMAY_FS_OK);
            const MarimayOwnedBytes path = {.data = (unsigned char *)"authorized", .length = 10};
            status = mmay_fs_handles_open(&context, &path, &handle);
            mmay_owned_bytes_dispose(&line);
            if (status == MMAY_FS_OK) status = mmay_fs_read_line(&context, handle, 1, &line, &eof, NULL);
        }
        complete = status == MMAY_FS_OK;
        MMAY_EXPECT(complete || status == MMAY_FS_ALLOCATION_FAILED);
        if (complete) MMAY_EXPECT(line.length == 1 && line.data[0] == 'a' && !eof && handle == 1);
        mmay_owned_bytes_dispose(&line); mmay_fs_handles_dispose(&context);
        if (complete) MMAY_EXPECT(host.closes == 2);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(complete);
}

void mmay_test_runtime_lines(void) {
    line_case((const unsigned char *)"", 0, 0, NULL, 0, MMAY_FS_OK, 0);
    line_case((const unsigned char *)"\n", 1, 0, NULL, 0, MMAY_FS_OK, 0);
    line_case((const unsigned char *)"\r\n", 2, 0, NULL, 0, MMAY_FS_OK, 0);
    line_case((const unsigned char *)"a\0b\r\n", 5, 3, (const unsigned char *)"a\0b", 3, MMAY_FS_OK, 0);
    line_case((const unsigned char *)"a\r", 2, 2, (const unsigned char *)"a\r", 2, MMAY_FS_OK, 0);
    line_case((const unsigned char *)"a\r", 2, 1, NULL, 0, MMAY_FS_LIMIT_EXCEEDED, 0);
    line_case((const unsigned char *)"ab\n", 3, 1, NULL, 0, MMAY_FS_LIMIT_EXCEEDED, 0);
    const unsigned char invalid[] = {'a', 0xff, '\n'};
    line_case(invalid, sizeof(invalid), 8, NULL, 0, MMAY_FS_INVALID_UTF8, 1);
    unsigned char boundary[4101];
    memset(boundary, 'x', 4095); memcpy(boundary + 4095, "\xf0\x9f\x8c\xbf\r\n", 6);
    line_case(boundary, sizeof(boundary), 4099, boundary, 4099, MMAY_FS_OK, 0);
    line_cursor(); line_host_errors(); line_reuse();
    static unsigned char maximum[MMAY_FS_MAX_FILE_BYTES + 2];
    memset(maximum, 'x', MMAY_FS_MAX_FILE_BYTES);
    maximum[MMAY_FS_MAX_FILE_BYTES] = '\r'; maximum[MMAY_FS_MAX_FILE_BYTES + 1] = '\n';
    line_case(maximum, sizeof(maximum), MMAY_FS_MAX_FILE_BYTES, maximum, MMAY_FS_MAX_FILE_BYTES, MMAY_FS_OK, 0);
}
