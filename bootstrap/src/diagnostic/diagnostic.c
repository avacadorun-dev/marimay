#include "ruda/diagnostic.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/unicode.h"
#include "ruda/base/utf8.h"

static const char *ruda_severity_name(RudaSeverity severity) {
    switch (severity) {
        case RUDA_SEVERITY_ERROR:
            return "error";
        case RUDA_SEVERITY_WARNING:
            return "warning";
        case RUDA_SEVERITY_INFORMATION:
            return "information";
        case RUDA_SEVERITY_HELP:
            return "help";
    }
    return "error";
}

static bool ruda_json_bytes(
    RudaWriter writer,
    const unsigned char *text,
    size_t length
) {
    const unsigned char *cursor = text;
    size_t remaining = length;
    char escape[7];
    if (!ruda_writer_write_cstr(writer, "\"")) {
        return false;
    }
    while (remaining != 0) {
        unsigned char byte = *cursor;
        if (byte == '"' || byte == '\\') {
            char pair[2] = {'\\', (char)byte};
            if (!ruda_writer_write(writer, pair, sizeof(pair))) {
                return false;
            }
        } else if (byte == '\b') {
            if (!ruda_writer_write_cstr(writer, "\\b")) {
                return false;
            }
        } else if (byte == '\f') {
            if (!ruda_writer_write_cstr(writer, "\\f")) {
                return false;
            }
        } else if (byte == '\n') {
            if (!ruda_writer_write_cstr(writer, "\\n")) {
                return false;
            }
        } else if (byte == '\r') {
            if (!ruda_writer_write_cstr(writer, "\\r")) {
                return false;
            }
        } else if (byte == '\t') {
            if (!ruda_writer_write_cstr(writer, "\\t")) {
                return false;
            }
        } else if (byte < 0x20U) {
            int escaped_length = snprintf(
                escape, sizeof(escape), "\\u%04x", (unsigned int)byte
            );
            if (escaped_length != 6
                || !ruda_writer_write(writer, escape, 6)) {
                return false;
            }
        } else if (byte < 0x80U) {
            if (!ruda_writer_write(writer, (const char *)cursor, 1)) {
                return false;
            }
        } else {
            RudaUtf8Scalar scalar;
            if (ruda_utf8_decode(cursor, remaining, &scalar)) {
                if (!ruda_writer_write(writer, (const char *)cursor, scalar.width)) {
                    return false;
                }
                cursor += scalar.width;
                remaining -= scalar.width;
                continue;
            }
            if (!ruda_writer_write_cstr(writer, "\\ufffd")) {
                return false;
            }
        }
        cursor += 1;
        remaining -= 1;
    }
    return ruda_writer_write_cstr(writer, "\"");
}

static bool ruda_json_string(RudaWriter writer, const char *text) {
    return ruda_json_bytes(
        writer, (const unsigned char *)text, strlen(text)
    );
}

static size_t ruda_diagnostic_message_length(
    const RudaDiagnostic *diagnostic
) {
    return diagnostic->message_is_counted
        ? diagnostic->message_length : strlen(diagnostic->message);
}

static bool ruda_diagnostic_write_message(
    RudaWriter writer,
    const RudaDiagnostic *diagnostic
) {
    return ruda_writer_write(
        writer, diagnostic->message,
        ruda_diagnostic_message_length(diagnostic)
    );
}

static bool ruda_write_size(RudaWriter writer, size_t value) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%zu", value);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return false;
    }
    return ruda_writer_write(writer, buffer, (size_t)length);
}

static bool ruda_diagnostic_source_positions(
    const RudaDiagnostic *diagnostic,
    RudaSourcePosition *start,
    RudaSourcePosition *end
) {
    return diagnostic->source != NULL
        && diagnostic->source_start <= diagnostic->source_end
        && diagnostic->source_end <= ruda_source_length(diagnostic->source)
        && ruda_source_position(diagnostic->source, diagnostic->source_start, start)
        && ruda_source_position(diagnostic->source, diagnostic->source_end, end);
}

