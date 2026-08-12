#include "ruda/base/path.h"

#include <stdbool.h>
#include <string.h>

#include "ruda/base/utf8.h"
#include "ruda/base/vector.h"

typedef struct RudaPathSegment {
    const char *bytes;
    size_t length;
    bool parent;
} RudaPathSegment;

typedef enum RudaPathPrefix {
    RUDA_PREFIX_NONE,
    RUDA_PREFIX_ROOT,
    RUDA_PREFIX_DRIVE,
    RUDA_PREFIX_UNC
} RudaPathPrefix;

static bool ruda_path_separator(RudaPathFlavor flavor, char character) {
    return character == '/' || (flavor == RUDA_PATH_WINDOWS && character == '\\');
}

static bool ruda_path_alpha(char character) {
    return (character >= 'A' && character <= 'Z')
        || (character >= 'a' && character <= 'z');
}

static char ruda_path_ascii_upper(char character) {
    return character >= 'a' && character <= 'z'
        ? (char)(character - ('a' - 'A'))
        : character;
}

static bool ruda_path_windows_invalid_byte(unsigned char byte) {
    return byte < 0x20U || byte == '<' || byte == '>' || byte == '"'
        || byte == ':' || byte == '|' || byte == '?' || byte == '*';
}

static bool ruda_path_windows_component_valid(const char *bytes, size_t length) {
    size_t index = 0;
    size_t base_length = 0;
    if (length == 0 || bytes[length - 1] == ' ' || bytes[length - 1] == '.') {
        return false;
    }
    for (index = 0; index < length; index += 1) {
        if (ruda_path_windows_invalid_byte((unsigned char)bytes[index])) {
            return false;
        }
        if (bytes[index] == '.' && base_length == 0) {
            base_length = index;
        }
    }
    if (base_length == 0) {
        base_length = length;
    }
    if (base_length == 3) {
        char first = ruda_path_ascii_upper(bytes[0]);
        char second = ruda_path_ascii_upper(bytes[1]);
        char third = ruda_path_ascii_upper(bytes[2]);
        if ((first == 'C' && second == 'O' && third == 'N')
            || (first == 'P' && second == 'R' && third == 'N')
            || (first == 'A' && second == 'U' && third == 'X')
            || (first == 'N' && second == 'U' && third == 'L')) {
            return false;
        }
    }
    if (base_length == 4) {
        char first = ruda_path_ascii_upper(bytes[0]);
        char second = ruda_path_ascii_upper(bytes[1]);
        char third = ruda_path_ascii_upper(bytes[2]);
        bool device = (first == 'C' && second == 'O' && third == 'M')
            || (first == 'L' && second == 'P' && third == 'T');
        if (device && bytes[3] >= '1' && bytes[3] <= '9') {
            return false;
        }
    }
    return true;
}

static bool ruda_path_segment_equals(
    const char *bytes,
    size_t length,
    const char *literal
) {
    size_t literal_length = strlen(literal);
    return length == literal_length && memcmp(bytes, literal, length) == 0;
}

static bool ruda_path_append(RudaVector *bytes, const char *text, size_t length) {
    return ruda_vector_append(bytes, text, length);
}

