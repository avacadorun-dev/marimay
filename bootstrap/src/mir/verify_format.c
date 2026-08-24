#include "mir_internal.h"

static bool mmay_mir_format_integer(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64;
}

MarimayMirVerifyStatus mmay_mir_verify_format_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId target;
    MarimayTypeId first;
    MarimayTypeId second;
    if (function == NULL || instruction == NULL) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    target = mmay_mir_register_type(function, instruction->target);
    first = mmay_mir_register_type(function, instruction->first);
    second = mmay_mir_register_type(function, instruction->second);
    if (target == MMAY_TYPE_INVALID || first == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (target != MMAY_TYPE_STRING) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    switch (instruction->kind) {
        case MMAY_MIR_FORMAT_F32:
            return first == MMAY_TYPE_F32
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_FORMAT_F64:
            return first == MMAY_TYPE_F64
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_FORMAT_BOOL:
            return first == MMAY_TYPE_BOOL
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_FORMAT_INTEGER:
            return mmay_mir_format_integer(first)
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_STRING_CONCAT:
            if (second == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return first == MMAY_TYPE_STRING && second == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        default:
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
}