static bool ruda_diagnostic_utf8_continuation(unsigned char byte) {
    return (byte & 0xc0U) == 0x80U;
}

static bool ruda_diagnostic_repeat(
    RudaWriter writer,
    char character,
    size_t count
) {
    char buffer[64];
    memset(buffer, character, sizeof(buffer));
    while (count != 0) {
        size_t chunk = count < sizeof(buffer) ? count : sizeof(buffer);
        if (!ruda_writer_write(writer, buffer, chunk)) {
            return false;
        }
        count -= chunk;
    }
    return true;
}

static size_t ruda_diagnostic_scalar_width(
    unsigned char byte,
    const RudaUtf8Scalar *scalar,
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
    return ruda_unicode_formatter_width(scalar->value);
}

static bool ruda_diagnostic_measure_range(
    const unsigned char *bytes,
    size_t start,
    size_t end,
    size_t initial_column,
    size_t *width
) {
    size_t cursor = start;
    size_t column = initial_column;
    while (cursor < end) {
        RudaUtf8Scalar scalar;
        size_t scalar_width = 0;
        size_t byte_width = 1;
        bool valid = ruda_utf8_decode(bytes + cursor, end - cursor, &scalar);
        if (valid) {
            byte_width = scalar.width;
        }
        scalar_width = ruda_diagnostic_scalar_width(
            bytes[cursor],
            valid ? &scalar : NULL,
            column
        );
        if (!ruda_size_add(column, scalar_width, &column)) {
            return false;
        }
        cursor += byte_width;
    }
    *width = column - initial_column;
    return true;
}

static bool ruda_diagnostic_write_sanitized_range(
    RudaWriter writer,
    const unsigned char *bytes,
    size_t start,
    size_t end,
    size_t *column
) {
    static const char replacement[] = "\xef\xbf\xbd";
    size_t cursor = start;
    while (cursor < end) {
        unsigned char byte = bytes[cursor];
        RudaUtf8Scalar scalar;
        size_t scalar_width = 0;
        size_t byte_width = 1;
        bool valid = ruda_utf8_decode(bytes + cursor, end - cursor, &scalar);
        if (valid) {
            byte_width = scalar.width;
        }
        scalar_width = ruda_diagnostic_scalar_width(
            byte,
            valid ? &scalar : NULL,
            *column
        );
        if (byte == '\t') {
            if (!ruda_diagnostic_repeat(writer, ' ', scalar_width)) {
                return false;
            }
        } else if (byte < 0x20U || byte == 0x7fU) {
            if (!ruda_writer_write_cstr(writer, "?")) {
                return false;
            }
        } else if (valid) {
            if (!ruda_writer_write(
                    writer,
                    (const char *)bytes + cursor,
                    byte_width
                )) {
                return false;
            }
        } else if (!ruda_writer_write(writer, replacement, sizeof(replacement) - 1)) {
            return false;
        }
        if (!ruda_size_add(*column, scalar_width, column)) {
            return false;
        }
        cursor += byte_width;
    }
    return true;
}

