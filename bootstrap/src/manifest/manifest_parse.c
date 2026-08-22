#include "manifest_internal.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/utf8.h"
#include "mmay/base/path.h"
#include "mmay/generated/diagnostic_codes.h"

static bool mmay_manifest_horizontal(unsigned char byte) {
    return byte == ' ' || byte == '\t';
}

static size_t mmay_manifest_trim_left(
    const unsigned char *bytes,
    size_t start,
    size_t end
) {
    while (start < end && mmay_manifest_horizontal(bytes[start])) {
        start += 1;
    }
    return start;
}

static size_t mmay_manifest_trim_right(
    const unsigned char *bytes,
    size_t start,
    size_t end
) {
    while (end > start && mmay_manifest_horizontal(bytes[end - 1])) {
        end -= 1;
    }
    return end;
}

static void mmay_manifest_diagnose(
    MarimayManifestParser *parser,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    MarimayManifestDiagnostic diagnostic;
    if (parser->result->diagnostics.length >= parser->options.max_diagnostics) {
        parser->result->suppressed_diagnostics += 1;
        return;
    }
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.start = start <= parser->length ? start : parser->length;
    diagnostic.end = end <= parser->length ? end : parser->length;
    if (diagnostic.end < diagnostic.start) {
        diagnostic.end = diagnostic.start;
    }
    if (!mmay_vector_push(&parser->result->diagnostics, &diagnostic)) {
        parser->allocation_failed = true;
    }
}

static bool mmay_manifest_hex(unsigned char byte, uint32_t *value) {
    if (byte >= '0' && byte <= '9') {
        *value = (uint32_t)(byte - '0');
        return true;
    }
    if (byte >= 'a' && byte <= 'f') {
        *value = (uint32_t)(byte - 'a' + 10);
        return true;
    }
    if (byte >= 'A' && byte <= 'F') {
        *value = (uint32_t)(byte - 'A' + 10);
        return true;
    }
    return false;
}

static bool mmay_manifest_hex4(
    const unsigned char *bytes,
    size_t start,
    size_t end,
    uint32_t *value
) {
    size_t index = 0;
    uint32_t result = 0;
    if (end - start < 4) {
        return false;
    }
    for (index = 0; index < 4; index += 1) {
        uint32_t digit = 0;
        if (!mmay_manifest_hex(bytes[start + index], &digit)) {
            return false;
        }
        result = (result << 4) | digit;
    }
    *value = result;
    return true;
}

