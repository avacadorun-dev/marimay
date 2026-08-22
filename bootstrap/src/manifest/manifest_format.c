#include "manifest_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/path.h"
#include "mmay/base/utf8.h"
#include "mmay/generated/diagnostic_codes.h"

static int mmay_manifest_bytes_compare(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
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

static int mmay_manifest_format_entry_compare(const void *left, const void *right) {
    const MarimayManifestFormatEntry *first = left;
    const MarimayManifestFormatEntry *second = right;
    int comparison = 0;
    if (first->kind != second->kind) {
        return first->kind < second->kind ? -1 : 1;
    }
    if (first->kind == MMAY_MANIFEST_DIRECTIVE_MODULE
        || first->kind == MMAY_MANIFEST_DIRECTIVE_LANGUAGE) {
        return first->source_order < second->source_order
            ? -1
            : (first->source_order > second->source_order ? 1 : 0);
    }
    if (first->kind == MMAY_MANIFEST_DIRECTIVE_METADATA) {
        comparison = mmay_manifest_bytes_compare(&first->name, &second->name);
        if (comparison != 0) {
            return comparison;
        }
    }
    comparison = mmay_manifest_bytes_compare(&first->values[0], &second->values[0]);
    if (comparison != 0 || first->value_count == 1 || second->value_count == 1) {
        return comparison;
    }
    return mmay_manifest_bytes_compare(&first->values[1], &second->values[1]);
}

static bool mmay_manifest_output_append(
    MarimayManifestOutput *output,
    const void *bytes,
    size_t length
) {
    if (length > output->max_bytes - output->bytes.length) {
        output->limit_reached = true;
        return false;
    }
    if (!mmay_vector_append(&output->bytes, bytes, length)) {
        output->allocation_failed = true;
        return false;
    }
    return true;
}

static bool mmay_manifest_output_cstr(
    MarimayManifestOutput *output,
    const char *text
) {
    return mmay_manifest_output_append(output, text, strlen(text));
}

static bool mmay_manifest_output_json_string(
    MarimayManifestOutput *output,
    const MarimayOwnedBytes *value
) {
    static const char hex[] = "0123456789abcdef";
    size_t cursor = 0;
    if (!mmay_manifest_output_cstr(output, "\"")) {
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
            if (!mmay_manifest_output_cstr(output, escape)) {
                return false;
            }
            cursor += 1;
        } else if (byte < 0x20U) {
            char encoded[6] = {'\\', 'u', '0', '0', '0', '0'};
            encoded[4] = hex[byte >> 4];
            encoded[5] = hex[byte & 0x0fU];
            if (!mmay_manifest_output_append(output, encoded, sizeof(encoded))) {
                return false;
            }
            cursor += 1;
        } else {
            MarimayUtf8Scalar scalar;
            size_t width = 1;
            if (byte >= 0x80U) {
                if (!mmay_utf8_decode(
                        value->data + cursor,
                        value->length - cursor,
                        &scalar
                    )) {
                    return false;
                }
                width = scalar.width;
            }
            if (!mmay_manifest_output_append(
                    output,
                    value->data + cursor,
                    width
                )) {
                return false;
            }
            cursor += width;
        }
    }
    return mmay_manifest_output_cstr(output, "\"");
}

static void mmay_manifest_format_entry_dispose(MarimayManifestFormatEntry *entry) {
    size_t index = 0;
    mmay_owned_bytes_dispose(&entry->name);
    for (index = 0; index < entry->value_count; index += 1) {
        mmay_owned_bytes_dispose(&entry->values[index]);
    }
}

