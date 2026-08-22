#include "mmay/diagnostic.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/unicode.h"
#include "mmay/base/utf8.h"

static const char *mmay_severity_name(MarimaySeverity severity) {
    switch (severity) {
        case MMAY_SEVERITY_ERROR:
            return "error";
        case MMAY_SEVERITY_WARNING:
            return "warning";
        case MMAY_SEVERITY_INFORMATION:
            return "information";
        case MMAY_SEVERITY_HELP:
            return "help";
    }
    return "error";
}

static bool mmay_json_bytes(
    MarimayWriter writer,
    const unsigned char *text,
    size_t length
) {
    const unsigned char *cursor = text;
    size_t remaining = length;
    char escape[7];
    if (!mmay_writer_write_cstr(writer, "\"")) {
        return false;
    }
    while (remaining != 0) {
        unsigned char byte = *cursor;
        if (byte == '"' || byte == '\\') {
            char pair[2] = {'\\', (char)byte};
            if (!mmay_writer_write(writer, pair, sizeof(pair))) {
                return false;
            }
        } else if (byte == '\b') {
            if (!mmay_writer_write_cstr(writer, "\\b")) {
                return false;
            }
        } else if (byte == '\f') {
            if (!mmay_writer_write_cstr(writer, "\\f")) {
                return false;
            }
        } else if (byte == '\n') {
            if (!mmay_writer_write_cstr(writer, "\\n")) {
                return false;
            }
        } else if (byte == '\r') {
            if (!mmay_writer_write_cstr(writer, "\\r")) {
                return false;
            }
        } else if (byte == '\t') {
            if (!mmay_writer_write_cstr(writer, "\\t")) {
                return false;
            }
        } else if (byte < 0x20U) {
            int escaped_length = snprintf(
                escape, sizeof(escape), "\\u%04x", (unsigned int)byte
            );
            if (escaped_length != 6
                || !mmay_writer_write(writer, escape, 6)) {
                return false;
            }
        } else if (byte < 0x80U) {
            if (!mmay_writer_write(writer, (const char *)cursor, 1)) {
                return false;
            }
        } else {
            MarimayUtf8Scalar scalar;
            if (mmay_utf8_decode(cursor, remaining, &scalar)) {
                if (!mmay_writer_write(writer, (const char *)cursor, scalar.width)) {
                    return false;
                }
                cursor += scalar.width;
                remaining -= scalar.width;
                continue;
            }
            if (!mmay_writer_write_cstr(writer, "\\ufffd")) {
                return false;
            }
        }
        cursor += 1;
        remaining -= 1;
    }
    return mmay_writer_write_cstr(writer, "\"");
}

static bool mmay_json_string(MarimayWriter writer, const char *text) {
    return mmay_json_bytes(
        writer, (const unsigned char *)text, strlen(text)
    );
}

static size_t mmay_diagnostic_message_length(
    const MarimayDiagnostic *diagnostic
) {
    return diagnostic->message_is_counted
        ? diagnostic->message_length : strlen(diagnostic->message);
}

static bool mmay_diagnostic_write_message(
    MarimayWriter writer,
    const MarimayDiagnostic *diagnostic
) {
    return mmay_writer_write(
        writer, diagnostic->message,
        mmay_diagnostic_message_length(diagnostic)
    );
}

static bool mmay_write_size(MarimayWriter writer, size_t value) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%zu", value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return false;
    }
    return mmay_writer_write(writer, buffer, (size_t)length);
}

static bool mmay_diagnostic_source_positions(
    const MarimayDiagnostic *diagnostic,
    MarimaySourcePosition *start,
    MarimaySourcePosition *end
) {
    return diagnostic->source != NULL
        && diagnostic->source_start <= diagnostic->source_end
        && diagnostic->source_end <= mmay_source_length(diagnostic->source)
        && mmay_source_position(diagnostic->source, diagnostic->source_start, start)
        && mmay_source_position(diagnostic->source, diagnostic->source_end, end);
}

static bool mmay_diagnostic_utf8_continuation(unsigned char byte) {
    return (byte & 0xc0U) == 0x80U;
}

static bool mmay_diagnostic_repeat(
    MarimayWriter writer,
    char character,
    size_t count
) {
    char buffer[64];
    memset(buffer, character, sizeof(buffer));
    while (count != 0) {
        size_t chunk = count < sizeof(buffer) ? count : sizeof(buffer);
        if (!mmay_writer_write(writer, buffer, chunk)) {
            return false;
        }
        count -= chunk;
    }
    return true;
}

