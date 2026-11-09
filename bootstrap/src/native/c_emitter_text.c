#include "c_emitter_internal.h"

static bool mmay_native_c_emit_text_status(
    MarimayNativeCBuffer *buffer,
    const MarimayMirInstruction *instruction
) {
    return mmay_native_c_write(
        buffer,
        " if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
        instruction->span.start, instruction->span.end
    );
}

bool mmay_native_c_emit_text_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(buffer,
        "static int mmay_g_text_string(mmay_g_execution *x, const unsigned char *bytes, size_t length, mmay_g_string *out) { MarimayManagedStatus status; status = mmay_managed_string_new(&x->heap, bytes, length, &out->managed); if (status == MMAY_MANAGED_OK) mmay_managed_string_generated_bytes(&x->heap, out->managed, &out->data, &out->length); return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_text_raw_array(mmay_g_execution *x, mmay_g_string text, const MarimayManagedArrayDescriptor *descriptor, bool runes, MarimayManagedReference *out) { int64_t signed_count = 0; size_t count = text.length, allocation = 0, index = 0, offset = 0; MarimayManagedCell *cells = NULL; MarimayManagedStatus status; if (runes && !mmay_text_rune_count(text.data, text.length, &signed_count)) return 7; if (runes) count = (size_t)signed_count; if (!mmay_size_multiply(count, sizeof(*cells), &allocation)) return 6; cells = allocation == 0 ? NULL : mmay_allocate(&x->allocator, allocation); if (allocation != 0 && cells == NULL) return 5; if (runes) { while (offset < text.length) { MarimayUtf8Scalar scalar; if (!mmay_utf8_decode(text.data + offset, text.length - offset, &scalar)) { mmay_deallocate(&x->allocator, cells, allocation); return 7; } cells[index++].f64_bits = scalar.value; offset += scalar.width; } } else { for (index = 0; index < text.length; index += 1) cells[index].f64_bits = text.data[index]; } status = mmay_managed_array_new(&x->heap, descriptor, cells, count, out); mmay_deallocate(&x->allocator, cells, allocation); return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_text_string_array(mmay_g_execution *x, mmay_g_string text, mmay_g_string separator, const MarimayManagedArrayDescriptor *descriptor, bool graphemes, MarimayManagedReference *out) { int64_t signed_count = 0; size_t count, cell_bytes = 0, index, start = 0, end = 0; MarimayManagedCell *cells = NULL; MarimayManagedRootFrame roots = {0}; MarimayManagedStatus status = MMAY_MANAGED_OK, pop_status; bool rooted = false; if (graphemes ? !mmay_text_grapheme_count(text.data, text.length, &signed_count) : !mmay_text_split_count(text.data, text.length, separator.data, separator.length, &signed_count)) return 7; count = (size_t)signed_count; if (!mmay_size_multiply(count, sizeof(*cells), &cell_bytes)) return 6; cells = cell_bytes == 0 ? NULL : mmay_allocate(&x->allocator, cell_bytes); if (cell_bytes != 0 && cells == NULL) return 5; for (index = 0; index < count; index += 1) cells[index].reference = mmay_managed_invalid_reference(); if (count != 0) { status = mmay_managed_root_push_strided(&x->heap, &roots, cells, sizeof(*cells), count); rooted = status == MMAY_MANAGED_OK; } for (index = 0; status == MMAY_MANAGED_OK && index < count; index += 1) { if (graphemes) { if (!mmay_text_next_grapheme(text.data, text.length, start, &end)) { status = MMAY_MANAGED_INVALID_ARGUMENT; break; } } else { end = start; while (end <= text.length && separator.length <= text.length - end && memcmp(text.data + end, separator.data, separator.length) != 0) end += 1; if (separator.length > text.length - end) end = text.length; } status = mmay_managed_string_new(&x->heap, mmay_text_range_start(text.data, start), end - start, &cells[index].reference); start = graphemes ? end : end + separator.length; } if (status == MMAY_MANAGED_OK) status = mmay_managed_array_new(&x->heap, descriptor, cells, count, out); if (rooted) { pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; } mmay_deallocate(&x->allocator, cells, cell_bytes); return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_text_array_bytes(mmay_g_execution *x, MarimayManagedReference array, unsigned char **bytes, size_t *length) { size_t count = 0, index; unsigned char *copy = NULL; MarimayManagedStatus status = mmay_managed_array_length(&x->heap, array, &count); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); copy = count == 0 ? NULL : mmay_allocate(&x->allocator, count); if (count != 0 && copy == NULL) return 5; for (index = 0; index < count; index += 1) { MarimayManagedCell cell = {0}; size_t ignored = 0; status = mmay_managed_array_get(&x->heap, array, (int64_t)index, &cell, &ignored); if (status != MMAY_MANAGED_OK) { mmay_deallocate(&x->allocator, copy, count); return mmay_g_managed_status(status); } copy[index] = (unsigned char)cell.f64_bits; } *bytes = copy; *length = count; return 0; }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_text_join(mmay_g_execution *x, MarimayManagedReference parts, mmay_g_string separator, mmay_g_string *out) { size_t count = 0, total = 0, index, offset = 0; unsigned char *joined = NULL; MarimayManagedStatus status = mmay_managed_array_length(&x->heap, parts, &count); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); for (index = 0; index < count; index += 1) { MarimayManagedCell cell = {0}; const unsigned char *bytes = NULL; size_t length = 0, ignored = 0; status = mmay_managed_array_get(&x->heap, parts, (int64_t)index, &cell, &ignored); if (status == MMAY_MANAGED_OK) status = mmay_managed_string_bytes(&x->heap, cell.reference, &bytes, &length); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (!mmay_size_add(total, length, &total) || (index != 0 && !mmay_size_add(total, separator.length, &total))) return 6; } joined = total == 0 ? NULL : mmay_allocate(&x->allocator, total); if (total != 0 && joined == NULL) return 5; for (index = 0; index < count; index += 1) { MarimayManagedCell cell = {0}; const unsigned char *bytes = NULL; size_t length = 0, ignored = 0; status = mmay_managed_array_get(&x->heap, parts, (int64_t)index, &cell, &ignored); if (status == MMAY_MANAGED_OK) status = mmay_managed_string_bytes(&x->heap, cell.reference, &bytes, &length); if (status != MMAY_MANAGED_OK) { mmay_deallocate(&x->allocator, joined, total); return mmay_g_managed_status(status); } if (index != 0 && separator.length != 0) { memcpy(joined + offset, separator.data, separator.length); offset += separator.length; } if (length != 0) memcpy(joined + offset, bytes, length); offset += length; } { int result = mmay_g_text_string(x, joined, total, out); mmay_deallocate(&x->allocator, joined, total); return result; } }\n"
    ) && mmay_native_c_write(buffer,
        "static MMAY_G_UNUSED void mmay_g_text_keep_helpers(void) { (void)mmay_g_text_string; (void)mmay_g_text_raw_array; (void)mmay_g_text_string_array; (void)mmay_g_text_array_bytes; (void)mmay_g_text_join; }\n"
    );
}