static MarimayManifestFormatStatus mmay_manifest_collect_format_entries(
    const MarimayManifestParseResult *parsed,
    MarimayAllocator allocator,
    MarimayManifestFormatOptions options,
    MarimayVector *entries
) {
    size_t index = 0;
    size_t decoded_total = 0;
    for (index = 0; index < parsed->entries.length; index += 1) {
        const MarimayManifestEntry *source = mmay_manifest_entry(parsed, index);
        MarimayManifestFormatEntry entry;
        size_t value_index = 0;
        memset(&entry, 0, sizeof(entry));
        if (source == NULL) {
            return MMAY_MANIFEST_FORMAT_INVALID_ARGUMENT;
        }
        entry.kind = source->kind;
        entry.value_count = source->value_count;
        entry.source_order = source->source_order;
        if (entry.kind == MMAY_MANIFEST_DIRECTIVE_METADATA
            && !mmay_owned_bytes_copy(
                &entry.name,
                allocator,
                mmay_source_bytes(parsed->snapshot) + source->name.start,
                source->name.end - source->name.start
            )) {
            mmay_manifest_format_entry_dispose(&entry);
            return MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED;
        }
        for (value_index = 0; value_index < entry.value_count; value_index += 1) {
            if (!mmay_manifest_decode_string(
                    parsed,
                    source->values[value_index],
                    allocator,
                    &entry.values[value_index]
                )) {
                mmay_manifest_format_entry_dispose(&entry);
                return MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED;
            }
            if (entry.values[value_index].length
                    > options.max_decoded_bytes - decoded_total) {
                mmay_manifest_format_entry_dispose(&entry);
                return MMAY_MANIFEST_FORMAT_LIMIT_REACHED;
            }
            decoded_total += entry.values[value_index].length;
        }
        if (parsed->kind == MMAY_MANIFEST_WORKSPACE) {
            MarimayPath normalized;
            MarimayPathStatus path_status;
            size_t path_index = 0;
            memset(&normalized, 0, sizeof(normalized));
            for (path_index = 0; path_index < entry.values[0].length; path_index += 1) {
                if (entry.values[0].data[path_index] == '\\') {
                    mmay_manifest_format_entry_dispose(&entry);
                    return MMAY_MANIFEST_FORMAT_SOURCE_INVALID;
                }
            }
            path_status = mmay_path_normalize(
                &normalized,
                allocator,
                MMAY_PATH_POSIX,
                (const char *)entry.values[0].data,
                entry.values[0].length
            );
            if (path_status != MMAY_PATH_OK || normalized.absolute
                || normalized.text.length == 0
                || mmay_manifest_first_parent(&normalized.text)
                || (normalized.text.length > 3
                    && normalized.text.data[0] == '.'
                    && normalized.text.data[1] == '.'
                    && normalized.text.data[2] == '/')) {
                mmay_path_dispose(&normalized);
                mmay_manifest_format_entry_dispose(&entry);
                return path_status == MMAY_PATH_ALLOCATION_FAILED
                    ? MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED
                    : MMAY_MANIFEST_FORMAT_SOURCE_INVALID;
            }
            mmay_owned_bytes_dispose(&entry.values[0]);
            entry.values[0] = normalized.text;
            normalized.text.data = NULL;
            normalized.text.length = 0;
            mmay_path_dispose(&normalized);
        }
        if (!mmay_vector_push(entries, &entry)) {
            mmay_manifest_format_entry_dispose(&entry);
            return MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED;
        }
    }
    return MMAY_MANIFEST_FORMAT_OK;
}

static bool mmay_manifest_begin_section(
    MarimayManifestOutput *output,
    bool *wrote_section
) {
    if (*wrote_section && !mmay_manifest_output_cstr(output, "\n")) {
        return false;
    }
    *wrote_section = true;
    return true;
}

static bool mmay_manifest_write_simple_section(
    MarimayManifestOutput *output,
    const MarimayVector *entries,
    MarimayManifestDirectiveKind kind,
    const char *name,
    bool *wrote_section
) {
    size_t index = 0;
    bool began = false;
    for (index = 0; index < entries->length; index += 1) {
        const MarimayManifestFormatEntry *entry = mmay_vector_at_const(entries, index);
        if (entry == NULL || entry->kind != kind) {
            continue;
        }
        if (!began) {
            if (!mmay_manifest_begin_section(output, wrote_section)) {
                return false;
            }
            began = true;
        }
        if (!mmay_manifest_output_cstr(output, name)
            || !mmay_manifest_output_cstr(output, " ")
            || !mmay_manifest_output_json_string(output, &entry->values[0])
            || !mmay_manifest_output_cstr(output, "\n")) {
            return false;
        }
    }
    return true;
}

static bool mmay_manifest_write_group_section(
    MarimayManifestOutput *output,
    const MarimayVector *entries,
    MarimayManifestDirectiveKind kind,
    const char *name,
    bool present,
    bool *wrote_section
) {
    size_t index = 0;
    if (!present) {
        return true;
    }
    if (!mmay_manifest_begin_section(output, wrote_section)
        || !mmay_manifest_output_cstr(output, name)
        || !mmay_manifest_output_cstr(output, " (\n")) {
        return false;
    }
    for (index = 0; index < entries->length; index += 1) {
        const MarimayManifestFormatEntry *entry = mmay_vector_at_const(entries, index);
        if (entry == NULL || entry->kind != kind) {
            continue;
        }
        if (!mmay_manifest_output_cstr(output, "  ")
            || !mmay_manifest_output_json_string(output, &entry->values[0])
            || !mmay_manifest_output_cstr(output, " ")
            || !mmay_manifest_output_json_string(output, &entry->values[1])
            || !mmay_manifest_output_cstr(output, "\n")) {
            return false;
        }
    }
    return mmay_manifest_output_cstr(output, ")\n");
}

static bool mmay_manifest_write_metadata_section(
    MarimayManifestOutput *output,
    const MarimayVector *entries,
    bool *wrote_section
) {
    size_t index = 0;
    bool began = false;
    for (index = 0; index < entries->length; index += 1) {
        const MarimayManifestFormatEntry *entry = mmay_vector_at_const(entries, index);
        if (entry == NULL || entry->kind != MMAY_MANIFEST_DIRECTIVE_METADATA) {
            continue;
        }
        if (!began) {
            if (!mmay_manifest_begin_section(output, wrote_section)) {
                return false;
            }
            began = true;
        }
        if (!mmay_manifest_output_append(
                output,
                entry->name.data,
                entry->name.length
            )
            || !mmay_manifest_output_cstr(output, " ")
            || !mmay_manifest_output_json_string(output, &entry->values[0])
            || !mmay_manifest_output_cstr(output, "\n")) {
            return false;
        }
    }
    return true;
}