static size_t mmay_diagnostic_scalar_width(
    unsigned char byte,
    const MarimayUtf8Scalar *scalar,
    size_t column
) {
    if (byte == '\t') {
        return 4 - (column % 4);
    }
    if (byte < 0x20U || byte == 0x7fU) {
        return 1;
    }
    if (scalar == NULL) {
        return 1;
    }
    return mmay_unicode_formatter_width(scalar->value);
}

static bool mmay_diagnostic_measure_range(
    const unsigned char *bytes,
    size_t start,
    size_t end,
    size_t initial_column,
    size_t *width
) {
    size_t cursor = start;
    size_t column = initial_column;
    while (cursor < end) {
        MarimayUtf8Scalar scalar;
        size_t scalar_width = 0;
        size_t byte_width = 1;
        bool valid = mmay_utf8_decode(bytes + cursor, end - cursor, &scalar);
        if (valid) {
            byte_width = scalar.width;
        }
        scalar_width = mmay_diagnostic_scalar_width(
            bytes[cursor],
            valid ? &scalar : NULL,
            column
        );
        if (!mmay_size_add(column, scalar_width, &column)) {
            return false;
        }
        cursor += byte_width;
    }
    *width = column - initial_column;
    return true;
}

static bool mmay_diagnostic_write_sanitized_range(
    MarimayWriter writer,
    const unsigned char *bytes,
    size_t start,
    size_t end,
    size_t *column
) {
    static const char replacement[] = "\xef\xbf\xbd";
    size_t cursor = start;
    while (cursor < end) {
        unsigned char byte = bytes[cursor];
        MarimayUtf8Scalar scalar;
        size_t scalar_width = 0;
        size_t byte_width = 1;
        bool valid = mmay_utf8_decode(bytes + cursor, end - cursor, &scalar);
        if (valid) {
            byte_width = scalar.width;
        }
        scalar_width = mmay_diagnostic_scalar_width(
            byte,
            valid ? &scalar : NULL,
            *column
        );
        if (byte == '\t') {
            if (!mmay_diagnostic_repeat(writer, ' ', scalar_width)) {
                return false;
            }
        } else if (byte < 0x20U || byte == 0x7fU) {
            if (!mmay_writer_write_cstr(writer, "?")) {
                return false;
            }
        } else if (valid) {
            if (!mmay_writer_write(
                    writer,
                    (const char *)bytes + cursor,
                    byte_width
                )) {
                return false;
            }
        } else if (!mmay_writer_write(writer, replacement, sizeof(replacement) - 1)) {
            return false;
        }
        if (!mmay_size_add(*column, scalar_width, column)) {
            return false;
        }
        cursor += byte_width;
    }
    return true;
}