static bool ruda_diagnostic_write_source_window(
    RudaWriter writer,
    const RudaDiagnostic *diagnostic,
    RudaSourcePosition start_position,
    RudaSourcePosition end_position
) {
    const unsigned char *bytes = ruda_source_bytes(diagnostic->source);
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
    if (!ruda_source_line_bounds(
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
            && ruda_diagnostic_utf8_continuation(bytes[window_start])) {
            window_start += 1;
        }
        prefix_elided = true;
    }
    window_end = line_end;
    if (window_end - window_start > 200) {
        window_end = window_start + 200;
        while (window_end > window_start && window_end < line_end
            && ruda_diagnostic_utf8_continuation(bytes[window_end])) {
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
    if (!ruda_writer_write(writer, label, (size_t)label_length)) {
        return false;
    }
    if (prefix_elided) {
        if (!ruda_writer_write_cstr(writer, "...")) {
            return false;
        }
        column = 3;
    }
    if (!ruda_diagnostic_write_sanitized_range(
            writer,
            bytes,
            window_start,
            window_end,
            &column
        )
        || (suffix_elided && !ruda_writer_write_cstr(writer, "..."))
        || !ruda_writer_write_cstr(writer, "\n")) {
        return false;
    }
    before_width = prefix_elided ? 3 : 0;
    {
        size_t measured = 0;
        if (!ruda_diagnostic_measure_range(
                bytes,
                window_start,
                primary_start,
                before_width,
                &measured
            ) || !ruda_size_add(before_width, measured, &before_width)) {
            return false;
        }
    }
    if (primary_end > primary_start) {
        if (!ruda_diagnostic_measure_range(
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
    return ruda_diagnostic_repeat(writer, ' ', line_digits)
        && ruda_writer_write_cstr(writer, " | ")
        && ruda_diagnostic_repeat(writer, ' ', before_width)
        && ruda_diagnostic_repeat(writer, '^', caret_width)
        && ruda_writer_write_cstr(writer, "\n");
}

static bool ruda_diagnostic_write_json_position(
    RudaWriter writer,
    RudaSourcePosition position
) {
    return ruda_writer_write_cstr(writer, "{\"line\":")
        && ruda_write_size(writer, position.line)
        && ruda_writer_write_cstr(writer, ",\"column\":")
        && ruda_write_size(writer, position.column)
        && ruda_writer_write_cstr(writer, "}");
}

static bool ruda_diagnostic_write_json_primary(
    RudaWriter writer,
    const RudaDiagnostic *diagnostic
) {
    RudaSourcePosition start;
    RudaSourcePosition end;
    if (diagnostic->source == NULL) {
        return ruda_writer_write_cstr(writer, "null");
    }
    if (!ruda_diagnostic_source_positions(diagnostic, &start, &end)) {
        return false;
    }
    return ruda_writer_write_cstr(writer, "{\"file\":")
        && ruda_json_string(writer, ruda_source_path(diagnostic->source))
        && ruda_writer_write_cstr(writer, ",\"snapshot\":")
        && ruda_json_string(
            writer,
            ruda_source_snapshot_identity(diagnostic->source)
        )
        && ruda_writer_write_cstr(writer, ",\"startByte\":")
        && ruda_write_size(writer, diagnostic->source_start)
        && ruda_writer_write_cstr(writer, ",\"endByte\":")
        && ruda_write_size(writer, diagnostic->source_end)
        && ruda_writer_write_cstr(writer, ",\"start\":")
        && ruda_diagnostic_write_json_position(writer, start)
        && ruda_writer_write_cstr(writer, ",\"end\":")
        && ruda_diagnostic_write_json_position(writer, end)
        && ruda_writer_write_cstr(writer, "}");
}

static bool ruda_diagnostic_render_json(RudaWriter writer, const RudaDiagnostic *diagnostic) {
    if (!ruda_writer_write_cstr(writer, "{\"schema\":\"ruda.diagnostic/1\",\"code\":")) {
        return false;
    }
    if (!ruda_json_string(writer, diagnostic->code)
        || !ruda_writer_write_cstr(writer, ",\"severity\":")
        || !ruda_json_string(writer, ruda_severity_name(diagnostic->severity))
        || !ruda_writer_write_cstr(writer, ",\"message\":")
        || !ruda_json_bytes(
            writer, (const unsigned char *)diagnostic->message,
            ruda_diagnostic_message_length(diagnostic)
        )
        || !ruda_writer_write_cstr(writer, ",\"primary\":")
        || !ruda_diagnostic_write_json_primary(writer, diagnostic)
        || !ruda_writer_write_cstr(
            writer,
            ",\"labels\":[],\"notes\":[],\"fixes\":[]"
        )) {
        return false;
    }
    if (diagnostic->has_command_argument) {
        if (!ruda_writer_write_cstr(
                writer,
                ",\"context\":{\"kind\":\"command-line\",\"argument\":"
            )
            || !ruda_write_size(writer, diagnostic->command_argument)
            || !ruda_writer_write_cstr(writer, "}")) {
            return false;
        }
    } else if (diagnostic->runtime_resource
        == RUDA_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET) {
        if (!ruda_writer_write_cstr(
                writer,
                ",\"context\":{\"kind\":\"runtime-resource\","
                "\"resource\":\"managed-heap\","
                "\"classification\":\"budget-exceeded\",\"liveBytes\":"
            )
            || !ruda_write_size(writer, diagnostic->resource_live_bytes)
            || !ruda_writer_write_cstr(writer, ",\"requestedBytes\":")
            || !ruda_write_size(writer, diagnostic->resource_requested_bytes)
            || !ruda_writer_write_cstr(writer, ",\"limitBytes\":")
            || !ruda_write_size(writer, diagnostic->resource_limit_bytes)
            || !ruda_writer_write_cstr(writer, "}")) {
            return false;
        }
    } else if (diagnostic->runtime_resource
        == RUDA_RUNTIME_RESOURCE_HOST_ALLOCATION) {
        if (!ruda_writer_write_cstr(
                writer,
                ",\"context\":{\"kind\":\"runtime-resource\","
                "\"resource\":\"host-memory\","
                "\"classification\":\"allocation-failed\"}"
            )) {
            return false;
        }
    }
    return ruda_writer_write_cstr(writer, "}\n");
}

bool ruda_diagnostic_render(
    RudaWriter writer,
    RudaDiagnosticStyle style,
    const RudaDiagnostic *diagnostic
) {
    const char *severity = NULL;
    RudaSourcePosition start;
    RudaSourcePosition end;
    bool has_source = false;
    if (diagnostic == NULL || diagnostic->code == NULL || diagnostic->message == NULL) {
        return false;
    }
    if (diagnostic->source != NULL
        && !ruda_diagnostic_source_positions(diagnostic, &start, &end)) {
        return false;
    }
    has_source = diagnostic->source != NULL;
    severity = ruda_severity_name(diagnostic->severity);
    if (style == RUDA_DIAGNOSTIC_JSON) {
        return ruda_diagnostic_render_json(writer, diagnostic);
    }
    if (style == RUDA_DIAGNOSTIC_SHORT) {
        if (has_source
            && (!ruda_writer_write_cstr(writer, ruda_source_path(diagnostic->source))
                || !ruda_writer_write_cstr(writer, ":")
                || !ruda_write_size(writer, start.line)
                || !ruda_writer_write_cstr(writer, ":")
                || !ruda_write_size(writer, start.column)
                || !ruda_writer_write_cstr(writer, ":"))) {
            return false;
        }
        return ruda_writer_write_cstr(writer, diagnostic->code)
            && ruda_writer_write_cstr(writer, ":")
            && ruda_writer_write_cstr(writer, severity)
            && ruda_writer_write_cstr(writer, ":")
            && ruda_diagnostic_write_message(writer, diagnostic)
            && ruda_writer_write_cstr(writer, "\n");
    }
    if (!ruda_writer_write_cstr(writer, severity)
        || !ruda_writer_write_cstr(writer, "[")
        || !ruda_writer_write_cstr(writer, diagnostic->code)
        || !ruda_writer_write_cstr(writer, "]: ")
        || !ruda_diagnostic_write_message(writer, diagnostic)
        || !ruda_writer_write_cstr(writer, "\n")) {
        return false;
    }
    if (!has_source) {
        return true;
    }
    if (!ruda_writer_write_cstr(writer, "  --> ")
        || !ruda_writer_write_cstr(writer, ruda_source_path(diagnostic->source))
        || !ruda_writer_write_cstr(writer, ":")
        || !ruda_write_size(writer, start.line)
        || !ruda_writer_write_cstr(writer, ":")
        || !ruda_write_size(writer, start.column)
        || !ruda_writer_write_cstr(writer, "\n")) {
        return false;
    }
    return ruda_diagnostic_write_source_window(
        writer,
        diagnostic,
        start,
        end
    );
}