static bool mmay_manifest_scan_string(
    MarimayManifestParser *parser,
    size_t *cursor,
    size_t end,
    MarimayManifestSpan *span
) {
    size_t start = *cursor;
    size_t index = start;
    if (index >= end || parser->bytes[index] != '"') {
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
            "expected a quoted manifest string",
            index,
            index < end ? index + 1 : index
        );
        return false;
    }
    index += 1;
    while (index < end) {
        unsigned char byte = parser->bytes[index];
        if (byte == '"') {
            span->start = start;
            span->end = index + 1;
            *cursor = index + 1;
            return true;
        }
        if (byte < 0x20U) {
            mmay_manifest_diagnose(
                parser,
                MMAY_DIAGNOSTIC_MANIFEST_STRING,
                "manifest string contains an unescaped control byte",
                index,
                index + 1
            );
            return false;
        }
        if (byte == '\\') {
            uint32_t first = 0;
            if (index + 1 >= end) {
                mmay_manifest_diagnose(
                    parser,
                    MMAY_DIAGNOSTIC_MANIFEST_STRING,
                    "manifest string ends inside an escape",
                    index,
                    end
                );
                return false;
            }
            byte = parser->bytes[index + 1];
            if (byte == '"' || byte == '\\' || byte == '/'
                || byte == 'b' || byte == 'f' || byte == 'n'
                || byte == 'r' || byte == 't') {
                index += 2;
                continue;
            }
            if (byte != 'u'
                || !mmay_manifest_hex4(parser->bytes, index + 2, end, &first)) {
                mmay_manifest_diagnose(
                    parser,
                    MMAY_DIAGNOSTIC_MANIFEST_STRING,
                    "manifest string contains an invalid JSON escape",
                    index,
                    index + 2 < end ? index + 2 : end
                );
                return false;
            }
            index += 6;
            if (first >= 0xd800U && first <= 0xdbffU) {
                uint32_t second = 0;
                if (index + 6 > end || parser->bytes[index] != '\\'
                    || parser->bytes[index + 1] != 'u'
                    || !mmay_manifest_hex4(
                        parser->bytes,
                        index + 2,
                        end,
                        &second
                    )
                    || second < 0xdc00U || second > 0xdfffU) {
                    mmay_manifest_diagnose(
                        parser,
                        MMAY_DIAGNOSTIC_MANIFEST_STRING,
                        "high surrogate escape requires a following low surrogate",
                        index - 6,
                        index
                    );
                    return false;
                }
                index += 6;
            } else if (first >= 0xdc00U && first <= 0xdfffU) {
                mmay_manifest_diagnose(
                    parser,
                    MMAY_DIAGNOSTIC_MANIFEST_STRING,
                    "low surrogate escape has no preceding high surrogate",
                    index - 6,
                    index
                );
                return false;
            }
            continue;
        }
        if (byte >= 0x80U) {
            MarimayUtf8Scalar scalar;
            if (!mmay_utf8_decode(
                    parser->bytes + index,
                    end - index,
                    &scalar
                )) {
                mmay_manifest_diagnose(
                    parser,
                    MMAY_DIAGNOSTIC_MANIFEST_STRING,
                    "manifest string contains malformed UTF-8",
                    index,
                    index + 1
                );
                return false;
            }
            index += scalar.width;
        } else {
            index += 1;
        }
    }
    mmay_manifest_diagnose(
        parser,
        MMAY_DIAGNOSTIC_MANIFEST_STRING,
        "manifest string is missing its closing quote",
        start,
        end
    );
    return false;
}

static bool mmay_manifest_name_part(
    const unsigned char *bytes,
    size_t start,
    size_t end
) {
    size_t index = start;
    if (start == end || bytes[start] < 'a' || bytes[start] > 'z') {
        return false;
    }
    for (index = start + 1; index < end; index += 1) {
        unsigned char byte = bytes[index];
        if (!((byte >= 'a' && byte <= 'z')
                || (byte >= '0' && byte <= '9')
                || byte == '_' || byte == '-')) {
            return false;
        }
    }
    return true;
}

static bool mmay_manifest_metadata_name(
    const unsigned char *bytes,
    size_t start,
    size_t end
) {
    size_t index = start;
    size_t dot = SIZE_MAX;
    for (index = start; index < end; index += 1) {
        if (bytes[index] == '.') {
            if (dot != SIZE_MAX) {
                return false;
            }
            dot = index;
        }
    }
    return dot != SIZE_MAX
        && mmay_manifest_name_part(bytes, start, dot)
        && mmay_manifest_name_part(bytes, dot + 1, end);
}

static bool mmay_manifest_name_equals(
    const unsigned char *bytes,
    size_t start,
    size_t end,
    const char *literal
) {
    size_t length = strlen(literal);
    return end - start == length && memcmp(bytes + start, literal, length) == 0;
}

static void mmay_manifest_push_entry(
    MarimayManifestParser *parser,
    MarimayManifestEntry entry
) {
    if (parser->result->entries.length >= parser->options.max_entries) {
        parser->limit_reached = true;
        return;
    }
    entry.source_order = parser->result->entries.length;
    if (!mmay_vector_push(&parser->result->entries, &entry)) {
        parser->allocation_failed = true;
    }
}