static bool mmay_diagnostic_write_source_window(
    MarimayWriter writer,
    const MarimayDiagnostic *diagnostic,
    MarimaySourcePosition start_position,
    MarimaySourcePosition end_position
) {
    const unsigned char *bytes = mmay_source_bytes(diagnostic->source);
    size_t line_start = 0;
    size_t line_end = 0;
    size_t primary_start = diagnostic->source_start;
    size_t primary_end = diagnostic->source_end;
    size_t window_start = 0;
    size_t window_end = 0;
    size_t before_width = 0;
    size_t caret_width = 1;
    size_t column = 0;
    size_t line_digits = 0;
    bool prefix_elided = false;
    bool suffix_elided = false;
    char label[64];
    int label_length = 0;
    if (!mmay_source_line_bounds(
            diagnostic->source,
            start_position.line,
            &line_start,
            &line_end
        )) {
        return false;
    }
    if (primary_start < line_start) {
        primary_start = line_start;
    }
    if (primary_start > line_end) {
        primary_start = line_end;
    }
    if (primary_end > line_end || end_position.line != start_position.line) {
        primary_end = line_end;
    }
    window_start = line_start;
    if (primary_start - line_start > 80) {
        window_start = primary_start - 80;
        while (window_start < primary_start
            && mmay_diagnostic_utf8_continuation(bytes[window_start])) {
            window_start += 1;
        }
        prefix_elided = true;
    }
    window_end = line_end;
    if (window_end - window_start > 200) {
        window_end = window_start + 200;
        while (window_end > window_start && window_end < line_end
            && mmay_diagnostic_utf8_continuation(bytes[window_end])) {
            window_end -= 1;
        }
        suffix_elided = true;
    }
    label_length = snprintf(
        label,
        sizeof(label),
        "%zu | ",
        start_position.line
    );
    if (label_length < 0 || (size_t)label_length >= sizeof(label)) {
        return false;
    }
    line_digits = (size_t)label_length - 3;
    if (!mmay_writer_write(writer, label, (size_t)label_length)) {
        return false;
    }
    if (prefix_elided) {
        if (!mmay_writer_write_cstr(writer, "...")) {
            return false;
        }
        column = 3;
    }
    if (!mmay_diagnostic_write_sanitized_range(
            writer,
            bytes,
            window_start,
            window_end,
            &column
        )
        || (suffix_elided && !mmay_writer_write_cstr(writer, "..."))
        || !mmay_writer_write_cstr(writer, "\n")) {
        return false;
    }
    before_width = prefix_elided ? 3 : 0;
    {
        size_t measured = 0;
        if (!mmay_diagnostic_measure_range(
                bytes,
                window_start,
                primary_start,
                before_width,
                &measured
            ) || !mmay_size_add(before_width, measured, &before_width)) {
            return false;
        }
    }
    if (primary_end > primary_start) {
        if (!mmay_diagnostic_measure_range(
                bytes,
                primary_start,
                primary_end,
                before_width,
                &caret_width
            )) {
            return false;
        }
        if (caret_width == 0) {
            caret_width = 1;
        }
    }
    if (caret_width > 80) {
        caret_width = 80;
    }
    return mmay_diagnostic_repeat(writer, ' ', line_digits)
        && mmay_writer_write_cstr(writer, " | ")
        && mmay_diagnostic_repeat(writer, ' ', before_width)
        && mmay_diagnostic_repeat(writer, '^', caret_width)
        && mmay_writer_write_cstr(writer, "\n");
}

static bool mmay_diagnostic_write_json_position(
    MarimayWriter writer,
    MarimaySourcePosition position
) {
    return mmay_writer_write_cstr(writer, "{\"line\":")
        && mmay_write_size(writer, position.line)
        && mmay_writer_write_cstr(writer, ",\"column\":")
        && mmay_write_size(writer, position.column)
        && mmay_writer_write_cstr(writer, "}");
}

static bool mmay_diagnostic_write_json_primary(
    MarimayWriter writer,
    const MarimayDiagnostic *diagnostic
) {
    MarimaySourcePosition start;
    MarimaySourcePosition end;
    if (diagnostic->source == NULL) {
        return mmay_writer_write_cstr(writer, "null");
    }
    if (!mmay_diagnostic_source_positions(diagnostic, &start, &end)) {
        return false;
    }
    return mmay_writer_write_cstr(writer, "{\"file\":")
        && mmay_json_string(writer, mmay_source_path(diagnostic->source))
        && mmay_writer_write_cstr(writer, ",\"snapshot\":")
        && mmay_json_string(
            writer,
            mmay_source_snapshot_identity(diagnostic->source)
        )
        && mmay_writer_write_cstr(writer, ",\"startByte\":")
        && mmay_write_size(writer, diagnostic->source_start)
        && mmay_writer_write_cstr(writer, ",\"endByte\":")
        && mmay_write_size(writer, diagnostic->source_end)
        && mmay_writer_write_cstr(writer, ",\"start\":")
        && mmay_diagnostic_write_json_position(writer, start)
        && mmay_writer_write_cstr(writer, ",\"end\":")
        && mmay_diagnostic_write_json_position(writer, end)
        && mmay_writer_write_cstr(writer, "}");
}

