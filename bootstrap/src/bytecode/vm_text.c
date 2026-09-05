#include "vm_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/text.h"
#include "mmay/base/utf8.h"
#include "mmay/runtime/string.h"

static MarimayBytecodeRunStatus mmay_vm_text_string(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value,
    const unsigned char **bytes,
    size_t *length
) {
    return mmay_bytecode_execution_string_bytes(
        execution, value, bytes, length
    ) ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
}

static MarimayBytecodeRunStatus mmay_vm_text_intern(
    MarimayBytecodeExecution *execution,
    const unsigned char *bytes,
    size_t length,
    MarimayBytecodeValue *target
) {
    return mmay_bytecode_execution_intern(
        execution, bytes, length, &target->as.reference
    );
}

static MarimayBytecodeRunStatus mmay_vm_text_new_array(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue *target,
    uint32_t layout,
    const MarimayManagedCell *cells,
    size_t length
) {
    const MarimayManagedArrayDescriptor *descriptor =
        mmay_bytecode_array_descriptor_for(
            execution, function, target->type, layout
        );
    if (descriptor == NULL) {
        return execution->array_descriptor_allocation_failed
            ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return mmay_bytecode_run_managed_status(mmay_managed_array_new(
        &execution->managed_heap, descriptor, cells, length,
        &target->as.managed_reference
    ));
}

static MarimayBytecodeRunStatus mmay_vm_text_raw_array(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodeValue *target,
    uint32_t layout,
    const unsigned char *text,
    size_t length,
    bool runes
) {
    size_t count = length;
    size_t cell_bytes = 0;
    MarimayManagedCell *cells = NULL;
    size_t index = 0;
    MarimayBytecodeRunStatus status;
    if (runes) {
        int64_t scalar_count = 0;
        if (!mmay_text_rune_count(text, length, &scalar_count)) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        count = (size_t)scalar_count;
    }
    if (!mmay_size_multiply(count, sizeof(*cells), &cell_bytes)) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    cells = cell_bytes == 0 ? NULL : mmay_allocate(&allocator, cell_bytes);
    if (cell_bytes != 0 && cells == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (runes) {
        size_t offset = 0;
        while (offset < length) {
            MarimayUtf8Scalar scalar;
            if (!mmay_utf8_decode(text + offset, length - offset, &scalar)) {
                mmay_deallocate(&allocator, cells, cell_bytes);
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            cells[index].f64_bits = scalar.value;
            index += 1U;
            offset += scalar.width;
        }
    } else {
        for (index = 0; index < length; index += 1U) {
            cells[index].f64_bits = text[index];
        }
    }
    status = mmay_vm_text_new_array(
        execution, function, target, layout, cells, count
    );
    mmay_deallocate(&allocator, cells, cell_bytes);
    return status;
}

static MarimayBytecodeRunStatus mmay_vm_text_string_array(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodeValue *target,
    uint32_t layout,
    const unsigned char *text,
    size_t text_length,
    const unsigned char *separator,
    size_t separator_length,
    bool graphemes
) {
    int64_t signed_count = 0;
    size_t count = 0;
    size_t cell_bytes = 0;
    size_t location_bytes = 0;
    MarimayManagedCell *cells = NULL;
    MarimayManagedReference **locations = NULL;
    MarimayManagedRootFrame roots = {0};
    MarimayManagedStatus managed_status;
    MarimayBytecodeRunStatus status;
    size_t index = 0;
    size_t start = 0;
    size_t end = 0;
    if (graphemes) {
        if (!mmay_text_grapheme_count(text, text_length, &signed_count)) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
    } else if (!mmay_text_split_count(
            text, text_length, separator, separator_length, &signed_count
        )) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    count = (size_t)signed_count;
    if (!mmay_size_multiply(count, sizeof(*cells), &cell_bytes)
        || !mmay_size_multiply(count, sizeof(*locations), &location_bytes)) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    cells = cell_bytes == 0 ? NULL : mmay_allocate(&allocator, cell_bytes);
    locations = location_bytes == 0
        ? NULL : mmay_allocate(&allocator, location_bytes);
    if ((cell_bytes != 0 && cells == NULL)
        || (location_bytes != 0 && locations == NULL)) {
        mmay_deallocate(&allocator, locations, location_bytes);
        mmay_deallocate(&allocator, cells, cell_bytes);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    for (index = 0; index < count; index += 1U) {
        cells[index].reference = mmay_managed_invalid_reference();
        locations[index] = &cells[index].reference;
    }
    managed_status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, count
    );
    for (index = 0; managed_status == MMAY_MANAGED_OK && index < count;
        index += 1U) {
        if (graphemes) {
            if (!mmay_text_next_grapheme(text, text_length, start, &end)) {
                managed_status = MMAY_MANAGED_INVALID_ARGUMENT;
                break;
            }
        } else {
            end = start;
            while (end <= text_length
                && separator_length <= text_length - end
                && memcmp(text + end, separator, separator_length) != 0) {
                end += 1U;
            }
            if (separator_length > text_length - end) end = text_length;
        }
        managed_status = mmay_managed_string_new(
            &execution->managed_heap, mmay_text_range_start(text, start), end - start,
            &cells[index].reference
        );
        start = graphemes ? end : end + separator_length;
    }
    status = managed_status == MMAY_MANAGED_OK
        ? mmay_vm_text_new_array(
            execution, function, target, layout, cells, count
        ) : mmay_bytecode_run_managed_status(managed_status);
    {
        MarimayManagedStatus pop = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (status == MMAY_BYTECODE_RUN_OK && pop != MMAY_MANAGED_OK) {
            status = mmay_bytecode_run_managed_status(pop);
        }
    }
    mmay_deallocate(&allocator, locations, location_bytes);
    mmay_deallocate(&allocator, cells, cell_bytes);
    return status;
}

static MarimayBytecodeRunStatus mmay_vm_text_array_bytes(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue array,
    MarimayAllocator allocator,
    unsigned char **bytes,
    size_t *length
) {
    MarimayManagedStatus managed_status;
    size_t index = 0;
    size_t byte_count = 0;
    unsigned char *copy = NULL;
    managed_status = mmay_managed_array_length(
        &execution->managed_heap, array.as.managed_reference, &byte_count
    );
    if (managed_status != MMAY_MANAGED_OK) {
        return mmay_bytecode_run_managed_status(managed_status);
    }
    copy = byte_count == 0 ? NULL : mmay_allocate(&allocator, byte_count);
    if (byte_count != 0 && copy == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    for (index = 0; index < byte_count; index += 1U) {
        MarimayManagedCell cell;
        size_t ignored = 0;
        managed_status = mmay_managed_array_get(
            &execution->managed_heap, array.as.managed_reference,
            (int64_t)index, &cell, &ignored
        );
        if (managed_status != MMAY_MANAGED_OK) {
            mmay_deallocate(&allocator, copy, byte_count);
            return mmay_bytecode_run_managed_status(managed_status);
        }
        copy[index] = (unsigned char)cell.f64_bits;
    }
    *bytes = copy;
    *length = byte_count;
    return MMAY_BYTECODE_RUN_OK;
}

static MarimayBytecodeRunStatus mmay_vm_text_join(
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator,
    MarimayBytecodeValue parts,
    const unsigned char *separator,
    size_t separator_length,
    MarimayBytecodeValue *target
) {
    MarimayManagedStatus managed_status;
    size_t count = 0;
    size_t total = 0;
    size_t index = 0;
    size_t offset = 0;
    unsigned char *buffer = NULL;
    managed_status = mmay_managed_array_length(
        &execution->managed_heap, parts.as.managed_reference, &count
    );
    if (managed_status != MMAY_MANAGED_OK) {
        return mmay_bytecode_run_managed_status(managed_status);
    }
    for (index = 0; index < count; index += 1U) {
        MarimayManagedCell cell;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        size_t ignored = 0;
        managed_status = mmay_managed_array_get(
            &execution->managed_heap, parts.as.managed_reference,
            (int64_t)index, &cell, &ignored
        );
        if (managed_status == MMAY_MANAGED_OK) {
            managed_status = mmay_managed_string_bytes(
                &execution->managed_heap, cell.reference, &bytes, &length
            );
        }
        if (managed_status != MMAY_MANAGED_OK
            || !mmay_size_add(total, length, &total)
            || (index != 0
                && !mmay_size_add(total, separator_length, &total))) {
            return managed_status != MMAY_MANAGED_OK
                ? mmay_bytecode_run_managed_status(managed_status)
                : MMAY_BYTECODE_RUN_LIMIT_REACHED;
        }
    }
    buffer = total == 0 ? NULL : mmay_allocate(&allocator, total);
    if (total != 0 && buffer == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    for (index = 0; index < count; index += 1U) {
        MarimayManagedCell cell;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        size_t ignored = 0;
        if (index != 0 && separator_length != 0) {
            memcpy(buffer + offset, separator, separator_length);
            offset += separator_length;
        }
        managed_status = mmay_managed_array_get(
            &execution->managed_heap, parts.as.managed_reference,
            (int64_t)index, &cell, &ignored
        );
        if (managed_status == MMAY_MANAGED_OK) {
            managed_status = mmay_managed_string_bytes(
                &execution->managed_heap, cell.reference, &bytes, &length
            );
        }
        if (managed_status != MMAY_MANAGED_OK) {
            mmay_deallocate(&allocator, buffer, total);
            return mmay_bytecode_run_managed_status(managed_status);
        }
        if (length != 0) memcpy(buffer + offset, bytes, length);
        offset += length;
    }
    {
        MarimayBytecodeRunStatus status = mmay_vm_text_intern(
            execution, buffer, total, target
        );
        mmay_deallocate(&allocator, buffer, total);
        return status;
    }
}

MarimayBytecodeRunStatus mmay_bytecode_run_text_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const MarimayIoStdout *stdout_sink
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    size_t first = (size_t)instruction->operands[1].unsigned_value;
    size_t second = (size_t)instruction->operands[2].unsigned_value;
    const unsigned char *left = NULL;
    const unsigned char *right = NULL;
    size_t left_length = 0;
    size_t right_length = 0;
    MarimayBytecodeRunStatus status;
    (void)stdout_sink;
    if (instruction->opcode == MMAY_OPCODE_TEXT_JOIN) {
        status = mmay_vm_text_string(
            execution, registers[second], &right, &right_length
        );
        return status == MMAY_BYTECODE_RUN_OK
            ? mmay_vm_text_join(
                execution, allocator, registers[first], right, right_length,
                &registers[target]
            ) : status;
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_INVALID_UTF8_OFFSET
        || instruction->opcode == MMAY_OPCODE_TEXT_DECODE_UTF8) {
        unsigned char *bytes = NULL;
        size_t length = 0;
        size_t invalid = 0;
        status = mmay_vm_text_array_bytes(
            execution, registers[first], allocator, &bytes, &length
        );
        if (status != MMAY_BYTECODE_RUN_OK) return status;
        if (instruction->opcode == MMAY_OPCODE_TEXT_INVALID_UTF8_OFFSET) {
            registers[target].as.i64 = mmay_utf8_validate(
                (const char *)bytes, length, &invalid
            ) ? -1 : (int64_t)invalid;
            status = MMAY_BYTECODE_RUN_OK;
        } else if (!mmay_utf8_validate((const char *)bytes, length, &invalid)) {
            status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        } else {
            status = mmay_vm_text_intern(
                execution, bytes, length, &registers[target]
            );
        }
        mmay_deallocate(&allocator, bytes, length);
        return status;
    }
    status = mmay_vm_text_string(
        execution, registers[first], &left, &left_length
    );
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    if (instruction->opcode == MMAY_OPCODE_TEXT_BYTE_LENGTH) {
        return mmay_text_byte_length(left_length, &registers[target].as.i64)
            ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_RUNE_COUNT) {
        return mmay_text_rune_count(
            left, left_length, &registers[target].as.i64
        ) ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_GRAPHEME_COUNT) {
        return mmay_text_grapheme_count(
            left, left_length, &registers[target].as.i64
        ) ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_TRIM_ASCII) {
        size_t start = 0;
        size_t end = 0;
        mmay_text_trim_ascii_range(left, left_length, &start, &end);
        return mmay_vm_text_intern(
            execution, mmay_text_range_start(left, start), end - start, &registers[target]
        );
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_RUNES
        || instruction->opcode == MMAY_OPCODE_TEXT_ENCODE_UTF8) {
        return mmay_vm_text_raw_array(
            execution, function, allocator, &registers[target],
            mmay_bytecode_register_layout(function, (uint32_t)target),
            left, left_length,
            instruction->opcode == MMAY_OPCODE_TEXT_RUNES
        );
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_GRAPHEMES) {
        return mmay_vm_text_string_array(
            execution, function, allocator, &registers[target],
            mmay_bytecode_register_layout(function, (uint32_t)target),
            left, left_length, NULL, 0, true
        );
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_IS_RUNE_BOUNDARY) {
        registers[target].as.boolean = mmay_text_is_rune_boundary(
            left, left_length, registers[second].as.i64
        );
        return MMAY_BYTECODE_RUN_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_TEXT_DROP_PREFIX
        || instruction->opcode == MMAY_OPCODE_TEXT_TAKE_PREFIX) {
        int64_t wire = registers[second].as.i64;
        if (!mmay_text_is_rune_boundary(left, left_length, wire)) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        return instruction->opcode == MMAY_OPCODE_TEXT_DROP_PREFIX
            ? mmay_vm_text_intern(
                execution, mmay_text_range_start(left, (size_t)wire),
                left_length - (size_t)wire,
                &registers[target]
            ) : mmay_vm_text_intern(
                execution, left, (size_t)wire, &registers[target]
            );
    }
    status = mmay_vm_text_string(
        execution, registers[second], &right, &right_length
    );
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    if (instruction->opcode == MMAY_OPCODE_TEXT_CONTAINS) {
        registers[target].as.boolean = mmay_text_contains(
            left, left_length, right, right_length
        );
    } else if (instruction->opcode == MMAY_OPCODE_TEXT_STARTS_WITH) {
        registers[target].as.boolean = mmay_text_starts_with(
            left, left_length, right, right_length
        );
    } else if (instruction->opcode == MMAY_OPCODE_TEXT_ENDS_WITH) {
        registers[target].as.boolean = mmay_text_ends_with(
            left, left_length, right, right_length
        );
    } else if (instruction->opcode == MMAY_OPCODE_TEXT_SPLIT_COUNT) {
        return mmay_text_split_count(
            left, left_length, right, right_length,
            &registers[target].as.i64
        ) ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    } else if (instruction->opcode == MMAY_OPCODE_TEXT_SPLIT) {
        return mmay_vm_text_string_array(
            execution, function, allocator, &registers[target],
            mmay_bytecode_register_layout(function, (uint32_t)target),
            left, left_length, right, right_length, false
        );
    } else {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return MMAY_BYTECODE_RUN_OK;
}
