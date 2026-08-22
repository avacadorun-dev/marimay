#include "mmay/base/path.h"

#include <stdbool.h>
#include <string.h>

#include "mmay/base/utf8.h"
#include "mmay/base/vector.h"

typedef struct MarimayPathSegment {
    const char *bytes;
    size_t length;
    bool parent;
} MarimayPathSegment;

typedef enum MarimayPathPrefix {
    MMAY_PREFIX_NONE,
    MMAY_PREFIX_ROOT,
    MMAY_PREFIX_DRIVE,
    MMAY_PREFIX_UNC
} MarimayPathPrefix;

static bool mmay_path_separator(MarimayPathFlavor flavor, char character) {
    return character == '/' || (flavor == MMAY_PATH_WINDOWS && character == '\\');
}

static bool mmay_path_alpha(char character) {
    return (character >= 'A' && character <= 'Z')
        || (character >= 'a' && character <= 'z');
}

static char mmay_path_ascii_upper(char character) {
    return character >= 'a' && character <= 'z'
        ? (char)(character - ('a' - 'A'))
        : character;
}

static bool mmay_path_windows_invalid_byte(unsigned char byte) {
    return byte < 0x20U || byte == '<' || byte == '>' || byte == '"'
        || byte == ':' || byte == '|' || byte == '?' || byte == '*';
}