static bool mmay_diagnostic_render_json(MarimayWriter writer, const MarimayDiagnostic *diagnostic) {
    if (!mmay_writer_write_cstr(writer, "{\"schema\":\"marimay.diagnostic/1\",\"code\":")) {
        return false;
    }
    if (!mmay_json_string(writer, diagnostic->code)
        || !mmay_writer_write_cstr(writer, ",\"severity\":")
        || !mmay_json_string(writer, mmay_severity_name(diagnostic->severity))
        || !mmay_writer_write_cstr(writer, ",\"message\":")
        || !mmay_json_bytes(
            writer, (const unsigned char *)diagnostic->message,
            mmay_diagnostic_message_length(diagnostic)
        )
        || !mmay_writer_write_cstr(writer, ",\"primary\":")
        || !mmay_diagnostic_write_json_primary(writer, diagnostic)
        || !mmay_writer_write_cstr(
            writer,
            ",\"labels\":[],\"notes\":[],\"fixes\":[]"
        )) {
        return false;
    }
    if (diagnostic->has_command_argument) {
        if (!mmay_writer_write_cstr(
                writer,
                ",\"context\":{\"kind\":\"command-line\",\"argument\":"
            )
            || !mmay_write_size(writer, diagnostic->command_argument)
            || !mmay_writer_write_cstr(writer, "}")) {
            return false;
        }
    } else if (diagnostic->runtime_resource
        == MMAY_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET) {
        if (!mmay_writer_write_cstr(
                writer,
                ",\"context\":{\"kind\":\"runtime-resource\","
                "\"resource\":\"managed-heap\","
                "\"classification\":\"budget-exceeded\",\"liveBytes\":"
            )
            || !mmay_write_size(writer, diagnostic->resource_live_bytes)
            || !mmay_writer_write_cstr(writer, ",\"requestedBytes\":")
            || !mmay_write_size(writer, diagnostic->resource_requested_bytes)
            || !mmay_writer_write_cstr(writer, ",\"limitBytes\":")
            || !mmay_write_size(writer, diagnostic->resource_limit_bytes)
            || !mmay_writer_write_cstr(writer, "}")) {
            return false;
        }
    } else if (diagnostic->runtime_resource
        == MMAY_RUNTIME_RESOURCE_HOST_ALLOCATION) {
        if (!mmay_writer_write_cstr(
                writer,
                ",\"context\":{\"kind\":\"runtime-resource\","
                "\"resource\":\"host-memory\","
                "\"classification\":\"allocation-failed\"}"
            )) {
            return false;
        }
    }
    return mmay_writer_write_cstr(writer, "}\n");
}

bool mmay_diagnostic_render(
    MarimayWriter writer,
    MarimayDiagnosticStyle style,
    const MarimayDiagnostic *diagnostic
) {
    const char *severity = NULL;
    MarimaySourcePosition start;
    MarimaySourcePosition end;
    bool has_source = false;
    if (diagnostic == NULL || diagnostic->code == NULL || diagnostic->message == NULL) {
        return false;
    }
    if (diagnostic->source != NULL
        && !mmay_diagnostic_source_positions(diagnostic, &start, &end)) {
        return false;
    }
    has_source = diagnostic->source != NULL;
    severity = mmay_severity_name(diagnostic->severity);
    if (style == MMAY_DIAGNOSTIC_JSON) {
        return mmay_diagnostic_render_json(writer, diagnostic);
    }
    if (style == MMAY_DIAGNOSTIC_SHORT) {
        if (has_source
            && (!mmay_writer_write_cstr(writer, mmay_source_path(diagnostic->source))
                || !mmay_writer_write_cstr(writer, ":")
                || !mmay_write_size(writer, start.line)
                || !mmay_writer_write_cstr(writer, ":")
                || !mmay_write_size(writer, start.column)
                || !mmay_writer_write_cstr(writer, ":"))) {
            return false;
        }
        return mmay_writer_write_cstr(writer, diagnostic->code)
            && mmay_writer_write_cstr(writer, ":")
            && mmay_writer_write_cstr(writer, severity)
            && mmay_writer_write_cstr(writer, ":")
            && mmay_diagnostic_write_message(writer, diagnostic)
            && mmay_writer_write_cstr(writer, "\n");
    }
    if (!mmay_writer_write_cstr(writer, severity)
        || !mmay_writer_write_cstr(writer, "[")
        || !mmay_writer_write_cstr(writer, diagnostic->code)
        || !mmay_writer_write_cstr(writer, "]: ")
        || !mmay_diagnostic_write_message(writer, diagnostic)
        || !mmay_writer_write_cstr(writer, "\n")) {
        return false;
    }
    if (!has_source) {
        return true;
    }
    if (!mmay_writer_write_cstr(writer, "  --> ")
        || !mmay_writer_write_cstr(writer, mmay_source_path(diagnostic->source))
        || !mmay_writer_write_cstr(writer, ":")
        || !mmay_write_size(writer, start.line)
        || !mmay_writer_write_cstr(writer, ":")
        || !mmay_write_size(writer, start.column)
        || !mmay_writer_write_cstr(writer, "\n")) {
        return false;
    }
    return mmay_diagnostic_write_source_window(
        writer,
        diagnostic,
        start,
        end
    );
}
