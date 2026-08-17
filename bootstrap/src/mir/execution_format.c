#include "mir_internal.h"

#include "ruda/base/number_format.h"

RudaMirInterpretStatus ruda_mir_execute_format_instruction(
    const RudaMirInstruction *instruction,
    const RudaMirValue *source,
    RudaMirValue *target,
    RudaMirInterpretOptions options
) {
    char text[RUDA_NUMBER_FORMAT_F64_CAPACITY];
    size_t length = 0;
    RudaNumberFormatStatus status;
    if (instruction == NULL || source == NULL || target == NULL
        || options.execution == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    status = instruction->kind == RUDA_MIR_FORMAT_F32
        ? ruda_format_f32(
            (uint32_t)source->as.integer_bits,
            text,
            sizeof(text),
            &length
        )
        : ruda_format_f64(
            source->as.f64_bits,
            text,
            sizeof(text),
            &length
        );
    if (status != RUDA_NUMBER_FORMAT_OK) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    return ruda_mir_execution_new_string(
        options.execution,
        (const unsigned char *)text,
        length,
        &target->as.reference
    );
}