bool mmay_native_c_emit_text_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayTypeId *target_type = instruction != NULL
        ? mmay_vector_at_const(
            &function->register_types, instruction->target
        ) : NULL;
    const char *descriptor = target_type != NULL
        ? mmay_native_c_array_descriptor(function, *target_type) : NULL;
    if (buffer == NULL || function == NULL || instruction == NULL
        || target_type == NULL) return false;
    switch (instruction->text_operation) {
        case MMAY_MIR_TEXT_BYTE_LENGTH:
            return mmay_native_c_write(
                buffer,
                "  if (!mmay_text_byte_length(mmay_g_v_%u.length, &mmay_g_v_%u)) { mmay_g_s = 6; goto mmay_g_cleanup; }\n",
                instruction->first, instruction->target
            );
        case MMAY_MIR_TEXT_RUNE_COUNT:
        case MMAY_MIR_TEXT_GRAPHEME_COUNT:
            return mmay_native_c_write(
                buffer,
                "  if (!%s(mmay_g_v_%u.data, mmay_g_v_%u.length, &mmay_g_v_%u)) { mmay_g_s = 7; goto mmay_g_cleanup; }\n",
                instruction->text_operation == MMAY_MIR_TEXT_RUNE_COUNT
                    ? "mmay_text_rune_count" : "mmay_text_grapheme_count",
                instruction->first, instruction->first,
                instruction->target
            );
        case MMAY_MIR_TEXT_IS_RUNE_BOUNDARY:
            return mmay_native_c_write(
                buffer,
                "  mmay_g_v_%u = mmay_text_is_rune_boundary(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u);\n",
                instruction->target, instruction->first,
                instruction->first, instruction->second
            );
        case MMAY_MIR_TEXT_SUBSTRING:
            if (!mmay_native_c_write(buffer,
                    "  if (mmay_g_v_%u < mmay_g_v_%u || !mmay_text_is_rune_boundary(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u) || !mmay_text_is_rune_boundary(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u)) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_s = mmay_g_text_string(mmay_g_x, mmay_text_range_start(mmay_g_v_%u.data, (size_t)mmay_g_v_%u), (size_t)(mmay_g_v_%u - mmay_g_v_%u), &mmay_g_v_%u);",
                    instruction->third, instruction->second,
                    instruction->first, instruction->first, instruction->second,
                    instruction->first, instruction->first, instruction->third,
                    instruction->first, instruction->second,
                    instruction->third, instruction->second, instruction->target
                )) return false;
            return mmay_native_c_emit_text_status(buffer, instruction);
        case MMAY_MIR_TEXT_DROP_PREFIX:
        case MMAY_MIR_TEXT_TAKE_PREFIX:
            if (instruction->text_operation == MMAY_MIR_TEXT_DROP_PREFIX) {
                if (!mmay_native_c_write(
                        buffer,
                        "  if (!mmay_text_is_rune_boundary(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u)) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_s = mmay_g_text_string(mmay_g_x, mmay_text_range_start(mmay_g_v_%u.data, (size_t)mmay_g_v_%u), mmay_g_v_%u.length - (size_t)mmay_g_v_%u, &mmay_g_v_%u);",
                        instruction->first, instruction->first,
                        instruction->second, instruction->first,
                        instruction->second, instruction->first,
                        instruction->second, instruction->target
                    )) return false;
            } else if (!mmay_native_c_write(
                    buffer,
                    "  if (!mmay_text_is_rune_boundary(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u)) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_s = mmay_g_text_string(mmay_g_x, mmay_g_v_%u.data, (size_t)mmay_g_v_%u, &mmay_g_v_%u);",
                    instruction->first, instruction->first,
                    instruction->second, instruction->first,
                    instruction->second, instruction->target
                )) return false;
            if (!mmay_native_c_emit_text_status(buffer, instruction)) {
                return false;
            }
            return true;
        case MMAY_MIR_TEXT_CONTAINS:
        case MMAY_MIR_TEXT_STARTS_WITH:
        case MMAY_MIR_TEXT_ENDS_WITH:
            return mmay_native_c_write(
                buffer,
                "  mmay_g_v_%u = %s(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u.data, mmay_g_v_%u.length);\n",
                instruction->target,
                instruction->text_operation == MMAY_MIR_TEXT_CONTAINS
                    ? "mmay_text_contains"
                    : instruction->text_operation == MMAY_MIR_TEXT_STARTS_WITH
                        ? "mmay_text_starts_with" : "mmay_text_ends_with",
                instruction->first, instruction->first,
                instruction->second, instruction->second
            );
        case MMAY_MIR_TEXT_TRIM_ASCII:
            if (!mmay_native_c_write(
                    buffer,
                    "  { size_t mmay_g_text_start = 0, mmay_g_text_end = 0; mmay_text_trim_ascii_range(mmay_g_v_%u.data, mmay_g_v_%u.length, &mmay_g_text_start, &mmay_g_text_end); mmay_g_s = mmay_g_text_string(mmay_g_x, mmay_text_range_start(mmay_g_v_%u.data, mmay_g_text_start), mmay_g_text_end - mmay_g_text_start, &mmay_g_v_%u);",
                    instruction->first, instruction->first,
                    instruction->first, instruction->target
                ) || !mmay_native_c_emit_text_status(buffer, instruction)) {
                return false;
            }
            return mmay_native_c_write(buffer, "  }\n");
        case MMAY_MIR_TEXT_SPLIT_COUNT:
            return mmay_native_c_write(
                buffer,
                "  if (!mmay_text_split_count(mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u.data, mmay_g_v_%u.length, &mmay_g_v_%u)) { mmay_g_s = 7; goto mmay_g_cleanup; }\n",
                instruction->first, instruction->first,
                instruction->second, instruction->second,
                instruction->target
            );
        case MMAY_MIR_TEXT_SPLIT:
        case MMAY_MIR_TEXT_GRAPHEMES:
            if (descriptor == NULL) return false;
            if (instruction->text_operation == MMAY_MIR_TEXT_GRAPHEMES) {
                if (!mmay_native_c_write(
                    buffer,
                    "  mmay_g_s = mmay_g_text_string_array(mmay_g_x, mmay_g_v_%u, (mmay_g_string){NULL, 0, mmay_managed_invalid_reference()}, %s, true, &mmay_g_v_%u);",
                    instruction->first,
                    descriptor, instruction->target
                )) return false;
            } else if (!mmay_native_c_write(
                    buffer,
                    "  mmay_g_s = mmay_g_text_string_array(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, %s, false, &mmay_g_v_%u);",
                    instruction->first, instruction->second, descriptor,
                    instruction->target
                )) return false;
            if (!mmay_native_c_emit_text_status(buffer, instruction)) {
                return false;
            }
            return mmay_native_c_write(
                buffer,
                "  mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0};\n",
                instruction->target
            );
        case MMAY_MIR_TEXT_JOIN:
            if (!mmay_native_c_write(
                    buffer,
                    "  mmay_g_s = mmay_g_text_join(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, &mmay_g_v_%u);",
                    instruction->first, instruction->second,
                    instruction->target
                )) return false;
            return mmay_native_c_emit_text_status(buffer, instruction);
        case MMAY_MIR_TEXT_RUNES:
        case MMAY_MIR_TEXT_ENCODE_UTF8:
            if (descriptor == NULL || !mmay_native_c_write(
                    buffer,
                    "  mmay_g_s = mmay_g_text_raw_array(mmay_g_x, mmay_g_v_%u, %s, %s, &mmay_g_v_%u);",
                    instruction->first, descriptor,
                    instruction->text_operation == MMAY_MIR_TEXT_RUNES
                        ? "true" : "false",
                    instruction->target
                ) || !mmay_native_c_emit_text_status(buffer, instruction)) {
                return false;
            }
            return mmay_native_c_write(
                buffer,
                "  mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0};\n",
                instruction->target
            );
        case MMAY_MIR_TEXT_INVALID_UTF8_OFFSET:
        case MMAY_MIR_TEXT_DECODE_UTF8:
            if (instruction->text_operation
                    == MMAY_MIR_TEXT_INVALID_UTF8_OFFSET) {
                if (!mmay_native_c_write(
                    buffer,
                    "  { unsigned char *mmay_g_text_bytes = NULL; size_t mmay_g_text_length = 0, mmay_g_text_invalid = 0; mmay_g_s = mmay_g_text_array_bytes(mmay_g_x, mmay_g_v_%u, &mmay_g_text_bytes, &mmay_g_text_length); if (mmay_g_s == 0) mmay_g_v_%u = mmay_utf8_validate((const char *)mmay_g_text_bytes, mmay_g_text_length, &mmay_g_text_invalid) ? -1 : (int64_t)mmay_g_text_invalid; mmay_deallocate(&mmay_g_x->allocator, mmay_g_text_bytes, mmay_g_text_length);",
                    instruction->first, instruction->target
                )) return false;
            } else if (!mmay_native_c_write(
                    buffer,
                    "  { unsigned char *mmay_g_text_bytes = NULL; size_t mmay_g_text_length = 0, mmay_g_text_invalid = 0; mmay_g_s = mmay_g_text_array_bytes(mmay_g_x, mmay_g_v_%u, &mmay_g_text_bytes, &mmay_g_text_length); if (mmay_g_s == 0 && !mmay_utf8_validate((const char *)mmay_g_text_bytes, mmay_g_text_length, &mmay_g_text_invalid)) mmay_g_s = 7; if (mmay_g_s == 0) mmay_g_s = mmay_g_text_string(mmay_g_x, mmay_g_text_bytes, mmay_g_text_length, &mmay_g_v_%u); mmay_deallocate(&mmay_g_x->allocator, mmay_g_text_bytes, mmay_g_text_length);",
                    instruction->first, instruction->target
                )) return false;
            if (!mmay_native_c_emit_text_status(buffer, instruction)) {
                return false;
            }
            return mmay_native_c_write(buffer, "  }\n");
    }
    return false;
}