MarimayManifestFormatOptions mmay_manifest_format_default_options(void) {
    MarimayManifestFormatOptions options;
    options.max_decoded_bytes = 64U * 1024U * 1024U;
    options.max_output_bytes = 64U * 1024U * 1024U;
    return options;
}

MarimayManifestFormatStatus mmay_manifest_format(
    const MarimayManifestParseResult *parsed,
    MarimayAllocator allocator,
    MarimayManifestFormatOptions options,
    MarimayManifestFormatResult *result
) {
    MarimayVector entries;
    MarimayManifestOutput output;
    MarimayManifestFormatStatus status = MMAY_MANIFEST_FORMAT_OK;
    bool wrote_section = false;
    size_t index = 0;
    if (result == NULL) {
        return MMAY_MANIFEST_FORMAT_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    memset(&entries, 0, sizeof(entries));
    memset(&output, 0, sizeof(output));
    if (parsed == NULL || !parsed->initialized || parsed->snapshot == NULL
        || options.max_decoded_bytes == 0 || options.max_output_bytes == 0
        || !mmay_vector_init(&entries, allocator, sizeof(MarimayManifestFormatEntry))
        || !mmay_vector_init(&output.bytes, allocator, sizeof(unsigned char))) {
        return MMAY_MANIFEST_FORMAT_INVALID_ARGUMENT;
    }
    output.max_bytes = options.max_output_bytes;
    if (parsed->diagnostics.length != 0 || parsed->suppressed_diagnostics != 0) {
        status = MMAY_MANIFEST_FORMAT_SOURCE_INVALID;
        goto cleanup;
    }
    status = mmay_manifest_collect_format_entries(
        parsed,
        allocator,
        options,
        &entries
    );
    if (status != MMAY_MANIFEST_FORMAT_OK) {
        goto cleanup;
    }
    if (entries.length > 1) {
        qsort(
            entries.data,
            entries.length,
            sizeof(MarimayManifestFormatEntry),
            mmay_manifest_format_entry_compare
        );
    }
    if (parsed->kind == MMAY_MANIFEST_WORKSPACE) {
        if (!mmay_manifest_write_simple_section(
                &output,
                &entries,
                MMAY_MANIFEST_DIRECTIVE_USE,
                "use",
                &wrote_section
            )) {
            goto output_failure;
        }
    } else {
        if (!mmay_manifest_write_simple_section(
                &output,
                &entries,
                MMAY_MANIFEST_DIRECTIVE_MODULE,
                "module",
                &wrote_section
            )
            || !mmay_manifest_write_simple_section(
                &output,
                &entries,
                MMAY_MANIFEST_DIRECTIVE_LANGUAGE,
                "language",
                &wrote_section
            )
            || !mmay_manifest_write_group_section(
                &output,
                &entries,
                MMAY_MANIFEST_DIRECTIVE_REQUIRE,
                "require",
                parsed->has_require_group,
                &wrote_section
            )
            || !mmay_manifest_write_group_section(
                &output,
                &entries,
                MMAY_MANIFEST_DIRECTIVE_CAPABILITY,
                "capability",
                parsed->has_capability_group,
                &wrote_section
            )
            || !mmay_manifest_write_metadata_section(
                &output,
                &entries,
                &wrote_section
            )) {
            goto output_failure;
        }
    }
    if (!wrote_section && !mmay_manifest_output_cstr(&output, "\n")) {
        goto output_failure;
    }
    if (!mmay_owned_bytes_copy(
            &result->output,
            allocator,
            output.bytes.data,
            output.bytes.length
        )) {
        status = MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED;
        goto cleanup;
    }
    result->changed = output.bytes.length != mmay_source_length(parsed->snapshot)
        || memcmp(
            output.bytes.data,
            mmay_source_bytes(parsed->snapshot),
            output.bytes.length
        ) != 0;
    result->initialized = true;
    goto cleanup;

output_failure:
    status = output.limit_reached
        ? MMAY_MANIFEST_FORMAT_LIMIT_REACHED
        : (output.allocation_failed
            ? MMAY_MANIFEST_FORMAT_ALLOCATION_FAILED
            : MMAY_MANIFEST_FORMAT_INVALID_ARGUMENT);

cleanup:
    for (index = 0; index < entries.length; index += 1) {
        MarimayManifestFormatEntry *entry = mmay_vector_at(&entries, index);
        if (entry != NULL) {
            mmay_manifest_format_entry_dispose(entry);
        }
    }
    mmay_vector_dispose(&output.bytes);
    mmay_vector_dispose(&entries);
    return status;
}

void mmay_manifest_format_result_dispose(MarimayManifestFormatResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    mmay_owned_bytes_dispose(&result->output);
    result->changed = false;
    result->initialized = false;
}