static bool mmay_manifest_parse_values(
    MarimayManifestParser *parser,
    size_t cursor,
    size_t end,
    size_t count,
    MarimayManifestEntry *entry
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        cursor = mmay_manifest_trim_left(parser->bytes, cursor, end);
        if (!mmay_manifest_scan_string(
                parser,
                &cursor,
                end,
                &entry->values[index]
            )) {
            return false;
        }
    }
    cursor = mmay_manifest_trim_left(parser->bytes, cursor, end);
    if (cursor != end) {
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
            "unexpected token after manifest directive values",
            cursor,
            end
        );
        return false;
    }
    entry->value_count = count;
    return true;
}

static bool mmay_manifest_validate_workspace_value(
    MarimayManifestParser *parser,
    MarimayManifestSpan span
) {
    MarimayOwnedBytes decoded = {0};
    MarimayPath normalized;
    MarimayPathStatus path_status;
    size_t index = 0;
    bool valid = true;
    memset(&normalized, 0, sizeof(normalized));
    if (!mmay_manifest_decode_string(
            parser->result,
            span,
            parser->result->allocator,
            &decoded
        )) {
        parser->allocation_failed = true;
        return false;
    }
    for (index = 0; index < decoded.length; index += 1) {
        if (decoded.data[index] == '\\') {
            valid = false;
            break;
        }
    }
    path_status = valid
        ? mmay_path_normalize(
            &normalized,
            parser->result->allocator,
            MMAY_PATH_POSIX,
            (const char *)decoded.data,
            decoded.length
        )
        : MMAY_PATH_INVALID_CHARACTER;
    if (path_status == MMAY_PATH_ALLOCATION_FAILED) {
        parser->allocation_failed = true;
        valid = false;
    } else if (path_status != MMAY_PATH_OK || normalized.absolute
        || normalized.text.length == 0
        || mmay_manifest_first_parent(&normalized.text)
        || (normalized.text.length > 3
            && normalized.text.data[0] == '.'
            && normalized.text.data[1] == '.'
            && normalized.text.data[2] == '/')) {
        valid = false;
    }
    if (!valid && !parser->allocation_failed) {
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_WORKSPACE_PATH,
            "workspace use path must be relative and remain inside the workspace root",
            span.start,
            span.end
        );
    }
    mmay_path_dispose(&normalized);
    mmay_owned_bytes_dispose(&decoded);
    return valid;
}

static void mmay_manifest_parse_group_line(
    MarimayManifestParser *parser,
    size_t start,
    size_t end
) {
    MarimayManifestEntry entry;
    memset(&entry, 0, sizeof(entry));
    if (end - start == 1 && parser->bytes[start] == ')') {
        parser->in_group = false;
        return;
    }
    entry.kind = parser->group_kind;
    if (mmay_manifest_parse_values(parser, start, end, 2, &entry)) {
        mmay_manifest_push_entry(parser, entry);
    }
}

