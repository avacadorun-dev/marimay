#include "mir_internal.h"

#include "mmay/base/integer_text.h"
#include "mmay/base/number_format.h"

#include <string.h>

static bool mmay_mir_format_integer_kind(
    MarimayTypeId type,
    MarimayIntegerKind *kind
) {
    if (kind == NULL) {
        return false;
    }
    switch (type) {
        case MMAY_TYPE_I8: *kind = MMAY_INTEGER_I8; return true;
        case MMAY_TYPE_I16: *kind = MMAY_INTEGER_I16; return true;
        case MMAY_TYPE_I32: *kind = MMAY_INTEGER_I32; return true;
        case MMAY_TYPE_I64: *kind = MMAY_INTEGER_I64; return true;
        case MMAY_TYPE_U8: *kind = MMAY_INTEGER_U8; return true;
        case MMAY_TYPE_U16: *kind = MMAY_INTEGER_U16; return true;
        case MMAY_TYPE_U32: *kind = MMAY_INTEGER_U32; return true;
        case MMAY_TYPE_U64: *kind = MMAY_INTEGER_U64; return true;
        default: return false;
    }
}

MarimayMirInterpretStatus mmay_mir_execute_format_instruction(
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    char text[MMAY_INTEGER_TEXT_CAPACITY];
    size_t length = 0;
    MarimayNumberFormatStatus status;
    if (instruction == NULL || source == NULL || target == NULL
        || options.execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (instruction->kind == MMAY_MIR_FORMAT_BOOL) {
        MarimayIntegerTextStatus bool_status = mmay_format_bool(
            source->as.boolean, text, sizeof(text), &length
        );
        if (bool_status != MMAY_INTEGER_TEXT_OK) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
    } else if (instruction->kind == MMAY_MIR_FORMAT_INTEGER) {
        MarimayIntegerKind kind;
        MarimayIntegerTextStatus integer_status;
        if (!mmay_mir_format_integer_kind(source->type, &kind)) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        integer_status = mmay_format_integer(
            (MarimayIntegerValue){kind, source->as.integer_bits},
            10U, 1U, text, sizeof(text), &length
        );
        if (integer_status != MMAY_INTEGER_TEXT_OK) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
    } else {
        status = instruction->kind == MMAY_MIR_FORMAT_F32
            ? mmay_format_f32(
            (uint32_t)source->as.integer_bits,
            text,
            sizeof(text),
            &length
        )
            : mmay_format_f64(
                source->as.f64_bits,
                text,
                sizeof(text),
                &length
            );
        if (status != MMAY_NUMBER_FORMAT_OK) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
    }
    return mmay_mir_execution_new_string(
        options.execution,
        (const unsigned char *)text,
        length,
        &target->as.reference
    );
}

MarimayMirInterpretStatus mmay_mir_execute_string_concat_instruction(
    const MarimayMirValue *left,
    const MarimayMirValue *right,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    const unsigned char *left_bytes = NULL;
    const unsigned char *right_bytes = NULL;
    size_t left_length = 0;
    size_t right_length = 0;
    size_t length = 0;
    unsigned char *joined = NULL;
    MarimayMirInterpretStatus status;
    if (left == NULL || right == NULL || target == NULL
        || options.execution == NULL
        || !mmay_mir_execution_string_bytes(
            options.execution, *left, &left_bytes, &left_length
        ) || !mmay_mir_execution_string_bytes(
            options.execution, *right, &right_bytes, &right_length
        ) || !mmay_size_add(left_length, right_length, &length)) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (length != 0) {
        joined = mmay_allocate(&options.execution->allocator, length);
        if (joined == NULL) {
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        if (left_length != 0) memcpy(joined, left_bytes, left_length);
        if (right_length != 0) {
            memcpy(joined + left_length, right_bytes, right_length);
        }
    }
    status = mmay_mir_execution_new_string(
        options.execution, joined, length, &target->as.reference
    );
    if (joined != NULL) {
        mmay_deallocate(&options.execution->allocator, joined, length);
    }
    return status;
}
