#include "manifest_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/path.h"
#include "ruda/base/utf8.h"
#include "ruda/generated/diagnostic_codes.h"

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