static void mmay_manifest_parse_top_line(
    MarimayManifestParser *parser,
    size_t start,
    size_t end
) {
    MarimayManifestEntry entry;
    size_t name_start = start;
    size_t name_end = start;
    size_t cursor = start;
    memset(&entry, 0, sizeof(entry));
    while (name_end < end && !mmay_manifest_horizontal(parser->bytes[name_end])) {
        name_end += 1;
    }
    cursor = mmay_manifest_trim_left(parser->bytes, name_end, end);
    entry.name.start = name_start;
    entry.name.end = name_end;
    if (parser->result->kind == MMAY_MANIFEST_WORKSPACE) {
        if (!mmay_manifest_name_equals(
                parser->bytes,
                name_start,
                name_end,
                "use"
            )) {
            mmay_manifest_diagnose(
                parser,
                MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
                "mmay.work accepts only use directives",
                name_start,
                name_end
            );
            return;
        }
        entry.kind = MMAY_MANIFEST_DIRECTIVE_USE;
        if (mmay_manifest_parse_values(parser, cursor, end, 1, &entry)
            && mmay_manifest_validate_workspace_value(
                parser,
                entry.values[0]
            )) {
            mmay_manifest_push_entry(parser, entry);
        }
        return;
    }
    if (mmay_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "module"
        )) {
        entry.kind = MMAY_MANIFEST_DIRECTIVE_MODULE;
    } else if (mmay_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "language"
        )) {
        entry.kind = MMAY_MANIFEST_DIRECTIVE_LANGUAGE;
    } else if (mmay_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "require"
        ) || mmay_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "capability"
        )) {
        bool require = parser->bytes[name_start] == 'r';
        if (end - cursor != 1 || parser->bytes[cursor] != '(') {
            mmay_manifest_diagnose(
                parser,
                MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
                "manifest group directive requires '(' alone after its name",
                cursor,
                end
            );
            return;
        }
        if ((require && parser->result->has_require_group)
            || (!require && parser->result->has_capability_group)) {
            mmay_manifest_diagnose(
                parser,
                MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
                "manifest group directive cannot be repeated",
                name_start,
                name_end
            );
            return;
        }
        parser->group_kind = require
            ? MMAY_MANIFEST_DIRECTIVE_REQUIRE
            : MMAY_MANIFEST_DIRECTIVE_CAPABILITY;
        parser->in_group = true;
        if (require) {
            parser->result->has_require_group = true;
        } else {
            parser->result->has_capability_group = true;
        }
        return;
    } else if (mmay_manifest_metadata_name(
            parser->bytes,
            name_start,
            name_end
        )) {
        entry.kind = MMAY_MANIFEST_DIRECTIVE_METADATA;
    } else {
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
            "unknown manifest directive must use a namespaced name",
            name_start,
            name_end
        );
        return;
    }
    if (mmay_manifest_parse_values(parser, cursor, end, 1, &entry)) {
        mmay_manifest_push_entry(parser, entry);
    }
}


MarimayManifestParseStatus mmay_manifest_parse_content(MarimayManifestParser *parser) {
    size_t cursor = 0;
    if (mmay_source_has_bom(parser->snapshot)) {
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
            "manifest files do not accept a byte-order mark",
            0,
            3
        );
        if (parser->allocation_failed) {
            mmay_manifest_parse_result_dispose(parser->result);
            return MMAY_MANIFEST_PARSE_ALLOCATION_FAILED;
        }
        return MMAY_MANIFEST_PARSE_OK;
    }
    if (!mmay_source_is_valid_utf8(parser->snapshot)) {
        size_t invalid = mmay_source_invalid_utf8_offset(parser->snapshot);
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_INVALID_UTF8,
            "manifest contains malformed UTF-8",
            invalid,
            invalid < parser->length ? invalid + 1 : invalid
        );
        if (parser->allocation_failed) {
            mmay_manifest_parse_result_dispose(parser->result);
            return MMAY_MANIFEST_PARSE_ALLOCATION_FAILED;
        }
        return MMAY_MANIFEST_PARSE_OK;
    }
    while (cursor < parser->length && !parser->allocation_failed
        && !parser->limit_reached) {
        size_t line_start = cursor;
        size_t line_end = cursor;
        size_t trimmed_start = 0;
        size_t trimmed_end = 0;
        while (line_end < parser->length && parser->bytes[line_end] != '\r'
            && parser->bytes[line_end] != '\n') {
            line_end += 1;
        }
        cursor = line_end;
        if (cursor < parser->length && parser->bytes[cursor] == '\r'
            && cursor + 1 < parser->length && parser->bytes[cursor + 1] == '\n') {
            cursor += 2;
        } else if (cursor < parser->length) {
            cursor += 1;
        }
        trimmed_start = mmay_manifest_trim_left(
            parser->bytes,
            line_start,
            line_end
        );
        trimmed_end = mmay_manifest_trim_right(
            parser->bytes,
            trimmed_start,
            line_end
        );
        if (trimmed_start == trimmed_end) {
            continue;
        }
        if (parser->in_group) {
            mmay_manifest_parse_group_line(parser, trimmed_start, trimmed_end);
        } else {
            mmay_manifest_parse_top_line(parser, trimmed_start, trimmed_end);
        }
    }
    if (parser->in_group && !parser->allocation_failed && !parser->limit_reached) {
        mmay_manifest_diagnose(
            parser,
            MMAY_DIAGNOSTIC_MANIFEST_SYNTAX,
            "manifest group is missing its closing ')'",
            parser->length,
            parser->length
        );
    }
    if (parser->allocation_failed) {
        mmay_manifest_parse_result_dispose(parser->result);
        return MMAY_MANIFEST_PARSE_ALLOCATION_FAILED;
    }
    return parser->limit_reached
        ? MMAY_MANIFEST_PARSE_LIMIT_REACHED
        : MMAY_MANIFEST_PARSE_OK;
}