static bool mmay_path_windows_component_valid(const char *bytes, size_t length) {
    size_t index = 0;
    size_t base_length = 0;
    if (length == 0 || bytes[length - 1] == ' ' || bytes[length - 1] == '.') {
        return false;
    }
    for (index = 0; index < length; index += 1) {
        if (mmay_path_windows_invalid_byte((unsigned char)bytes[index])) {
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
        char first = mmay_path_ascii_upper(bytes[0]);
        char second = mmay_path_ascii_upper(bytes[1]);
        char third = mmay_path_ascii_upper(bytes[2]);
        if ((first == 'C' && second == 'O' && third == 'N')
            || (first == 'P' && second == 'R' && third == 'N')
            || (first == 'A' && second == 'U' && third == 'X')
            || (first == 'N' && second == 'U' && third == 'L')) {
            return false;
        }
    }
    if (base_length == 4) {
        char first = mmay_path_ascii_upper(bytes[0]);
        char second = mmay_path_ascii_upper(bytes[1]);
        char third = mmay_path_ascii_upper(bytes[2]);
        bool device = (first == 'C' && second == 'O' && third == 'M')
            || (first == 'L' && second == 'P' && third == 'T');
        if (device && bytes[3] >= '1' && bytes[3] <= '9') {
            return false;
        }
    }
    return true;
}

static bool mmay_path_segment_equals(
    const char *bytes,
    size_t length,
    const char *literal
) {
    size_t literal_length = strlen(literal);
    return length == literal_length && memcmp(bytes, literal, length) == 0;
}

static bool mmay_path_append(MarimayVector *bytes, const char *text, size_t length) {
    return mmay_vector_append(bytes, text, length);
}

static MarimayPathStatus mmay_path_collect_segments(
    MarimayPathFlavor flavor,
    const char *input,
    size_t input_length,
    size_t cursor,
    bool absolute,
    MarimayVector *segments
) {
    while (cursor < input_length) {
        size_t start = 0;
        size_t length = 0;
        MarimayPathSegment segment;
        while (cursor < input_length && mmay_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        if (cursor == input_length) {
            break;
        }
        start = cursor;
        while (cursor < input_length && !mmay_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        length = cursor - start;
        if (mmay_path_segment_equals(input + start, length, ".")) {
            continue;
        }
        if (mmay_path_segment_equals(input + start, length, "..")) {
            if (segments->length != 0) {
                MarimayPathSegment *last = mmay_vector_at(segments, segments->length - 1);
                if (!last->parent) {
                    segments->length -= 1;
                    continue;
                }
            }
            if (absolute) {
                return MMAY_PATH_ESCAPES_ROOT;
            }
            segment.bytes = input + start;
            segment.length = length;
            segment.parent = true;
        } else {
            if (flavor == MMAY_PATH_WINDOWS
                && !mmay_path_windows_component_valid(input + start, length)) {
                return MMAY_PATH_INVALID_CHARACTER;
            }
            segment.bytes = input + start;
            segment.length = length;
            segment.parent = false;
        }
        if (!mmay_vector_push(segments, &segment)) {
            return MMAY_PATH_ALLOCATION_FAILED;
        }
    }
    return MMAY_PATH_OK;
}

MarimayPathStatus mmay_path_normalize(
    MarimayPath *output,
    MarimayAllocator allocator,
    MarimayPathFlavor flavor,
    const char *input,
    size_t input_length
) {
    MarimayVector segments;
    MarimayVector normalized;
    MarimayPathStatus status = MMAY_PATH_OK;
    MarimayPathPrefix prefix = MMAY_PREFIX_NONE;
    size_t cursor = 0;
    size_t index = 0;
    bool absolute = false;
    char drive = 0;
    const char *unc_server = NULL;
    size_t unc_server_length = 0;
    const char *unc_share = NULL;
    size_t unc_share_length = 0;

    if (output == NULL || (input == NULL && input_length != 0)
        || (flavor != MMAY_PATH_POSIX && flavor != MMAY_PATH_WINDOWS)) {
        return MMAY_PATH_INVALID_ARGUMENT;
    }
    output->text.allocator = allocator;
    output->text.data = NULL;
    output->text.length = 0;
    output->flavor = flavor;
    output->absolute = false;
    if (!mmay_utf8_validate(input, input_length, NULL)) {
        return MMAY_PATH_INVALID_UTF8;
    }
    if (input_length != 0 && memchr(input, 0, input_length) != NULL) {
        return MMAY_PATH_INVALID_CHARACTER;
    }
    if (!mmay_vector_init(&segments, allocator, sizeof(MarimayPathSegment))
        || !mmay_vector_init(&normalized, allocator, sizeof(char))) {
        return MMAY_PATH_INVALID_ARGUMENT;
    }

    if (flavor == MMAY_PATH_WINDOWS && input_length >= 4
        && mmay_path_separator(flavor, input[0]) && mmay_path_separator(flavor, input[1])
        && (input[2] == '?' || input[2] == '.')
        && mmay_path_separator(flavor, input[3])) {
        status = MMAY_PATH_UNSUPPORTED_PREFIX;
        goto cleanup;
    }
    if (flavor == MMAY_PATH_WINDOWS && input_length >= 2
        && mmay_path_separator(flavor, input[0]) && mmay_path_separator(flavor, input[1])) {
        size_t start = 0;
        prefix = MMAY_PREFIX_UNC;
        absolute = true;
        cursor = 2;
        while (cursor < input_length && mmay_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        start = cursor;
        while (cursor < input_length && !mmay_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        unc_server = input + start;
        unc_server_length = cursor - start;
        while (cursor < input_length && mmay_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        start = cursor;
        while (cursor < input_length && !mmay_path_separator(flavor, input[cursor])) {
            cursor += 1;
        }
        unc_share = input + start;
        unc_share_length = cursor - start;
        if (unc_server_length == 0 || unc_share_length == 0
            || !mmay_path_windows_component_valid(unc_server, unc_server_length)
            || !mmay_path_windows_component_valid(unc_share, unc_share_length)
            || mmay_path_segment_equals(unc_server, unc_server_length, "..")
            || mmay_path_segment_equals(unc_share, unc_share_length, "..")) {
            status = MMAY_PATH_UNSUPPORTED_PREFIX;
            goto cleanup;
        }
    } else if (flavor == MMAY_PATH_WINDOWS && input_length >= 2
        && mmay_path_alpha(input[0]) && input[1] == ':') {
        if (input_length < 3 || !mmay_path_separator(flavor, input[2])) {
            status = MMAY_PATH_UNSUPPORTED_PREFIX;
            goto cleanup;
        }
        prefix = MMAY_PREFIX_DRIVE;
        absolute = true;
        drive = input[0] >= 'a' && input[0] <= 'z'
            ? (char)(input[0] - ('a' - 'A'))
            : input[0];
        cursor = 3;
    } else if (input_length != 0 && mmay_path_separator(flavor, input[0])) {
        prefix = MMAY_PREFIX_ROOT;
        absolute = true;
        cursor = 1;
    }

    status = mmay_path_collect_segments(
        flavor,
        input,
        input_length,
        cursor,
        absolute,
        &segments
    );
    if (status != MMAY_PATH_OK) {
        goto cleanup;
    }

    if (prefix == MMAY_PREFIX_ROOT) {
        if (!mmay_path_append(&normalized, "/", 1)) {
            status = MMAY_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else if (prefix == MMAY_PREFIX_DRIVE) {
        char drive_prefix[3] = {drive, ':', '/'};
        if (!mmay_path_append(&normalized, drive_prefix, sizeof(drive_prefix))) {
            status = MMAY_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    } else if (prefix == MMAY_PREFIX_UNC) {
        if (!mmay_path_append(&normalized, "//", 2)
            || !mmay_path_append(&normalized, unc_server, unc_server_length)
            || !mmay_path_append(&normalized, "/", 1)
            || !mmay_path_append(&normalized, unc_share, unc_share_length)) {
            status = MMAY_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    }

    for (index = 0; index < segments.length; index += 1) {
        const MarimayPathSegment *segment = mmay_vector_at_const(&segments, index);
        bool needs_separator = normalized.length != 0
            && ((const char *)normalized.data)[normalized.length - 1] != '/';
        if (needs_separator && !mmay_path_append(&normalized, "/", 1)) {
            status = MMAY_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (!mmay_path_append(&normalized, segment->bytes, segment->length)) {
            status = MMAY_PATH_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (normalized.length == 0 && !mmay_path_append(&normalized, ".", 1)) {
        status = MMAY_PATH_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!mmay_owned_bytes_copy(
            &output->text,
            allocator,
            normalized.data,
            normalized.length
        )) {
        status = MMAY_PATH_ALLOCATION_FAILED;
        goto cleanup;
    }
    output->absolute = absolute;

cleanup:
    mmay_vector_dispose(&normalized);
    mmay_vector_dispose(&segments);
    if (status != MMAY_PATH_OK) {
        mmay_owned_bytes_dispose(&output->text);
    }
    return status;
}

void mmay_path_dispose(MarimayPath *path) {
    if (path != NULL) {
        mmay_owned_bytes_dispose(&path->text);
        path->absolute = false;
    }
}
