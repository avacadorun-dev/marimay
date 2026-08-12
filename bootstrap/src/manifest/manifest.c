#include "ruda/manifest/manifest.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/path.h"
#include "ruda/base/utf8.h"
#include "ruda/generated/diagnostic_codes.h"

typedef struct RudaManifestParser {
    const RudaSourceSnapshot *snapshot;
    const unsigned char *bytes;
    size_t length;
    RudaManifestParseResult *result;
    RudaManifestParseOptions options;
    RudaManifestDirectiveKind group_kind;
    bool in_group;
    bool allocation_failed;
    bool limit_reached;
} RudaManifestParser;

typedef struct RudaManifestFormatEntry {
    RudaManifestDirectiveKind kind;
    RudaOwnedBytes name;
    RudaOwnedBytes values[2];
    size_t value_count;
    size_t source_order;
} RudaManifestFormatEntry;

typedef struct RudaManifestOutput {
    RudaVector bytes;
    size_t max_bytes;
    bool limit_reached;
    bool allocation_failed;
} RudaManifestOutput;

static bool ruda_manifest_decode_string(
    const RudaManifestParseResult *parsed,
    RudaManifestSpan span,
    RudaAllocator allocator,
    RudaOwnedBytes *output
);
static bool ruda_manifest_first_parent(const RudaOwnedBytes *path);

static bool ruda_manifest_horizontal(unsigned char byte) {
    return byte == ' ' || byte == '\t';
}

static size_t ruda_manifest_trim_left(
    const unsigned char *bytes,
    size_t start,
    size_t end
) {
    while (start < end && ruda_manifest_horizontal(bytes[start])) {
        start += 1;
    }
    return start;
}

static size_t ruda_manifest_trim_right(
    const unsigned char *bytes,
    size_t start,
    size_t end
) {
    while (end > start && ruda_manifest_horizontal(bytes[end - 1])) {
        end -= 1;
    }
    return end;
}

static void ruda_manifest_diagnose(
    RudaManifestParser *parser,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaManifestDiagnostic diagnostic;
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
    if (!ruda_vector_push(&parser->result->diagnostics, &diagnostic)) {
        parser->allocation_failed = true;
    }
}

static bool ruda_manifest_hex(unsigned char byte, uint32_t *value) {
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

static bool ruda_manifest_hex4(
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
        if (!ruda_manifest_hex(bytes[start + index], &digit)) {
            return false;
        }
        result = (result << 4) | digit;
    }
    *value = result;
    return true;
}