bool mmay_manifest_decode_string(
    const MarimayManifestParseResult *parsed,
    MarimayManifestSpan span,
    MarimayAllocator allocator,
    MarimayOwnedBytes *output
) {
    const unsigned char *bytes = mmay_source_bytes(parsed->snapshot);
    MarimayVector decoded;
    size_t cursor = span.start + 1;
    size_t end = span.end - 1;
    bool ok = false;
    if (span.end <= span.start + 1 || span.end > mmay_source_length(parsed->snapshot)
        || bytes[span.start] != '"' || bytes[span.end - 1] != '"'
        || !mmay_vector_init(&decoded, allocator, sizeof(unsigned char))) {
        return false;
    }
    while (cursor < end) {
        unsigned char byte = bytes[cursor];
        if (byte != '\\') {
            MarimayUtf8Scalar scalar;
            size_t width = 1;
            if (byte >= 0x80U) {
                if (!mmay_utf8_decode(bytes + cursor, end - cursor, &scalar)) {
                    goto cleanup;
                }
                width = scalar.width;
            }
            if (!mmay_vector_append(&decoded, bytes + cursor, width)) {
                goto cleanup;
            }
            cursor += width;
            continue;
        }
        cursor += 1;
        byte = bytes[cursor];
        if (byte == '"' || byte == '\\' || byte == '/') {
            if (!mmay_vector_push(&decoded, &byte)) {
                goto cleanup;
            }
            cursor += 1;
        } else if (byte == 'b' || byte == 'f' || byte == 'n'
            || byte == 'r' || byte == 't') {
            unsigned char decoded_byte = byte == 'b' ? '\b'
                : (byte == 'f' ? '\f'
                    : (byte == 'n' ? '\n'
                        : (byte == 'r' ? '\r' : '\t')));
            if (!mmay_vector_push(&decoded, &decoded_byte)) {
                goto cleanup;
            }
            cursor += 1;
        } else {
            uint32_t first = 0;
            uint32_t value = 0;
            unsigned char encoded[4];
            size_t encoded_width = 0;
            if (byte != 'u'
                || !mmay_manifest_hex4(bytes, cursor + 1, end, &first)) {
                goto cleanup;
            }
            cursor += 5;
            value = first;
            if (first >= 0xd800U && first <= 0xdbffU) {
                uint32_t second = 0;
                if (cursor + 6 > end || bytes[cursor] != '\\'
                    || bytes[cursor + 1] != 'u'
                    || !mmay_manifest_hex4(bytes, cursor + 2, end, &second)) {
                    goto cleanup;
                }
                value = UINT32_C(0x10000)
                    + ((first - UINT32_C(0xd800)) << 10)
                    + (second - UINT32_C(0xdc00));
                cursor += 6;
            }
            if (!mmay_utf8_encode(value, encoded, &encoded_width)
                || !mmay_vector_append(&decoded, encoded, encoded_width)) {
                goto cleanup;
            }
        }
    }
    ok = mmay_owned_bytes_copy(output, allocator, decoded.data, decoded.length);

cleanup:
    mmay_vector_dispose(&decoded);
    return ok;
}
bool mmay_manifest_first_parent(const MarimayOwnedBytes *path) {
    return path->length == 2 && path->data[0] == '.' && path->data[1] == '.';
}
