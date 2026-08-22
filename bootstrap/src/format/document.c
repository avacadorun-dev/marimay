#include "mmay/format/document.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/unicode.h"
#include "mmay/base/utf8.h"

typedef struct MarimayDocumentRenderState {
    const MarimayDocument *document;
    MarimayWriter writer;
    MarimayDocumentRenderOptions options;
    MarimayVector group_modes;
    size_t column;
    size_t indent;
    size_t scan_steps;
    bool at_line_start;
    bool suppress_indent;
    bool limit_reached;
} MarimayDocumentRenderState;

static MarimayDocumentStatus mmay_document_push(
    MarimayDocument *document,
    MarimayDocumentPart part
) {
    if (document == NULL || !document->initialized) {
        return MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    if (document->parts.length >= document->max_parts) {
        return MMAY_DOCUMENT_LIMIT_REACHED;
    }
    if (!mmay_vector_push(&document->parts, &part)) {
        return MMAY_DOCUMENT_ALLOCATION_FAILED;
    }
    return MMAY_DOCUMENT_OK;
}

bool mmay_document_init(
    MarimayDocument *document,
    MarimayAllocator allocator,
    size_t max_parts
) {
    if (document == NULL || max_parts == 0) {
        return false;
    }
    memset(document, 0, sizeof(*document));
    if (!mmay_vector_init(&document->parts, allocator, sizeof(MarimayDocumentPart))) {
        return false;
    }
    document->max_parts = max_parts;
    document->initialized = true;
    return true;
}

void mmay_document_dispose(MarimayDocument *document) {
    if (document == NULL || !document->initialized) {
        return;
    }
    mmay_vector_dispose(&document->parts);
    document->max_parts = 0;
    document->initialized = false;
}

MarimayDocumentStatus mmay_document_text(
    MarimayDocument *document,
    const unsigned char *text,
    size_t length
) {
    MarimayDocumentPart part;
    if ((text == NULL && length != 0)
        || (length != 0 && memchr(text, '\n', length) != NULL)
        || (length != 0 && memchr(text, '\r', length) != NULL)
        || !mmay_utf8_validate((const char *)text, length, NULL)) {
        return MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    part.kind = MMAY_DOCUMENT_TEXT;
    part.text = text;
    part.length = length;
    return mmay_document_push(document, part);
}

MarimayDocumentStatus mmay_document_break_text(
    MarimayDocument *document,
    const unsigned char *text,
    size_t length
) {
    MarimayDocumentPart part;
    if ((text == NULL && length != 0)
        || (length != 0 && memchr(text, '\n', length) != NULL)
        || (length != 0 && memchr(text, '\r', length) != NULL)
        || !mmay_utf8_validate((const char *)text, length, NULL)) {
        return MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    part.kind = MMAY_DOCUMENT_BREAK_TEXT;
    part.text = text;
    part.length = length;
    return mmay_document_push(document, part);
}

MarimayDocumentStatus mmay_document_part(
    MarimayDocument *document,
    MarimayDocumentPartKind kind
) {
    MarimayDocumentPart part;
    if (kind == MMAY_DOCUMENT_TEXT || kind == MMAY_DOCUMENT_BREAK_TEXT
        || kind > MMAY_DOCUMENT_LITERAL_LINE) {
        return MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    part.kind = kind;
    part.text = NULL;
    part.length = 0;
    return mmay_document_push(document, part);
}

MarimayDocumentRenderOptions mmay_document_default_render_options(void) {
    MarimayDocumentRenderOptions options;
    options.width = 100;
    options.indent_width = 2;
    options.max_nesting = 512;
    options.max_scan_steps = 4000000;
    return options;
}

static bool mmay_document_text_width(
    const MarimayDocumentPart *part,
    size_t limit,
    size_t *width
) {
    const unsigned char *cursor = part->text;
    size_t remaining = part->length;
    size_t measured = 0;
    while (remaining != 0) {
        MarimayUtf8Scalar scalar;
        size_t scalar_width = 0;
        if (!mmay_utf8_decode(cursor, remaining, &scalar)) {
            return false;
        }
        scalar_width = mmay_unicode_formatter_width(scalar.value);
        if (!mmay_size_add(measured, scalar_width, &measured)) {
            return false;
        }
        if (measured > limit) {
            *width = measured;
            return true;
        }
        cursor += scalar.width;
        remaining -= scalar.width;
    }
    *width = measured;
    return true;
}

static bool mmay_document_group_fits(
    MarimayDocumentRenderState *state,
    size_t start,
    size_t remaining_width
) {
    size_t index = start;
    size_t nesting = 1;
    size_t used = 0;
    while (index < state->document->parts.length) {
        const MarimayDocumentPart *part = mmay_vector_at_const(
            &state->document->parts,
            index
        );
        size_t text_width = 0;
        if (!mmay_size_add(state->scan_steps, 1, &state->scan_steps)
            || state->scan_steps > state->options.max_scan_steps) {
            state->limit_reached = true;
            return false;
        }
        if (part == NULL) {
            return false;
        }
        if (part->kind == MMAY_DOCUMENT_GROUP_BEGIN) {
            nesting += 1;
            if (nesting > state->options.max_nesting) {
                state->limit_reached = true;
                return false;
            }
        } else if (part->kind == MMAY_DOCUMENT_GROUP_END) {
            nesting -= 1;
            if (nesting == 0) {
                return used <= remaining_width;
            }
        } else if (part->kind == MMAY_DOCUMENT_HARDLINE
            || part->kind == MMAY_DOCUMENT_LITERAL_LINE) {
            return false;
        } else if (part->kind == MMAY_DOCUMENT_SOFTLINE) {
            if (!mmay_size_add(used, 1, &used)) {
                state->limit_reached = true;
                return false;
            }
        } else if (part->kind == MMAY_DOCUMENT_TEXT) {
            if (!mmay_document_text_width(part, remaining_width, &text_width)
                || !mmay_size_add(used, text_width, &used)) {
                state->limit_reached = true;
                return false;
            }
        }
        if (used > remaining_width) {
            return false;
        }
        index += 1;
    }
    return false;
}

static bool mmay_document_write_spaces(MarimayWriter writer, size_t count) {
    static const char spaces[] =
        "                                                                ";
    while (count != 0) {
        size_t chunk = count < sizeof(spaces) - 1 ? count : sizeof(spaces) - 1;
        if (!mmay_writer_write(writer, spaces, chunk)) {
            return false;
        }
        count -= chunk;
    }
    return true;
}

static bool mmay_document_current_flat(const MarimayDocumentRenderState *state) {
    const bool *flat = NULL;
    if (state->group_modes.length == 0) {
        return false;
    }
    flat = mmay_vector_at_const(
        &state->group_modes,
        state->group_modes.length - 1
    );
    return flat != NULL && *flat;
}

MarimayDocumentStatus mmay_document_render(
    const MarimayDocument *document,
    MarimayWriter writer,
    MarimayDocumentRenderOptions options
) {
    MarimayDocumentRenderState state;
    size_t index = 0;
    size_t indent_nesting = 0;
    MarimayDocumentStatus status = MMAY_DOCUMENT_OK;
    if (document == NULL || !document->initialized || writer.write == NULL
        || options.width == 0 || options.indent_width == 0
        || options.max_nesting == 0 || options.max_scan_steps == 0) {
        return MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    memset(&state, 0, sizeof(state));
    state.document = document;
    state.writer = writer;
    state.options = options;
    if (!mmay_vector_init(
            &state.group_modes,
            document->parts.allocator,
            sizeof(bool)
        )) {
        return MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    for (index = 0; index < document->parts.length; index += 1) {
        const MarimayDocumentPart *part = mmay_vector_at_const(&document->parts, index);
        bool flat = false;
        size_t text_width = 0;
        if (!mmay_size_add(state.scan_steps, 1, &state.scan_steps)
            || state.scan_steps > options.max_scan_steps) {
            status = MMAY_DOCUMENT_LIMIT_REACHED;
            break;
        }
        if (part == NULL) {
            status = MMAY_DOCUMENT_INVALID_ARGUMENT;
            break;
        }
        if (part->kind == MMAY_DOCUMENT_TEXT
            || (part->kind == MMAY_DOCUMENT_BREAK_TEXT
                && !mmay_document_current_flat(&state))) {
            if (part->length != 0 && state.at_line_start) {
                if (!state.suppress_indent
                    && !mmay_document_write_spaces(writer, state.indent)) {
                    status = MMAY_DOCUMENT_WRITE_FAILED;
                    break;
                }
                state.column = state.suppress_indent ? 0 : state.indent;
                state.at_line_start = false;
                state.suppress_indent = false;
            }
            if (!mmay_document_text_width(part, SIZE_MAX, &text_width)
                || !mmay_writer_write(
                    writer,
                    (const char *)part->text,
                    part->length
                )) {
                status = MMAY_DOCUMENT_WRITE_FAILED;
                break;
            }
            if (!mmay_size_add(state.column, text_width, &state.column)) {
                status = MMAY_DOCUMENT_LIMIT_REACHED;
                break;
            }
        } else if (part->kind == MMAY_DOCUMENT_LITERAL_LINE) {
            if (!mmay_writer_write(writer, "\n", 1)) {
                status = MMAY_DOCUMENT_WRITE_FAILED;
                break;
            }
            state.column = 0;
            state.at_line_start = true;
            state.suppress_indent = true;
        } else if (part->kind == MMAY_DOCUMENT_HARDLINE
            || ((part->kind == MMAY_DOCUMENT_SOFTLINE
                    || part->kind == MMAY_DOCUMENT_SOFTBREAK)
                && !mmay_document_current_flat(&state))) {
            if (!mmay_writer_write(writer, "\n", 1)) {
                status = MMAY_DOCUMENT_WRITE_FAILED;
                break;
            }
            state.column = 0;
            state.at_line_start = true;
            state.suppress_indent = false;
        } else if (part->kind == MMAY_DOCUMENT_SOFTLINE) {
            if (!mmay_writer_write(writer, " ", 1)) {
                status = MMAY_DOCUMENT_WRITE_FAILED;
                break;
            }
            if (!mmay_size_add(state.column, 1, &state.column)) {
                status = MMAY_DOCUMENT_LIMIT_REACHED;
                break;
            }
        } else if (part->kind == MMAY_DOCUMENT_SOFTBREAK) {
            continue;
        } else if (part->kind == MMAY_DOCUMENT_GROUP_BEGIN) {
            if (state.group_modes.length >= options.max_nesting) {
                status = MMAY_DOCUMENT_LIMIT_REACHED;
                break;
            }
            flat = mmay_document_current_flat(&state)
                || (state.column <= options.width
                    && mmay_document_group_fits(
                        &state,
                        index + 1,
                        options.width - state.column
                    ));
            if (state.limit_reached) {
                status = MMAY_DOCUMENT_LIMIT_REACHED;
                break;
            }
            if (!mmay_vector_push(&state.group_modes, &flat)) {
                status = MMAY_DOCUMENT_ALLOCATION_FAILED;
                break;
            }
        } else if (part->kind == MMAY_DOCUMENT_GROUP_END) {
            if (state.group_modes.length == 0) {
                status = MMAY_DOCUMENT_INVALID_ARGUMENT;
                break;
            }
            state.group_modes.length -= 1;
        } else if (part->kind == MMAY_DOCUMENT_INDENT_BEGIN) {
            if (indent_nesting >= options.max_nesting
                || !mmay_size_add(
                    state.indent,
                    options.indent_width,
                    &state.indent
                )) {
                status = MMAY_DOCUMENT_LIMIT_REACHED;
                break;
            }
            indent_nesting += 1;
        } else if (part->kind == MMAY_DOCUMENT_INDENT_END) {
            if (indent_nesting == 0 || state.indent < options.indent_width) {
                status = MMAY_DOCUMENT_INVALID_ARGUMENT;
                break;
            }
            indent_nesting -= 1;
            state.indent -= options.indent_width;
        }
    }
    if (status == MMAY_DOCUMENT_OK
        && (state.group_modes.length != 0 || indent_nesting != 0)) {
        status = MMAY_DOCUMENT_INVALID_ARGUMENT;
    }
    mmay_vector_dispose(&state.group_modes);
    return status;
}
