#include "mir_internal.h"

#include "mmay/base/number_format.h"

MarimayMirInterpretStatus mmay_mir_execute_format_instruction(
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    char text[MMAY_NUMBER_FORMAT_F64_CAPACITY];
    size_t length = 0;
    MarimayNumberFormatStatus status;
    if (instruction == NULL || source == NULL || target == NULL
        || options.execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
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
    return mmay_mir_execution_new_string(
        options.execution,
        (const unsigned char *)text,
        length,
        &target->as.reference
    );
}