static bool ruda_manifest_scan_string(
    RudaManifestParser *parser,
    size_t *cursor,
    size_t end,
    RudaManifestSpan *span
) {
    size_t start = *cursor;
    size_t index = start;
    if (index >= end || parser->bytes[index] != '"') {
        ruda_manifest_diagnose(
            parser,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
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
            ruda_manifest_diagnose(
                parser,
                RUDA_DIAGNOSTIC_MANIFEST_STRING,
                "manifest string contains an unescaped control byte",
                index,
                index + 1
            );
            return false;
        }
        if (byte == '\\') {
            uint32_t first = 0;
            if (index + 1 >= end) {
                ruda_manifest_diagnose(
                    parser,
                    RUDA_DIAGNOSTIC_MANIFEST_STRING,
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
                || !ruda_manifest_hex4(parser->bytes, index + 2, end, &first)) {
                ruda_manifest_diagnose(
                    parser,
                    RUDA_DIAGNOSTIC_MANIFEST_STRING,
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
                    || !ruda_manifest_hex4(
                        parser->bytes,
                        index + 2,
                        end,
                        &second
                    )
                    || second < 0xdc00U || second > 0xdfffU) {
                    ruda_manifest_diagnose(
                        parser,
                        RUDA_DIAGNOSTIC_MANIFEST_STRING,
                        "high surrogate escape requires a following low surrogate",
                        index - 6,
                        index
                    );
                    return false;
                }
                index += 6;
            } else if (first >= 0xdc00U && first <= 0xdfffU) {
                ruda_manifest_diagnose(
                    parser,
                    RUDA_DIAGNOSTIC_MANIFEST_STRING,
                    "low surrogate escape has no preceding high surrogate",
                    index - 6,
                    index
                );
                return false;
            }
            continue;
        }
        if (byte >= 0x80U) {
            RudaUtf8Scalar scalar;
            if (!ruda_utf8_decode(
                    parser->bytes + index,
                    end - index,
                    &scalar
                )) {
                ruda_manifest_diagnose(
                    parser,
                    RUDA_DIAGNOSTIC_MANIFEST_STRING,
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
    ruda_manifest_diagnose(
        parser,
        RUDA_DIAGNOSTIC_MANIFEST_STRING,
        "manifest string is missing its closing quote",
        start,
        end
    );
    return false;
}

static bool ruda_manifest_name_part(
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

static bool ruda_manifest_metadata_name(
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
        && ruda_manifest_name_part(bytes, start, dot)
        && ruda_manifest_name_part(bytes, dot + 1, end);
}

static bool ruda_manifest_name_equals(
    const unsigned char *bytes,
    size_t start,
    size_t end,
    const char *literal
) {
    size_t length = strlen(literal);
    return end - start == length && memcmp(bytes + start, literal, length) == 0;
}

static void ruda_manifest_push_entry(
    RudaManifestParser *parser,
    RudaManifestEntry entry
) {
    if (parser->result->entries.length >= parser->options.max_entries) {
        parser->limit_reached = true;
        return;
    }
    entry.source_order = parser->result->entries.length;
    if (!ruda_vector_push(&parser->result->entries, &entry)) {
        parser->allocation_failed = true;
    }
}

static bool ruda_manifest_parse_values(
    RudaManifestParser *parser,
    size_t cursor,
    size_t end,
    size_t count,
    RudaManifestEntry *entry
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        cursor = ruda_manifest_trim_left(parser->bytes, cursor, end);
        if (!ruda_manifest_scan_string(
                parser,
                &cursor,
                end,
                &entry->values[index]
            )) {
            return false;
        }
    }
    cursor = ruda_manifest_trim_left(parser->bytes, cursor, end);
    if (cursor != end) {
        ruda_manifest_diagnose(
            parser,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
            "unexpected token after manifest directive values",
            cursor,
            end
        );
        return false;
    }
    entry->value_count = count;
    return true;
}

static bool ruda_manifest_validate_workspace_value(
    RudaManifestParser *parser,
    RudaManifestSpan span
) {
    RudaOwnedBytes decoded = {0};
    RudaPath normalized;
    RudaPathStatus path_status;
    size_t index = 0;
    bool valid = true;
    memset(&normalized, 0, sizeof(normalized));
    if (!ruda_manifest_decode_string(
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
        ? ruda_path_normalize(
            &normalized,
            parser->result->allocator,
            RUDA_PATH_POSIX,
            (const char *)decoded.data,
            decoded.length
        )
        : RUDA_PATH_INVALID_CHARACTER;
    if (path_status == RUDA_PATH_ALLOCATION_FAILED) {
        parser->allocation_failed = true;
        valid = false;
    } else if (path_status != RUDA_PATH_OK || normalized.absolute
        || normalized.text.length == 0
        || ruda_manifest_first_parent(&normalized.text)
        || (normalized.text.length > 3
            && normalized.text.data[0] == '.'
            && normalized.text.data[1] == '.'
            && normalized.text.data[2] == '/')) {
        valid = false;
    }
    if (!valid && !parser->allocation_failed) {
        ruda_manifest_diagnose(
            parser,
            RUDA_DIAGNOSTIC_WORKSPACE_PATH,
            "workspace use path must be relative and remain inside the workspace root",
            span.start,
            span.end
        );
    }
    ruda_path_dispose(&normalized);
    ruda_owned_bytes_dispose(&decoded);
    return valid;
}

static void ruda_manifest_parse_group_line(
    RudaManifestParser *parser,
    size_t start,
    size_t end
) {
    RudaManifestEntry entry;
    memset(&entry, 0, sizeof(entry));
    if (end - start == 1 && parser->bytes[start] == ')') {
        parser->in_group = false;
        return;
    }
    entry.kind = parser->group_kind;
    if (ruda_manifest_parse_values(parser, start, end, 2, &entry)) {
        ruda_manifest_push_entry(parser, entry);
    }
}

static void ruda_manifest_parse_top_line(
    RudaManifestParser *parser,
    size_t start,
    size_t end
) {
    RudaManifestEntry entry;
    size_t name_start = start;
    size_t name_end = start;
    size_t cursor = start;
    memset(&entry, 0, sizeof(entry));
    while (name_end < end && !ruda_manifest_horizontal(parser->bytes[name_end])) {
        name_end += 1;
    }
    cursor = ruda_manifest_trim_left(parser->bytes, name_end, end);
    entry.name.start = name_start;
    entry.name.end = name_end;
    if (parser->result->kind == RUDA_MANIFEST_WORKSPACE) {
        if (!ruda_manifest_name_equals(
                parser->bytes,
                name_start,
                name_end,
                "use"
            )) {
            ruda_manifest_diagnose(
                parser,
                RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
                "ruda.work accepts only use directives",
                name_start,
                name_end
            );
            return;
        }
        entry.kind = RUDA_MANIFEST_DIRECTIVE_USE;
        if (ruda_manifest_parse_values(parser, cursor, end, 1, &entry)
            && ruda_manifest_validate_workspace_value(
                parser,
                entry.values[0]
            )) {
            ruda_manifest_push_entry(parser, entry);
        }
        return;
    }
    if (ruda_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "module"
        )) {
        entry.kind = RUDA_MANIFEST_DIRECTIVE_MODULE;
    } else if (ruda_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "language"
        )) {
        entry.kind = RUDA_MANIFEST_DIRECTIVE_LANGUAGE;
    } else if (ruda_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "require"
        ) || ruda_manifest_name_equals(
            parser->bytes,
            name_start,
            name_end,
            "capability"
        )) {
        bool require = parser->bytes[name_start] == 'r';
        if (end - cursor != 1 || parser->bytes[cursor] != '(') {
            ruda_manifest_diagnose(
                parser,
                RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
                "manifest group directive requires '(' alone after its name",
                cursor,
                end
            );
            return;
        }
        if ((require && parser->result->has_require_group)
            || (!require && parser->result->has_capability_group)) {
            ruda_manifest_diagnose(
                parser,
                RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
                "manifest group directive cannot be repeated",
                name_start,
                name_end
            );
            return;
        }
        parser->group_kind = require
            ? RUDA_MANIFEST_DIRECTIVE_REQUIRE
            : RUDA_MANIFEST_DIRECTIVE_CAPABILITY;
        parser->in_group = true;
        if (require) {
            parser->result->has_require_group = true;
        } else {
            parser->result->has_capability_group = true;
        }
        return;
    } else if (ruda_manifest_metadata_name(
            parser->bytes,
            name_start,
            name_end
        )) {
        entry.kind = RUDA_MANIFEST_DIRECTIVE_METADATA;
    } else {
        ruda_manifest_diagnose(
            parser,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
            "unknown manifest directive must use a namespaced name",
            name_start,
            name_end
        );
        return;
    }
    if (ruda_manifest_parse_values(parser, cursor, end, 1, &entry)) {
        ruda_manifest_push_entry(parser, entry);
    }
}

RudaManifestParseOptions ruda_manifest_parse_default_options(void) {
    RudaManifestParseOptions options;
    options.max_entries = 100000;
    options.max_diagnostics = 100;
    return options;
}

RudaManifestParseStatus ruda_manifest_parse(
    const RudaSourceSnapshot *snapshot,
    RudaManifestKind kind,
    RudaAllocator allocator,
    RudaManifestParseOptions options,
    RudaManifestParseResult *result
) {
    RudaManifestParser parser;
    size_t cursor = 0;
    if (result == NULL) {
        return RUDA_MANIFEST_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (snapshot == NULL
        || (kind != RUDA_MANIFEST_MODULE && kind != RUDA_MANIFEST_WORKSPACE)
        || options.max_entries == 0 || options.max_diagnostics == 0
        || !ruda_vector_init(&result->entries, allocator, sizeof(RudaManifestEntry))
        || !ruda_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(RudaManifestDiagnostic)
        )) {
        return RUDA_MANIFEST_PARSE_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->snapshot = snapshot;
    result->kind = kind;
    result->initialized = true;
    memset(&parser, 0, sizeof(parser));
    parser.snapshot = snapshot;
    parser.bytes = ruda_source_bytes(snapshot);
    parser.length = ruda_source_length(snapshot);
    parser.result = result;
    parser.options = options;
    if (ruda_source_has_bom(snapshot)) {
        ruda_manifest_diagnose(
            &parser,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
            "manifest files do not accept a byte-order mark",
            0,
            3
        );
        if (parser.allocation_failed) {
            ruda_manifest_parse_result_dispose(result);
            return RUDA_MANIFEST_PARSE_ALLOCATION_FAILED;
        }
        return RUDA_MANIFEST_PARSE_OK;
    }
    if (!ruda_source_is_valid_utf8(snapshot)) {
        size_t invalid = ruda_source_invalid_utf8_offset(snapshot);
        ruda_manifest_diagnose(
            &parser,
            RUDA_DIAGNOSTIC_INVALID_UTF8,
            "manifest contains malformed UTF-8",
            invalid,
            invalid < parser.length ? invalid + 1 : invalid
        );
        if (parser.allocation_failed) {
            ruda_manifest_parse_result_dispose(result);
            return RUDA_MANIFEST_PARSE_ALLOCATION_FAILED;
        }
        return RUDA_MANIFEST_PARSE_OK;
    }
    while (cursor < parser.length && !parser.allocation_failed
        && !parser.limit_reached) {
        size_t line_start = cursor;
        size_t line_end = cursor;
        size_t trimmed_start = 0;
        size_t trimmed_end = 0;
        while (line_end < parser.length && parser.bytes[line_end] != '\r'
            && parser.bytes[line_end] != '\n') {
            line_end += 1;
        }
        cursor = line_end;
        if (cursor < parser.length && parser.bytes[cursor] == '\r'
            && cursor + 1 < parser.length && parser.bytes[cursor + 1] == '\n') {
            cursor += 2;
        } else if (cursor < parser.length) {
            cursor += 1;
        }
        trimmed_start = ruda_manifest_trim_left(
            parser.bytes,
            line_start,
            line_end
        );
        trimmed_end = ruda_manifest_trim_right(
            parser.bytes,
            trimmed_start,
            line_end
        );
        if (trimmed_start == trimmed_end) {
            continue;
        }
        if (parser.in_group) {
            ruda_manifest_parse_group_line(&parser, trimmed_start, trimmed_end);
        } else {
            ruda_manifest_parse_top_line(&parser, trimmed_start, trimmed_end);
        }
    }
    if (parser.in_group && !parser.allocation_failed && !parser.limit_reached) {
        ruda_manifest_diagnose(
            &parser,
            RUDA_DIAGNOSTIC_MANIFEST_SYNTAX,
            "manifest group is missing its closing ')'",
            parser.length,
            parser.length
        );
    }
    if (parser.allocation_failed) {
        ruda_manifest_parse_result_dispose(result);
        return RUDA_MANIFEST_PARSE_ALLOCATION_FAILED;
    }
    return parser.limit_reached
        ? RUDA_MANIFEST_PARSE_LIMIT_REACHED
        : RUDA_MANIFEST_PARSE_OK;
}

void ruda_manifest_parse_result_dispose(RudaManifestParseResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_vector_dispose(&result->diagnostics);
    ruda_vector_dispose(&result->entries);
    memset(result, 0, sizeof(*result));
}

const RudaManifestEntry *ruda_manifest_entry(
    const RudaManifestParseResult *result,
    size_t index
) {
    return result == NULL || !result->initialized
        ? NULL
        : ruda_vector_at_const(&result->entries, index);
}

const RudaManifestDiagnostic *ruda_manifest_diagnostic(
    const RudaManifestParseResult *result,
    size_t index
) {
    return result == NULL || !result->initialized
        ? NULL
        : ruda_vector_at_const(&result->diagnostics, index);
}

static bool ruda_manifest_decode_string(
    const RudaManifestParseResult *parsed,
    RudaManifestSpan span,
    RudaAllocator allocator,
    RudaOwnedBytes *output
) {
    const unsigned char *bytes = ruda_source_bytes(parsed->snapshot);
    RudaVector decoded;
    size_t cursor = span.start + 1;
    size_t end = span.end - 1;
    bool ok = false;
    if (span.end <= span.start + 1 || span.end > ruda_source_length(parsed->snapshot)
        || bytes[span.start] != '"' || bytes[span.end - 1] != '"'
        || !ruda_vector_init(&decoded, allocator, sizeof(unsigned char))) {
        return false;
    }
    while (cursor < end) {
        unsigned char byte = bytes[cursor];
        if (byte != '\\') {
            RudaUtf8Scalar scalar;
            size_t width = 1;
            if (byte >= 0x80U) {
                if (!ruda_utf8_decode(bytes + cursor, end - cursor, &scalar)) {
                    goto cleanup;
                }
                width = scalar.width;
            }
            if (!ruda_vector_append(&decoded, bytes + cursor, width)) {
                goto cleanup;
            }
            cursor += width;
            continue;
        }
        cursor += 1;
        byte = bytes[cursor];
        if (byte == '"' || byte == '\\' || byte == '/') {
            if (!ruda_vector_push(&decoded, &byte)) {
                goto cleanup;
            }
            cursor += 1;
        } else if (byte == 'b' || byte == 'f' || byte == 'n'
            || byte == 'r' || byte == 't') {
            unsigned char decoded_byte = byte == 'b' ? '\b'
                : (byte == 'f' ? '\f'
                    : (byte == 'n' ? '\n'
                        : (byte == 'r' ? '\r' : '\t')));
            if (!ruda_vector_push(&decoded, &decoded_byte)) {
                goto cleanup;
            }
            cursor += 1;
        } else {
            uint32_t first = 0;
            uint32_t value = 0;
            unsigned char encoded[4];
            size_t encoded_width = 0;
            if (byte != 'u'
                || !ruda_manifest_hex4(bytes, cursor + 1, end, &first)) {
                goto cleanup;
            }
            cursor += 5;
            value = first;
            if (first >= 0xd800U && first <= 0xdbffU) {
                uint32_t second = 0;
                if (cursor + 6 > end || bytes[cursor] != '\\'
                    || bytes[cursor + 1] != 'u'
                    || !ruda_manifest_hex4(bytes, cursor + 2, end, &second)) {
                    goto cleanup;
                }
                value = UINT32_C(0x10000)
                    + ((first - UINT32_C(0xd800)) << 10)
                    + (second - UINT32_C(0xdc00));
                cursor += 6;
            }
            if (!ruda_utf8_encode(value, encoded, &encoded_width)
                || !ruda_vector_append(&decoded, encoded, encoded_width)) {
                goto cleanup;
            }
        }
    }
    ok = ruda_owned_bytes_copy(output, allocator, decoded.data, decoded.length);

cleanup:
    ruda_vector_dispose(&decoded);
    return ok;
}

static int ruda_manifest_bytes_compare(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    size_t common = left->length < right->length ? left->length : right->length;
    int comparison = common == 0 ? 0 : memcmp(left->data, right->data, common);
    if (comparison != 0) {
        return comparison;
    }
    return left->length < right->length
        ? -1
        : (left->length > right->length ? 1 : 0);
}

static int ruda_manifest_format_entry_compare(const void *left, const void *right) {
    const RudaManifestFormatEntry *first = left;
    const RudaManifestFormatEntry *second = right;
    int comparison = 0;
    if (first->kind != second->kind) {
        return first->kind < second->kind ? -1 : 1;
    }
    if (first->kind == RUDA_MANIFEST_DIRECTIVE_MODULE
        || first->kind == RUDA_MANIFEST_DIRECTIVE_LANGUAGE) {
        return first->source_order < second->source_order
            ? -1
            : (first->source_order > second->source_order ? 1 : 0);
    }
    if (first->kind == RUDA_MANIFEST_DIRECTIVE_METADATA) {
        comparison = ruda_manifest_bytes_compare(&first->name, &second->name);
        if (comparison != 0) {
            return comparison;
        }
    }
    comparison = ruda_manifest_bytes_compare(&first->values[0], &second->values[0]);
    if (comparison != 0 || first->value_count == 1 || second->value_count == 1) {
        return comparison;
    }
    return ruda_manifest_bytes_compare(&first->values[1], &second->values[1]);
}

static bool ruda_manifest_output_append(
    RudaManifestOutput *output,
    const void *bytes,
    size_t length
) {
    if (length > output->max_bytes - output->bytes.length) {
        output->limit_reached = true;
        return false;
    }
    if (!ruda_vector_append(&output->bytes, bytes, length)) {
        output->allocation_failed = true;
        return false;
    }
    return true;
}

static bool ruda_manifest_output_cstr(
    RudaManifestOutput *output,
    const char *text
) {
    return ruda_manifest_output_append(output, text, strlen(text));
}

static bool ruda_manifest_output_json_string(
    RudaManifestOutput *output,
    const RudaOwnedBytes *value
) {
    static const char hex[] = "0123456789abcdef";
    size_t cursor = 0;
    if (!ruda_manifest_output_cstr(output, "\"")) {
        return false;
    }
    while (cursor < value->length) {
        unsigned char byte = value->data[cursor];
        const char *escape = NULL;
        if (byte == '"') {
            escape = "\\\"";
        } else if (byte == '\\') {
            escape = "\\\\";
        } else if (byte == '\b') {
            escape = "\\b";
        } else if (byte == '\f') {
            escape = "\\f";
        } else if (byte == '\n') {
            escape = "\\n";
        } else if (byte == '\r') {
            escape = "\\r";
        } else if (byte == '\t') {
            escape = "\\t";
        }
        if (escape != NULL) {
            if (!ruda_manifest_output_cstr(output, escape)) {
                return false;
            }
            cursor += 1;
        } else if (byte < 0x20U) {
            char encoded[6] = {'\\', 'u', '0', '0', '0', '0'};
            encoded[4] = hex[byte >> 4];
            encoded[5] = hex[byte & 0x0fU];
            if (!ruda_manifest_output_append(output, encoded, sizeof(encoded))) {
                return false;
            }
            cursor += 1;
        } else {
            RudaUtf8Scalar scalar;
            size_t width = 1;
            if (byte >= 0x80U) {
                if (!ruda_utf8_decode(
                        value->data + cursor,
                        value->length - cursor,
                        &scalar
                    )) {
                    return false;
                }
                width = scalar.width;
            }
            if (!ruda_manifest_output_append(
                    output,
                    value->data + cursor,
                    width
                )) {
                return false;
            }
            cursor += width;
        }
    }
    return ruda_manifest_output_cstr(output, "\"");
}

static void ruda_manifest_format_entry_dispose(RudaManifestFormatEntry *entry) {
    size_t index = 0;
    ruda_owned_bytes_dispose(&entry->name);
    for (index = 0; index < entry->value_count; index += 1) {
        ruda_owned_bytes_dispose(&entry->values[index]);
    }
}

static bool ruda_manifest_first_parent(const RudaOwnedBytes *path) {
    return path->length == 2 && path->data[0] == '.' && path->data[1] == '.';
}

static RudaManifestFormatStatus ruda_manifest_collect_format_entries(
    const RudaManifestParseResult *parsed,
    RudaAllocator allocator,
    RudaManifestFormatOptions options,
    RudaVector *entries
) {
    size_t index = 0;
    size_t decoded_total = 0;
    for (index = 0; index < parsed->entries.length; index += 1) {
        const RudaManifestEntry *source = ruda_manifest_entry(parsed, index);
        RudaManifestFormatEntry entry;
        size_t value_index = 0;
        memset(&entry, 0, sizeof(entry));
        if (source == NULL) {
            return RUDA_MANIFEST_FORMAT_INVALID_ARGUMENT;
        }
        entry.kind = source->kind;
        entry.value_count = source->value_count;
        entry.source_order = source->source_order;
        if (entry.kind == RUDA_MANIFEST_DIRECTIVE_METADATA
            && !ruda_owned_bytes_copy(
                &entry.name,
                allocator,
                ruda_source_bytes(parsed->snapshot) + source->name.start,
                source->name.end - source->name.start
            )) {
            ruda_manifest_format_entry_dispose(&entry);
            return RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED;
        }
        for (value_index = 0; value_index < entry.value_count; value_index += 1) {
            if (!ruda_manifest_decode_string(
                    parsed,
                    source->values[value_index],
                    allocator,
                    &entry.values[value_index]
                )) {
                ruda_manifest_format_entry_dispose(&entry);
                return RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED;
            }
            if (entry.values[value_index].length
                    > options.max_decoded_bytes - decoded_total) {
                ruda_manifest_format_entry_dispose(&entry);
                return RUDA_MANIFEST_FORMAT_LIMIT_REACHED;
            }
            decoded_total += entry.values[value_index].length;
        }
        if (parsed->kind == RUDA_MANIFEST_WORKSPACE) {
            RudaPath normalized;
            RudaPathStatus path_status;
            size_t path_index = 0;
            memset(&normalized, 0, sizeof(normalized));
            for (path_index = 0; path_index < entry.values[0].length; path_index += 1) {
                if (entry.values[0].data[path_index] == '\\') {
                    ruda_manifest_format_entry_dispose(&entry);
                    return RUDA_MANIFEST_FORMAT_SOURCE_INVALID;
                }
            }
            path_status = ruda_path_normalize(
                &normalized,
                allocator,
                RUDA_PATH_POSIX,
                (const char *)entry.values[0].data,
                entry.values[0].length
            );
            if (path_status != RUDA_PATH_OK || normalized.absolute
                || normalized.text.length == 0
                || ruda_manifest_first_parent(&normalized.text)
                || (normalized.text.length > 3
                    && normalized.text.data[0] == '.'
                    && normalized.text.data[1] == '.'
                    && normalized.text.data[2] == '/')) {
                ruda_path_dispose(&normalized);
                ruda_manifest_format_entry_dispose(&entry);
                return path_status == RUDA_PATH_ALLOCATION_FAILED
                    ? RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED
                    : RUDA_MANIFEST_FORMAT_SOURCE_INVALID;
            }
            ruda_owned_bytes_dispose(&entry.values[0]);
            entry.values[0] = normalized.text;
            normalized.text.data = NULL;
            normalized.text.length = 0;
            ruda_path_dispose(&normalized);
        }
        if (!ruda_vector_push(entries, &entry)) {
            ruda_manifest_format_entry_dispose(&entry);
            return RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED;
        }
    }
    return RUDA_MANIFEST_FORMAT_OK;
}

static bool ruda_manifest_begin_section(
    RudaManifestOutput *output,
    bool *wrote_section
) {
    if (*wrote_section && !ruda_manifest_output_cstr(output, "\n")) {
        return false;
    }
    *wrote_section = true;
    return true;
}

static bool ruda_manifest_write_simple_section(
    RudaManifestOutput *output,
    const RudaVector *entries,
    RudaManifestDirectiveKind kind,
    const char *name,
    bool *wrote_section
) {
    size_t index = 0;
    bool began = false;
    for (index = 0; index < entries->length; index += 1) {
        const RudaManifestFormatEntry *entry = ruda_vector_at_const(entries, index);
        if (entry == NULL || entry->kind != kind) {
            continue;
        }
        if (!began) {
            if (!ruda_manifest_begin_section(output, wrote_section)) {
                return false;
            }
            began = true;
        }
        if (!ruda_manifest_output_cstr(output, name)
            || !ruda_manifest_output_cstr(output, " ")
            || !ruda_manifest_output_json_string(output, &entry->values[0])
            || !ruda_manifest_output_cstr(output, "\n")) {
            return false;
        }
    }
    return true;
}

static bool ruda_manifest_write_group_section(
    RudaManifestOutput *output,
    const RudaVector *entries,
    RudaManifestDirectiveKind kind,
    const char *name,
    bool present,
    bool *wrote_section
) {
    size_t index = 0;
    if (!present) {
        return true;
    }
    if (!ruda_manifest_begin_section(output, wrote_section)
        || !ruda_manifest_output_cstr(output, name)
        || !ruda_manifest_output_cstr(output, " (\n")) {
        return false;
    }
    for (index = 0; index < entries->length; index += 1) {
        const RudaManifestFormatEntry *entry = ruda_vector_at_const(entries, index);
        if (entry == NULL || entry->kind != kind) {
            continue;
        }
        if (!ruda_manifest_output_cstr(output, "  ")
            || !ruda_manifest_output_json_string(output, &entry->values[0])
            || !ruda_manifest_output_cstr(output, " ")
            || !ruda_manifest_output_json_string(output, &entry->values[1])
            || !ruda_manifest_output_cstr(output, "\n")) {
            return false;
        }
    }
    return ruda_manifest_output_cstr(output, ")\n");
}

static bool ruda_manifest_write_metadata_section(
    RudaManifestOutput *output,
    const RudaVector *entries,
    bool *wrote_section
) {
    size_t index = 0;
    bool began = false;
    for (index = 0; index < entries->length; index += 1) {
        const RudaManifestFormatEntry *entry = ruda_vector_at_const(entries, index);
        if (entry == NULL || entry->kind != RUDA_MANIFEST_DIRECTIVE_METADATA) {
            continue;
        }
        if (!began) {
            if (!ruda_manifest_begin_section(output, wrote_section)) {
                return false;
            }
            began = true;
        }
        if (!ruda_manifest_output_append(
                output,
                entry->name.data,
                entry->name.length
            )
            || !ruda_manifest_output_cstr(output, " ")
            || !ruda_manifest_output_json_string(output, &entry->values[0])
            || !ruda_manifest_output_cstr(output, "\n")) {
            return false;
        }
    }
    return true;
}

RudaManifestFormatOptions ruda_manifest_format_default_options(void) {
    RudaManifestFormatOptions options;
    options.max_decoded_bytes = 64U * 1024U * 1024U;
    options.max_output_bytes = 64U * 1024U * 1024U;
    return options;
}

RudaManifestFormatStatus ruda_manifest_format(
    const RudaManifestParseResult *parsed,
    RudaAllocator allocator,
    RudaManifestFormatOptions options,
    RudaManifestFormatResult *result
) {
    RudaVector entries;
    RudaManifestOutput output;
    RudaManifestFormatStatus status = RUDA_MANIFEST_FORMAT_OK;
    bool wrote_section = false;
    size_t index = 0;
    if (result == NULL) {
        return RUDA_MANIFEST_FORMAT_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    memset(&entries, 0, sizeof(entries));
    memset(&output, 0, sizeof(output));
    if (parsed == NULL || !parsed->initialized || parsed->snapshot == NULL
        || options.max_decoded_bytes == 0 || options.max_output_bytes == 0
        || !ruda_vector_init(&entries, allocator, sizeof(RudaManifestFormatEntry))
        || !ruda_vector_init(&output.bytes, allocator, sizeof(unsigned char))) {
        return RUDA_MANIFEST_FORMAT_INVALID_ARGUMENT;
    }
    output.max_bytes = options.max_output_bytes;
    if (parsed->diagnostics.length != 0 || parsed->suppressed_diagnostics != 0) {
        status = RUDA_MANIFEST_FORMAT_SOURCE_INVALID;
        goto cleanup;
    }
    status = ruda_manifest_collect_format_entries(
        parsed,
        allocator,
        options,
        &entries
    );
    if (status != RUDA_MANIFEST_FORMAT_OK) {
        goto cleanup;
    }
    if (entries.length > 1) {
        qsort(
            entries.data,
            entries.length,
            sizeof(RudaManifestFormatEntry),
            ruda_manifest_format_entry_compare
        );
    }
    if (parsed->kind == RUDA_MANIFEST_WORKSPACE) {
        if (!ruda_manifest_write_simple_section(
                &output,
                &entries,
                RUDA_MANIFEST_DIRECTIVE_USE,
                "use",
                &wrote_section
            )) {
            goto output_failure;
        }
    } else {
        if (!ruda_manifest_write_simple_section(
                &output,
                &entries,
                RUDA_MANIFEST_DIRECTIVE_MODULE,
                "module",
                &wrote_section
            )
            || !ruda_manifest_write_simple_section(
                &output,
                &entries,
                RUDA_MANIFEST_DIRECTIVE_LANGUAGE,
                "language",
                &wrote_section
            )
            || !ruda_manifest_write_group_section(
                &output,
                &entries,
                RUDA_MANIFEST_DIRECTIVE_REQUIRE,
                "require",
                parsed->has_require_group,
                &wrote_section
            )
            || !ruda_manifest_write_group_section(
                &output,
                &entries,
                RUDA_MANIFEST_DIRECTIVE_CAPABILITY,
                "capability",
                parsed->has_capability_group,
                &wrote_section
            )
            || !ruda_manifest_write_metadata_section(
                &output,
                &entries,
                &wrote_section
            )) {
            goto output_failure;
        }
    }
    if (!wrote_section && !ruda_manifest_output_cstr(&output, "\n")) {
        goto output_failure;
    }
    if (!ruda_owned_bytes_copy(
            &result->output,
            allocator,
            output.bytes.data,
            output.bytes.length
        )) {
        status = RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED;
        goto cleanup;
    }
    result->changed = output.bytes.length != ruda_source_length(parsed->snapshot)
        || memcmp(
            output.bytes.data,
            ruda_source_bytes(parsed->snapshot),
            output.bytes.length
        ) != 0;
    result->initialized = true;
    goto cleanup;

output_failure:
    status = output.limit_reached
        ? RUDA_MANIFEST_FORMAT_LIMIT_REACHED
        : (output.allocation_failed
            ? RUDA_MANIFEST_FORMAT_ALLOCATION_FAILED
            : RUDA_MANIFEST_FORMAT_INVALID_ARGUMENT);

cleanup:
    for (index = 0; index < entries.length; index += 1) {
        RudaManifestFormatEntry *entry = ruda_vector_at(&entries, index);
        if (entry != NULL) {
            ruda_manifest_format_entry_dispose(entry);
        }
    }
    ruda_vector_dispose(&output.bytes);
    ruda_vector_dispose(&entries);
    return status;
}

void ruda_manifest_format_result_dispose(RudaManifestFormatResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_owned_bytes_dispose(&result->output);
    result->changed = false;
    result->initialized = false;
}
