#include "vm_internal.h"

#include "mmay/base/integer_text.h"
#include "mmay/base/number_format.h"

#include "mmay/runtime/string.h"

static bool mmay_bytecode_format_integer_kind(
    MarimayBytecodeType type,
    MarimayIntegerKind *kind
) {
    if (kind == NULL) return false;
    switch (type) {
        case MMAY_BYTECODE_TYPE_I8: *kind = MMAY_INTEGER_I8; return true;
        case MMAY_BYTECODE_TYPE_I16: *kind = MMAY_INTEGER_I16; return true;
        case MMAY_BYTECODE_TYPE_I32: *kind = MMAY_INTEGER_I32; return true;
        case MMAY_BYTECODE_TYPE_I64: *kind = MMAY_INTEGER_I64; return true;
        case MMAY_BYTECODE_TYPE_U8: *kind = MMAY_INTEGER_U8; return true;
        case MMAY_BYTECODE_TYPE_U16: *kind = MMAY_INTEGER_U16; return true;
        case MMAY_BYTECODE_TYPE_U32: *kind = MMAY_INTEGER_U32; return true;
        case MMAY_BYTECODE_TYPE_U64: *kind = MMAY_INTEGER_U64; return true;
        default: return false;
    }
}

MarimayBytecodeRunStatus mmay_bytecode_run_format_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution
) {
    uint32_t target = (uint32_t)instruction->operands[0].unsigned_value;
    uint32_t source = (uint32_t)instruction->operands[1].unsigned_value;
    char text[MMAY_INTEGER_TEXT_CAPACITY];
    size_t length = 0;
    if (instruction->opcode == MMAY_OPCODE_FORMAT_BOOL) {
        if (mmay_format_bool(
                registers[source].as.boolean,
                text, sizeof(text), &length
            ) != MMAY_INTEGER_TEXT_OK) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
    } else if (instruction->opcode == MMAY_OPCODE_FORMAT_INTEGER) {
        MarimayIntegerKind kind;
        if (!mmay_bytecode_format_integer_kind(registers[source].type, &kind)
            || mmay_format_integer(
                (MarimayIntegerValue){
                    kind, registers[source].as.integer_bits
                },
                10U, 1U, text, sizeof(text), &length
            ) != MMAY_INTEGER_TEXT_OK) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
    } else {
        MarimayNumberFormatStatus status = instruction->opcode
                == MMAY_OPCODE_FORMAT_F32
            ? mmay_format_f32(
            (uint32_t)registers[source].as.integer_bits,
            text,
            sizeof(text),
            &length
        )
            : mmay_format_f64(
                registers[source].as.f64_bits,
                text,
                sizeof(text),
                &length
            );
        if (status != MMAY_NUMBER_FORMAT_OK) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
    }
    return mmay_bytecode_execution_new_string(
        execution,
        (const unsigned char *)text,
        length,
        &registers[target].as.managed_reference
    );
}

MarimayBytecodeRunStatus mmay_bytecode_run_string_concat_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution
) {
    uint32_t target = (uint32_t)instruction->operands[0].unsigned_value;
    uint32_t left = (uint32_t)instruction->operands[1].unsigned_value;
    uint32_t right = (uint32_t)instruction->operands[2].unsigned_value;
    const unsigned char *left_bytes = NULL;
    const unsigned char *right_bytes = NULL;
    size_t left_length = 0;
    size_t right_length = 0;
    size_t length = 0;
    MarimayManagedReference result = mmay_managed_invalid_reference();
    MarimayManagedStatus status;
    if (!mmay_bytecode_execution_string_bytes(
            execution, registers[left], &left_bytes, &left_length
        ) || !mmay_bytecode_execution_string_bytes(
            execution, registers[right], &right_bytes, &right_length
        ) || !mmay_size_add(left_length, right_length, &length)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    status = mmay_managed_string_concat(
        &execution->managed_heap, left_bytes, left_length,
        right_bytes, right_length, &result
    );
    /* Keep the destination's input root until the transaction succeeds. */
    if (status == MMAY_MANAGED_OK)
        registers[target].as.managed_reference = result;
    return mmay_bytecode_run_managed_status(status);
}