static RudaPathStatus ruda_path_collect_segments(
    RudaPathFlavor flavor,
    const char *input,
    size_t input_length,
    size_t cursor,
    bool absolute,
    RudaVector *segments
) {
    while (cursor < input_length) {
        size_t start = 0;
        size_t length = 0;
        RudaPathSegment segment;
        while (cursor < input_length && ruda_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        if (cursor == input_length) {
            break;
        }
        start = cursor;
        while (cursor < input_length && !ruda_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        length = cursor - start;
        if (ruda_path_segment_equals(input + start, length, ".")) {
            continue;
        }
        if (ruda_path_segment_equals(input + start, length, "..")) {
            if (segments->length != 0) {
                RudaPathSegment *last = ruda_vector_at(segments, segments->length - 1);
                if (!last->parent) {
                    segments->length -= 1;
                    continue;
                }
            }
            if (absolute) {
                return RUDA_PATH_ESCAPES_ROOT;
            }
            segment.bytes = input + start;
            segment.length = length;
            segment.parent = true;
        } else {
            if (flavor == RUDA_PATH_WINDOWS
                && !ruda_path_windows_component_valid(input + start, length)) {
                return RUDA_PATH_INVALID_CHARACTER;
            }
            segment.bytes = input + start;
            segment.length = length;
            segment.parent = false;
        }
        if (!ruda_vector_push(segments, &segment)) {
            return RUDA_PATH_ALLOCATION_FAILED;
        }
    }
    return RUDA_PATH_OK;
}

RudaPathStatus ruda_path_normalize(
    RudaPath *output,
    RudaAllocator allocator,
    RudaPathFlavor flavor,
    const char *input,
    size_t input_length
) {
    RudaVector segments;
    RudaVector normalized;
    RudaPathStatus status = RUDA_PATH_OK;
    RudaPathPrefix prefix = RUDA_PREFIX_NONE;
    size_t cursor = 0;
    size_t index = 0;
    bool absolute = false;
    char drive = 0;
    const char *unc_server = NULL;
    size_t unc_server_length = 0;
    const char *unc_share = NULL;
    size_t unc_share_length = 0;

    if (output == NULL || (input == NULL && input_length != 0)
        || (flavor != RUDA_PATH_POSIX && flavor != RUDA_PATH_WINDOWS)) {
        return RUDA_PATH_INVALID_ARGUMENT;
    }
    output->text.allocator = allocator;
    output->text.data = NULL;
    output->text.length = 0;
    output->flavor = flavor;
    output->absolute = false;
    if (!ruda_utf8_validate(input, input_length, NULL)) {
        return RUDA_PATH_INVALID_UTF8;
    }
    if (input_length != 0 && memchr(input, 0, input_length) != NULL) {
        return RUDA_PATH_INVALID_CHARACTER;
    }
    if (!ruda_vector_init(&segments, allocator, sizeof(RudaPathSegment))
        || !ruda_vector_init(&normalized, allocator, sizeof(char))) {
        return RUDA_PATH_INVALID_ARGUMENT;
    }

    if (flavor == RUDA_PATH_WINDOWS && input_length >= 4
        && ruda_path_separator(flavor, input[0]) && ruda_path_separator(flavor, input[1])
        && (input[2] == '?' || input[2] == '.')
        && ruda_path_separator(flavor, input[3])) {
        status = RUDA_PATH_UNSUPPORTED_PREFIX;
        goto cleanup;
    }
    if (flavor == RUDA_PATH_WINDOWS && input_length >= 2
        && ruda_path_separator(flavor, input[0]) && ruda_path_separator(flavor, input[1])) {
        size_t start = 0;
        prefix = RUDA_PREFIX_UNC;
        absolute = true;
        cursor = 2;
        while (cursor < input_length && ruda_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        start = cursor;
        while (cursor < input_length && !ruda_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        unc_server = input + start;
        unc_server_length = cursor - start;
        while (cursor < input_length && ruda_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        start = cursor;
        while (cursor < input_length && !ruda_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        unc_share = input + start;
        unc_share_length = cursor - start;
        if (unc_server_length == 0 || unc_share_length == 0
            || !ruda_path_windows_component_valid(unc_server, unc_server_length)
            || !ruda_path_windows_component_valid(unc_share, unc_share_length)
            || ruda_path_segment_equals(unc_server, unc_server_length, "..")
            || ruda_path_segment_equals(unc_share, unc_share_length, "..")) {
            status = RUDA_PATH_UNSUPPORTED_PREFIX;
            goto cleanup;
        }
    } else if (flavor == RUDA_PATH_WINDOWS && input_length >= 2
        && ruda_path_alpha(input[0]) && input[1] == ':') {
        if (input_length < 3 || !ruda_path_separator(flavor, input[2])) {
            status = RUDA_PATH_UNSUPPORTED_PREFIX;
            goto cleanup;
        }
        prefix = RUDA_PREFIX_DRIVE;
        absolute = true;
        drive = input[0] >= 'a' && input[0] <= 'z'
            ? (char)(input[0] - ('a' - 'A'))
            : input[0];
        cursor = 3;
    } else if (input_length != 0 && ruda_path_separator(flavor, input[0])) {
        prefix = RUDA_PREFIX_ROOT;
        absolute = true;
        cursor = 1;
    }

    status = ruda_path_collect_segments(
        flavor,
        input,
        input_length,
        cursor,
        absolute,
        &segments
    );
    if (status != RUDA_PATH_OK) {
        goto cleanup;
    }

    if (prefix == RUDA_PREFIX_ROOT) {
        if (!ruda_path_append(&normalized, "/", 1)) {
            status = RUDA_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else if (prefix == RUDA_PREFIX_DRIVE) {
        char drive_prefix[3] = {drive, ':', '/'};
        if (!ruda_path_append(&normalized, drive_prefix, sizeof(drive_prefix))) {
            status = RUDA_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else if (prefix == RUDA_PREFIX_UNC) {
        if (!ruda_path_append(&normalized, "//", 2)
            || !ruda_path_append(&normalized, unc_server, unc_server_length)
            || !ruda_path_append(&normalized, "/", 1)
            || !ruda_path_append(&normalized, unc_share, unc_share_length)) {
            status = RUDA_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    }

    for (index = 0; index < segments.length; index += 1) {
        const RudaPathSegment *segment = ruda_vector_at_const(&segments, index);
        bool needs_separator = normalized.length != 0
            && ((const char *)normalized.data)[normalized.length - 1] != '/';
        if (needs_separator && !ruda_path_append(&normalized, "/", 1)) {
            status = RUDA_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (!ruda_path_append(&normalized, segment->bytes, segment->length)) {
            status = RUDA_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (normalized.length == 0 && !ruda_path_append(&normalized, ".", 1)) {
        status = RUDA_PATH_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!ruda_owned_bytes_copy(
            &output->text,
            allocator,
            normalized.data,
            normalized.length
        )) {
        status = RUDA_PATH_ALLOCATION_FAILED;
        goto cleanup;
    }
    output->absolute = absolute;

cleanup:
    ruda_vector_dispose(&normalized);
    ruda_vector_dispose(&segments);
    if (status != RUDA_PATH_OK) {
        ruda_owned_bytes_dispose(&output->text);
    }
    return status;
}

void ruda_path_dispose(RudaPath *path) {
    if (path != NULL) {
        ruda_owned_bytes_dispose(&path->text);
        path->absolute = false;
    }
}
