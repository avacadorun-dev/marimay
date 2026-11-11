#include "mir_internal.h"
#include "text_internal.h"

#include <string.h>

#include "mmay/base/text.h"
#include "mmay/base/utf8.h"

static MarimayMirInterpretStatus mmay_mir_text_bytes(
    MarimayMirInterpretOptions options,
    const MarimayMirValue *value,
    const unsigned char **bytes,
    size_t *length
) {
    return value != NULL && mmay_mir_execution_string_bytes(
        options.execution, *value, bytes, length
    ) ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_INVALID_REFERENCE;
}

static MarimayMirInterpretStatus mmay_mir_text_string(
    MarimayMirInterpretOptions options,
    const unsigned char *bytes,
    size_t length,
    MarimayMirValue *target
) {
    return mmay_mir_execution_new_string(
        options.execution, bytes, length, &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_text_new_array(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirInterpretOptions options,
    const MarimayMirValue *elements,
    size_t length,
    MarimayMirValue *target
) {
    const MarimayMirArray *layout = mmay_mir_array(
        function, instruction->array
    );
    return layout == NULL ? MMAY_MIR_INTERPRET_INVALID_MIR
        : mmay_mir_execution_new_array(
            options.execution, layout, elements, length,
            &target->as.reference
        );
}

static MarimayMirInterpretStatus mmay_mir_text_raw_array(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirInterpretOptions options,
    const unsigned char *bytes,
    size_t length,
    bool runes,
    MarimayMirValue *target
) {
    int64_t signed_count = 0;
    size_t count = length;
    size_t allocation = 0;
    size_t index = 0;
    MarimayMirValue *elements = NULL;
    MarimayMirInterpretStatus status;
    if (runes && !mmay_text_rune_count(bytes, length, &signed_count)) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (runes) count = (size_t)signed_count;
    if (!mmay_size_multiply(count, sizeof(*elements), &allocation)) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    elements = allocation == 0 ? NULL
        : mmay_allocate(&options.execution->allocator, allocation);
    if (allocation != 0 && elements == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (runes) {
        size_t offset = 0;
        while (offset < length) {
            MarimayUtf8Scalar scalar;
            if (!mmay_utf8_decode(bytes + offset, length - offset, &scalar)) {
                mmay_deallocate(
                    &options.execution->allocator, elements, allocation
                );
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            elements[index].type = MMAY_TYPE_RUNE;
            elements[index].as.integer_bits = scalar.value;
            index += 1U;
            offset += scalar.width;
        }
    } else {
        for (index = 0; index < length; index += 1U) {
            elements[index].type = MMAY_TYPE_U8;
            elements[index].as.integer_bits = bytes[index];
        }
    }
    status = mmay_mir_text_new_array(
        function, instruction, options, elements, count, target
    );
    mmay_deallocate(&options.execution->allocator, elements, allocation);
    return status;
}

static MarimayMirInterpretStatus mmay_mir_text_string_array(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirInterpretOptions options,
    const unsigned char *text,
    size_t text_length,
    const unsigned char *separator,
    size_t separator_length,
    bool graphemes,
    MarimayMirValue *target
) {
    int64_t signed_count = 0;
    size_t count = 0;
    size_t allocation = 0;
    size_t index = 0;
    size_t start = 0;
    MarimayMirValue *elements = NULL;
    MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_OK;
    if (graphemes) {
        if (!mmay_text_grapheme_count(text, text_length, &signed_count)) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
    } else if (!mmay_text_split_count(
            text, text_length, separator, separator_length, &signed_count
        )) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    count = (size_t)signed_count;
    if (!mmay_size_multiply(count, sizeof(*elements), &allocation)) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    elements = allocation == 0 ? NULL
        : mmay_allocate(&options.execution->allocator, allocation);
    if (allocation != 0 && elements == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (allocation != 0) memset(elements, 0, allocation);
    for (index = 0; index < count; index += 1U) {
        size_t end = start;
        elements[index].type = MMAY_TYPE_STRING;
        if (graphemes) {
            if (!mmay_text_next_grapheme(text, text_length, start, &end)) {
                status = MMAY_MIR_INTERPRET_INVALID_MIR;
                break;
            }
        } else {
            mmay_text_find_separator(text, text_length, start, separator,
                separator_length, &end);
        }
        status = mmay_mir_execution_new_string(
            options.execution, mmay_text_range_start(text, start), end - start,
            &elements[index].as.reference
        );
        if (status != MMAY_MIR_INTERPRET_OK) break;
        start = graphemes ? end : end + separator_length;
    }
    if (status == MMAY_MIR_INTERPRET_OK) {
        status = mmay_mir_text_new_array(
            function, instruction, options, elements, count, target
        );
    }
    mmay_deallocate(&options.execution->allocator, elements, allocation);
    return status;
}

static MarimayMirInterpretStatus mmay_mir_text_array_bytes(
    MarimayMirInterpretOptions options,
    const MarimayMirValue *array,
    unsigned char **bytes,
    size_t *length
) {
    size_t count = 0;
    size_t index = 0;
    unsigned char *copy = NULL;
    if (!mmay_mir_execution_array_length(options.execution, *array, &count)) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    copy = count == 0 ? NULL
        : mmay_allocate(&options.execution->allocator, count);
    if (count != 0 && copy == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    for (index = 0; index < count; index += 1U) {
        MarimayMirValue value;
        if (!mmay_mir_execution_array_element(
                options.execution, *array, index, &value
            ) || value.type != MMAY_TYPE_U8) {
            mmay_deallocate(&options.execution->allocator, copy, count);
            return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
        }
        copy[index] = (unsigned char)value.as.integer_bits;
    }
    *bytes = copy;
    *length = count;
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_mir_text_join(
    MarimayMirInterpretOptions options,
    const MarimayMirValue *parts,
    const unsigned char *separator,
    size_t separator_length,
    MarimayMirValue *target
) {
    size_t count = 0;
    size_t total = 0;
    size_t index = 0;
    size_t offset = 0;
    unsigned char *joined = NULL;
    if (!mmay_mir_execution_array_length(options.execution, *parts, &count)) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    for (index = 0; index < count; index += 1U) {
        MarimayMirValue part;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        if (!mmay_mir_execution_array_element(
                options.execution, *parts, index, &part
            ) || !mmay_mir_execution_string_bytes(
                options.execution, part, &bytes, &length
            ) || !mmay_size_add(total, length, &total)
            || (index != 0
                && !mmay_size_add(total, separator_length, &total))) {
            return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        }
    }
    joined = total == 0 ? NULL
        : mmay_allocate(&options.execution->allocator, total);
    if (total != 0 && joined == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    for (index = 0; index < count; index += 1U) {
        MarimayMirValue part;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        (void)mmay_mir_execution_array_element(
            options.execution, *parts, index, &part
        );
        (void)mmay_mir_execution_string_bytes(
            options.execution, part, &bytes, &length
        );
        if (index != 0 && separator_length != 0) {
            memcpy(joined + offset, separator, separator_length);
            offset += separator_length;
        }
        if (length != 0) memcpy(joined + offset, bytes, length);
        offset += length;
    }
    {
        MarimayMirInterpretStatus status = mmay_mir_text_string(
            options, joined, total, target
        );
        mmay_deallocate(&options.execution->allocator, joined, total);
        return status;
    }
}

MarimayMirInterpretStatus mmay_mir_execute_text_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    const MarimayMirValue *third,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    const unsigned char *left = NULL;
    const unsigned char *right = NULL;
    size_t left_length = 0;
    size_t right_length = 0;
    MarimayMirInterpretStatus status;
    (void)allocator;
    if (function == NULL || instruction == NULL || target == NULL
        || options.execution == NULL) return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    if (instruction->text_operation == MMAY_MIR_TEXT_JOIN) {
        status = mmay_mir_text_bytes(
            options, second, &right, &right_length
        );
        return status == MMAY_MIR_INTERPRET_OK
            ? mmay_mir_text_join(
                options, first, right, right_length, target
            ) : status;
    }
    if (instruction->text_operation == MMAY_MIR_TEXT_INVALID_UTF8_OFFSET
        || instruction->text_operation == MMAY_MIR_TEXT_DECODE_UTF8) {
        unsigned char *bytes = NULL;
        size_t length = 0;
        size_t invalid = 0;
        status = mmay_mir_text_array_bytes(options, first, &bytes, &length);
        if (status != MMAY_MIR_INTERPRET_OK) return status;
        if (instruction->text_operation
                == MMAY_MIR_TEXT_INVALID_UTF8_OFFSET) {
            target->as.i64 = mmay_utf8_validate(
                (const char *)bytes, length, &invalid
            ) ? -1 : (int64_t)invalid;
            status = MMAY_MIR_INTERPRET_OK;
        } else if (!mmay_utf8_validate((const char *)bytes, length, &invalid)) {
            status = MMAY_MIR_INTERPRET_INVALID_MIR;
        } else {
            status = mmay_mir_text_string(options, bytes, length, target);
        }
        mmay_deallocate(&options.execution->allocator, bytes, length);
        return status;
    }
    status = mmay_mir_text_bytes(options, first, &left, &left_length);
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    switch (instruction->text_operation) {
        case MMAY_MIR_TEXT_BYTE_LENGTH:
            return mmay_text_byte_length(left_length, &target->as.i64)
                ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        case MMAY_MIR_TEXT_RUNE_COUNT:
            return mmay_text_rune_count(left, left_length, &target->as.i64)
                ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_INVALID_MIR;
        case MMAY_MIR_TEXT_GRAPHEME_COUNT:
            return mmay_text_grapheme_count(
                left, left_length, &target->as.i64
            ) ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_INVALID_MIR;
        case MMAY_MIR_TEXT_IS_RUNE_BOUNDARY:
            target->as.boolean = mmay_text_is_rune_boundary(
                left, left_length, second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_TEXT_SUBSTRING:
            if (third->as.i64 < second->as.i64
                || !mmay_text_is_rune_boundary(left, left_length, second->as.i64)
                || !mmay_text_is_rune_boundary(left, left_length, third->as.i64)) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            return mmay_mir_text_string(options,
                mmay_text_range_start(left, (size_t)second->as.i64),
                (size_t)(third->as.i64 - second->as.i64), target);
        case MMAY_MIR_TEXT_DROP_PREFIX:
        case MMAY_MIR_TEXT_TAKE_PREFIX: {
            int64_t offset = second->as.i64;
            if (!mmay_text_is_rune_boundary(left, left_length, offset)) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            return instruction->text_operation == MMAY_MIR_TEXT_DROP_PREFIX
                ? mmay_mir_text_string(
                    options, mmay_text_range_start(left, (size_t)offset),
                    left_length - (size_t)offset, target
                ) : mmay_mir_text_string(
                    options, left, (size_t)offset, target
                );
        }
        case MMAY_MIR_TEXT_TRIM_ASCII: {
            size_t start = 0;
            size_t end = 0;
            mmay_text_trim_ascii_range(left, left_length, &start, &end);
            return mmay_mir_text_string(
                options, mmay_text_range_start(left, start), end - start, target
            );
        }
        case MMAY_MIR_TEXT_RUNES:
        case MMAY_MIR_TEXT_ENCODE_UTF8:
            return mmay_mir_text_raw_array(
                function, instruction, options, left, left_length,
                instruction->text_operation == MMAY_MIR_TEXT_RUNES, target
            );
        case MMAY_MIR_TEXT_GRAPHEMES:
            return mmay_mir_text_string_array(
                function, instruction, options, left, left_length,
                NULL, 0, true, target
            );
        case MMAY_MIR_TEXT_CONTAINS:
        case MMAY_MIR_TEXT_STARTS_WITH:
        case MMAY_MIR_TEXT_ENDS_WITH:
        case MMAY_MIR_TEXT_SPLIT_COUNT:
        case MMAY_MIR_TEXT_SPLIT:
            status = mmay_mir_text_bytes(
                options, second, &right, &right_length
            );
            if (status != MMAY_MIR_INTERPRET_OK) return status;
            if (instruction->text_operation == MMAY_MIR_TEXT_CONTAINS) {
                target->as.boolean = mmay_text_contains(
                    left, left_length, right, right_length
                );
            } else if (instruction->text_operation
                    == MMAY_MIR_TEXT_STARTS_WITH) {
                target->as.boolean = mmay_text_starts_with(
                    left, left_length, right, right_length
                );
            } else if (instruction->text_operation
                    == MMAY_MIR_TEXT_ENDS_WITH) {
                target->as.boolean = mmay_text_ends_with(
                    left, left_length, right, right_length
                );
            } else if (instruction->text_operation
                    == MMAY_MIR_TEXT_SPLIT_COUNT) {
                return mmay_text_split_count(
                    left, left_length, right, right_length, &target->as.i64
                ) ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_INVALID_MIR;
            } else {
                return mmay_mir_text_string_array(
                    function, instruction, options, left, left_length,
                    right, right_length, false, target
                );
            }
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_TEXT_JOIN:
        case MMAY_MIR_TEXT_INVALID_UTF8_OFFSET:
        case MMAY_MIR_TEXT_DECODE_UTF8:
            return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    return MMAY_MIR_INTERPRET_INVALID_MIR;
}
