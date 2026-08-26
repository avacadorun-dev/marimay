#include "mir_internal.h"
#include "text_internal.h"
#include "mmay/base/checked_i64.h"
#include "mmay/base/checked_integer.h"
#include "mmay/base/f64.h"
#include "mmay/base/map.h"
#include "mmay/base/utf8.h"
#include <limits.h>
#include <string.h>
typedef enum MarimayMirIntegerBinaryOperation {
    MMAY_MIR_INTEGER_ADD,
    MMAY_MIR_INTEGER_SUBTRACT,
    MMAY_MIR_INTEGER_MULTIPLY,
    MMAY_MIR_INTEGER_DIVIDE,
    MMAY_MIR_INTEGER_REMAINDER,
    MMAY_MIR_INTEGER_SHIFT_LEFT,
    MMAY_MIR_INTEGER_SHIFT_RIGHT
} MarimayMirIntegerBinaryOperation;
static bool mmay_mir_integer_kind(
    MarimayTypeId type,
    MarimayIntegerKind *result
) {
    if (result == NULL) {
        return false;
    }
    if (type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_I64) {
        *result = (MarimayIntegerKind)(type - MMAY_TYPE_I8);
        return true;
    }
    if (type >= MMAY_TYPE_U8 && type <= MMAY_TYPE_U64) {
        *result = (MarimayIntegerKind)(type - MMAY_TYPE_U8 + MMAY_INTEGER_U8);
        return true;
    }
    return false;
}
static uint64_t mmay_mir_integer_mask(MarimayIntegerKind kind) {
    unsigned int width = mmay_integer_bit_width(kind);
    return width == 64U ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}
static MarimayIntegerValue mmay_mir_integer_value(const MarimayMirValue *value) {
    MarimayIntegerKind kind = MMAY_INTEGER_I64;
    MarimayIntegerValue result;
    (void)mmay_mir_integer_kind(value->type, &kind);
    result.kind = kind;
    result.bits = value->as.integer_bits & mmay_mir_integer_mask(kind);
    return result;
}
static MarimayMirInterpretStatus mmay_mir_integer_status(
    MarimayCheckedIntegerStatus status
) {
    switch (status) {
        case MMAY_CHECKED_INTEGER_OK:
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_CHECKED_INTEGER_OVERFLOW:
            return MMAY_MIR_INTERPRET_OVERFLOW;
        case MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO:
            return MMAY_MIR_INTERPRET_DIVISION_BY_ZERO;
        case MMAY_CHECKED_INTEGER_INVALID_SHIFT:
            return MMAY_MIR_INTERPRET_INVALID_SHIFT;
    }
    return MMAY_MIR_INTERPRET_INVALID_MIR;
}

static MarimayMirInterpretStatus mmay_mir_integer_binary(
    MarimayMirIntegerBinaryOperation operation,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    MarimayMirValue *target
) {
    MarimayIntegerValue left = mmay_mir_integer_value(first);
    MarimayIntegerValue right = mmay_mir_integer_value(second);
    MarimayIntegerValue output = {left.kind, 0};
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OVERFLOW;
    switch (operation) {
        case MMAY_MIR_INTEGER_ADD:
            status = mmay_checked_integer_add(left, right, &output);
            break;
        case MMAY_MIR_INTEGER_SUBTRACT:
            status = mmay_checked_integer_subtract(left, right, &output);
            break;
        case MMAY_MIR_INTEGER_MULTIPLY:
            status = mmay_checked_integer_multiply(left, right, &output);
            break;
        case MMAY_MIR_INTEGER_DIVIDE:
            status = mmay_checked_integer_divide(left, right, &output);
            break;
        case MMAY_MIR_INTEGER_REMAINDER:
            status = mmay_checked_integer_remainder(left, right, &output);
            break;
        case MMAY_MIR_INTEGER_SHIFT_LEFT:
            status = mmay_checked_integer_shift_left(left, right, &output);
            break;
        case MMAY_MIR_INTEGER_SHIFT_RIGHT:
            status = mmay_checked_integer_shift_right(left, right, &output);
            break;
    }
    if (status == MMAY_CHECKED_INTEGER_OK) {
        target->as.integer_bits = output.bits;
    }
    return mmay_mir_integer_status(status);
}

static MarimayMirInterpretStatus mmay_mir_integer_negate(
    const MarimayMirValue *first,
    MarimayMirValue *target
) {
    MarimayIntegerValue input = mmay_mir_integer_value(first);
    MarimayIntegerValue output = {input.kind, 0};
    MarimayCheckedIntegerStatus status = mmay_checked_integer_negate(
        input, &output
    );
    if (status == MMAY_CHECKED_INTEGER_OK) {
        target->as.integer_bits = output.bits;
    }
    return mmay_mir_integer_status(status);
}

static void mmay_mir_integer_bitwise(
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    MarimayMirValue *target,
    unsigned int operation
) {
    MarimayIntegerValue left = mmay_mir_integer_value(first);
    uint64_t bits = left.bits;
    uint64_t mask = mmay_mir_integer_mask(left.kind);
    if (second != NULL) {
        uint64_t right = mmay_mir_integer_value(second).bits;
        bits = operation == 0U ? left.bits & right
            : operation == 1U ? left.bits ^ right : left.bits | right;
    } else {
        bits = ~bits;
    }
    target->as.integer_bits = bits & mask;
}

static bool mmay_mir_integer_less(
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    bool equal_allowed
) {
    MarimayIntegerValue left = mmay_mir_integer_value(first);
    MarimayIntegerValue right = mmay_mir_integer_value(second);
    if (mmay_integer_kind_is_signed(left.kind)) {
        int64_t left_value = 0;
        int64_t right_value = 0;
        (void)mmay_integer_to_i64(left, &left_value);
        (void)mmay_integer_to_i64(right, &right_value);
        return equal_allowed ? left_value <= right_value
            : left_value < right_value;
    }
    {
        uint64_t left_value = 0;
        uint64_t right_value = 0;
        (void)mmay_integer_to_u64(left, &left_value);
        (void)mmay_integer_to_u64(right, &right_value);
        return equal_allowed ? left_value <= right_value
            : left_value < right_value;
    }
}
MarimayMirInterpretOptions mmay_mir_interpret_default_options(void) {
    MarimayMirInterpretOptions options;
    options.max_steps = 10000000;
    options.remaining_shared_steps = NULL;
    options.call_context = NULL;
    options.call = NULL;
    options.execution = NULL; options.stdout_sink = NULL;
    options.panic = NULL;
    return options;
}
static bool mmay_mir_uses_f64(const MarimayMirFunction *function) {
    size_t index = 0;
    if (function == NULL || function->result_type == MMAY_TYPE_F64) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && *type == MMAY_TYPE_F64) {
            return true;
        }
    }
    return false;
}
MarimayMirInterpretStatus mmay_mir_invoke_call(
    const MarimayMirFunction *function,
    MarimayMirCallId call_id,
    const MarimayMirValue *registers,
    size_t register_count,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options,
    const MarimaySymbolId *selected_callee,
    const MarimayMirValue *selected_receiver,
    MarimayMirValue *result
) {
    const MarimayMirCall *call = mmay_mir_call(function, call_id);
    MarimayMirValue *arguments = NULL;
    size_t argument_bytes = 0;
    size_t argument_end = 0;
    size_t index = 0;
    MarimayMirInterpretStatus status;
    if (function == NULL || registers == NULL || allocator == NULL
        || result == NULL || call == NULL || options.call == NULL
        || ((selected_callee == NULL) != (selected_receiver == NULL))
        || (selected_receiver != NULL && call->argument_count == 0)
        || !mmay_size_add(
            call->argument_start,
            call->argument_count,
            &argument_end
        ) || argument_end > function->call_arguments.length
        || !mmay_size_multiply(
            call->argument_count,
            sizeof(MarimayMirValue),
            &argument_bytes
        )) {
        return options.call == NULL
            ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
            : MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (argument_bytes != 0) {
        arguments = mmay_allocate(allocator, argument_bytes);
        if (arguments == NULL) {
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
    }
    if (call->argument_count != 0 && arguments == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    for (index = 0; index < call->argument_count; index += 1) {
        const MarimayMirCallArgument *argument = mmay_mir_call_argument(
            function,
            call->argument_start + index
        );
        if (argument == NULL || argument->register_id >= register_count) {
            mmay_deallocate(allocator, arguments, argument_bytes);
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        arguments[index] = index == 0 && selected_receiver != NULL
            ? *selected_receiver
            : registers[argument->register_id];
    }
    status = options.call(
        options.call_context,
        selected_callee != NULL ? *selected_callee : call->callee,
        arguments,
        call->argument_count,
        result
    );
    mmay_deallocate(allocator, arguments, argument_bytes);
    return status;
}
static MarimayMirInterpretStatus mmay_mir_execute_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirValue *registers,
    size_t register_count,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    MarimayMirValue *target = NULL;
    const MarimayMirValue *first = NULL;
    const MarimayMirValue *second = NULL;
    const MarimayMirValue *third = NULL;
    if (function == NULL || instruction == NULL || registers == NULL
        || allocator == NULL
        || instruction->target >= register_count) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    target = &registers[instruction->target];
    first = target;
    second = target;
    third = target;
    if (mmay_mir_instruction_uses_first(instruction->kind)) {
        if (instruction->first >= register_count) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        first = &registers[instruction->first];
    }
    if (mmay_mir_instruction_uses_second(instruction->kind)) {
        if (instruction->second >= register_count) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        second = &registers[instruction->second];
    }
    if (mmay_mir_instruction_uses_third(instruction->kind)) {
        if (instruction->third >= register_count) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        third = &registers[instruction->third];
    }
    switch (instruction->kind) {
        case MMAY_MIR_INTEGER_POLICY:
        case MMAY_MIR_INTEGER_CONVERT:
            return mmay_mir_execute_numeric_instruction(
                function, instruction, first, second, target, options
            );
        case MMAY_MIR_FORMAT_F32:
        case MMAY_MIR_FORMAT_F64:
        case MMAY_MIR_FORMAT_BOOL:
        case MMAY_MIR_FORMAT_INTEGER:
            return mmay_mir_execute_format_instruction(
                instruction, first, target, options
            );
        case MMAY_MIR_STRING_CONCAT:
            return mmay_mir_execute_string_concat_instruction(
                first, second, target, options
            );
        case MMAY_MIR_PARSE_INTEGER:
            return mmay_mir_execute_parse_instruction(
                function, instruction, first, second, target, options
            );
        case MMAY_MIR_TEXT_UNARY:
        case MMAY_MIR_TEXT_BINARY:
            return mmay_mir_execute_text_instruction(
                function, instruction, first, second, target, allocator,
                options
            );
        case MMAY_MIR_IO_PRINTLN:
        case MMAY_MIR_IO_WRITE:
        case MMAY_MIR_IO_FLUSH:
        case MMAY_MIR_IO_READ_LINE:
        case MMAY_MIR_IO_READ_BYTES:
        case MMAY_MIR_IO_WRITE_BYTES:
            return mmay_mir_execute_io_instruction(function, instruction,
                first, target, allocator, options);
        case MMAY_MIR_TEST_OUTPUT_BEGIN:
        case MMAY_MIR_TEST_OUTPUT_END:
            return mmay_mir_execute_test_output_instruction(
                function, instruction, first, target, allocator, options
            );
        case MMAY_MIR_CONSTANT_I64:
            target->as.i64 = instruction->integer;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_CONSTANT_INTEGER:
            target->as.integer_bits = instruction->integer_bits;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_CONSTANT_F64:
            target->as.f64_bits = instruction->f64_bits;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_CONSTANT_BOOL:
            target->as.boolean = instruction->boolean;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_CONSTANT_STRING: {
            const MarimayMirStringConstant *constant = mmay_mir_string_constant(
                function,
                instruction->string_constant
            );
            if (constant == NULL || options.execution == NULL) {
                return options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            return mmay_mir_execution_new_string(
                options.execution,
                constant->value.data,
                constant->value.length,
                &target->as.reference
            );
        }
        case MMAY_MIR_COPY:
            *target = *first;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_ADD_PROVEN_I64:
            target->as.i64 = mmay_i64_add_proven(
                first->as.i64, second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
            target->as.i64 = mmay_i64_subtract_proven(
                first->as.i64, second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
            target->as.i64 = mmay_i64_multiply_proven(
                first->as.i64, second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_ADD_CHECKED_I64:
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_ADD, first, second, target
                );
            }
            return mmay_checked_i64_add(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            ) == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_SUBTRACT, first, second, target
                );
            }
            return mmay_checked_i64_subtract(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            ) == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_MULTIPLY, first, second, target
                );
            }
            return mmay_checked_i64_multiply(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            ) == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        case MMAY_MIR_DIVIDE_CHECKED_I64: {
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_DIVIDE, first, second, target
                );
            }
            MarimayCheckedI64Status checked = mmay_checked_i64_divide(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == MMAY_CHECKED_I64_DIVISION_BY_ZERO) {
                return MMAY_MIR_INTERPRET_DIVISION_BY_ZERO;
            }
            return checked == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        }
        case MMAY_MIR_REMAINDER_CHECKED_I64: {
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_REMAINDER, first, second, target
                );
            }
            MarimayCheckedI64Status checked = mmay_checked_i64_remainder(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == MMAY_CHECKED_I64_DIVISION_BY_ZERO) {
                return MMAY_MIR_INTERPRET_DIVISION_BY_ZERO;
            }
            return checked == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        }
        case MMAY_MIR_NEGATE_CHECKED_I64:
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_negate(first, target);
            }
            return mmay_checked_i64_negate(
                first->as.i64,
                &target->as.i64
            ) == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        case MMAY_MIR_ADD_F64:
            target->as.f64_bits = mmay_f64_add(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_SUBTRACT_F64:
            target->as.f64_bits = mmay_f64_subtract(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_MULTIPLY_F64:
            target->as.f64_bits = mmay_f64_multiply(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_DIVIDE_F64:
            target->as.f64_bits = mmay_f64_divide(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_NEGATE_F64:
            target->as.f64_bits = mmay_f64_negate(first->as.f64_bits);
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_NOT_BOOL:
            target->as.boolean = !first->as.boolean;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_BITWISE_NOT_I64:
            if (first->type != MMAY_TYPE_I64) {
                mmay_mir_integer_bitwise(first, NULL, target, 0U);
                return MMAY_MIR_INTERPRET_OK;
            }
            target->as.i64 = mmay_i64_bitwise_not(first->as.i64);
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_BITWISE_AND_I64:
            if (first->type != MMAY_TYPE_I64) {
                mmay_mir_integer_bitwise(first, second, target, 0U);
                return MMAY_MIR_INTERPRET_OK;
            }
            target->as.i64 = mmay_i64_bitwise_and(
                first->as.i64,
                second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_BITWISE_XOR_I64:
            if (first->type != MMAY_TYPE_I64) {
                mmay_mir_integer_bitwise(first, second, target, 1U);
                return MMAY_MIR_INTERPRET_OK;
            }
            target->as.i64 = mmay_i64_bitwise_xor(
                first->as.i64,
                second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_BITWISE_OR_I64:
            if (first->type != MMAY_TYPE_I64) {
                mmay_mir_integer_bitwise(first, second, target, 2U);
                return MMAY_MIR_INTERPRET_OK;
            }
            target->as.i64 = mmay_i64_bitwise_or(
                first->as.i64,
                second->as.i64
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64: {
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_SHIFT_LEFT, first, second, target
                );
            }
            MarimayCheckedI64Status checked = mmay_checked_i64_shift_left(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == MMAY_CHECKED_I64_INVALID_SHIFT) {
                return MMAY_MIR_INTERPRET_INVALID_SHIFT;
            }
            return checked == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        }
        case MMAY_MIR_SHIFT_RIGHT_I64: {
            if (first->type != MMAY_TYPE_I64) {
                return mmay_mir_integer_binary(
                    MMAY_MIR_INTEGER_SHIFT_RIGHT, first, second, target
                );
            }
            MarimayCheckedI64Status checked = mmay_checked_i64_shift_right(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == MMAY_CHECKED_I64_INVALID_SHIFT) {
                return MMAY_MIR_INTERPRET_INVALID_SHIFT;
            }
            return checked == MMAY_CHECKED_I64_OK
                ? MMAY_MIR_INTERPRET_OK
                : MMAY_MIR_INTERPRET_OVERFLOW;
        }
        case MMAY_MIR_EQUAL_I64:
            if (first->type != MMAY_TYPE_I64) {
                target->as.boolean = !mmay_mir_integer_less(
                    first, second, false
                ) && !mmay_mir_integer_less(second, first, false);
            } else {
                target->as.boolean = first->as.i64 == second->as.i64;
            }
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_EQUAL_F64:
            target->as.boolean = mmay_f64_equal(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_EQUAL_BOOL:
            target->as.boolean = first->as.boolean == second->as.boolean;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_EQUAL_STRING: {
            const MarimayMirManagedString *left =
                mmay_mir_execution_string_value(options.execution, *first);
            const MarimayMirManagedString *right =
                mmay_mir_execution_string_value(options.execution, *second);
            if (left == NULL || right == NULL) {
                return options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            target->as.boolean = left->length == right->length
                && (left->length == 0
                    || memcmp(left->bytes, right->bytes, left->length) == 0);
            return MMAY_MIR_INTERPRET_OK;
        }
        case MMAY_MIR_RECORD_IDENTITY_SAME: {
            const MarimayMirRecord *layout = mmay_mir_record(
                function,
                instruction->record
            );
            MarimayMirManagedRecord *left = options.execution != NULL
                ? mmay_mir_execution_record(
                    options.execution,
                    first->as.reference
                )
                : NULL;
            MarimayMirManagedRecord *right = options.execution != NULL
                ? mmay_mir_execution_record(
                    options.execution,
                    second->as.reference
                )
                : NULL;
            if (options.execution == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (layout == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (left == NULL || right == NULL
                || left->type != layout->type
                || right->type != layout->type) {
                return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (!mmay_mir_managed_record_matches_layout(
                    function,
                    layout,
                    left
                ) || !mmay_mir_managed_record_matches_layout(
                    function,
                    layout,
                    right
                )) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            target->as.boolean = mmay_mir_reference_equal(
                first->as.reference,
                second->as.reference
            );
            return MMAY_MIR_INTERPRET_OK;
        }
        case MMAY_MIR_LESS_I64:
            target->as.boolean = first->type != MMAY_TYPE_I64
                ? mmay_mir_integer_less(first, second, false)
                : first->as.i64 < second->as.i64;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_LESS_F64:
            target->as.boolean = mmay_f64_less(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_LESS_EQUAL_I64:
            target->as.boolean = first->type != MMAY_TYPE_I64
                ? mmay_mir_integer_less(first, second, true)
                : first->as.i64 <= second->as.i64;
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_LESS_EQUAL_F64:
            target->as.boolean = mmay_f64_less_equal(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return MMAY_MIR_INTERPRET_OK;
        case MMAY_MIR_ARRAY_NEW: {
            const MarimayMirArrayConstruction *construction =
                mmay_mir_array_construction(
                    function,
                    instruction->array_construction
                );
            const MarimayMirArray *layout = construction != NULL
                ? mmay_mir_array(function, construction->array)
                : NULL;
            MarimayMirValue *elements = NULL;
            size_t element_bytes = 0;
            size_t argument_end = 0;
            size_t index = 0;
            MarimayMirInterpretStatus status;
            if (construction == NULL || layout == NULL
                || instruction->array != construction->array
                || options.execution == NULL
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->array_arguments.length
                || !mmay_size_multiply(
                    construction->argument_count,
                    sizeof(MarimayMirValue),
                    &element_bytes
                )) {
                return options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            status = mmay_mir_execution_array_preflight(
                options.execution,
                construction->argument_count,
                NULL,
                NULL,
                NULL
            );
            if (status != MMAY_MIR_INTERPRET_OK) {
                return status;
            }
            if (element_bytes != 0) {
                elements = mmay_allocate(allocator, element_bytes);
                if (elements == NULL) {
                    return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
                }
            }
            if (construction->argument_count != 0 && elements == NULL) {
                return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const MarimayMirArrayArgument *argument =
                    mmay_mir_array_argument(
                        function,
                        construction->argument_start + index
                    );
                if (argument == NULL
                    || argument->register_id >= register_count) {
                    mmay_deallocate(allocator, elements, element_bytes);
                    return MMAY_MIR_INTERPRET_INVALID_MIR;
                }
                elements[index] = registers[argument->register_id];
            }
            status = mmay_mir_execution_new_array(
                options.execution,
                layout,
                elements,
                construction->argument_count,
                &target->as.reference
            );
            mmay_deallocate(allocator, elements, element_bytes);
            return status;
        }
        case MMAY_MIR_ARRAY_LENGTH:
        case MMAY_MIR_ARRAY_REQUIRE_INDEX:
        case MMAY_MIR_ARRAY_ASSUME_INDEX:
        case MMAY_MIR_ARRAY_GET:
        case MMAY_MIR_ARRAY_SET:
        case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
        case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
        case MMAY_MIR_ARRAY_PUSH:
        case MMAY_MIR_ARRAY_POP:
        case MMAY_MIR_ARRAY_TRUNCATE:
        case MMAY_MIR_ARRAY_CLEAR:
        case MMAY_MIR_ARRAY_RESERVE:
        case MMAY_MIR_ARRAY_GENERATION:
        case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
            return mmay_mir_execute_array_instruction(
                function, instruction, first, second, third, target,
                allocator, options
            );
        case MMAY_MIR_RECORD_NEW: {
            const MarimayMirRecordConstruction *construction =
                mmay_mir_record_construction(
                    function,
                    instruction->record_construction
                );
            const MarimayMirRecord *record = construction != NULL
                ? mmay_mir_record(function, construction->record)
                : NULL;
            const MarimayMirRecordField *layout_fields = record != NULL
                    && record->field_count != 0
                ? mmay_vector_at_const(
                    &function->record_fields,
                    record->field_start
                )
                : NULL;
            MarimayMirValue *fields = NULL;
            size_t field_bytes = 0;
            size_t argument_end = 0;
            size_t index = 0;
            MarimayMirInterpretStatus status;
            if (construction == NULL || record == NULL
                || instruction->record != construction->record
                || options.execution == NULL
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->record_arguments.length
                || !mmay_size_multiply(
                    construction->argument_count,
                    sizeof(MarimayMirValue),
                    &field_bytes
                )) {
                return options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (field_bytes != 0) {
                fields = mmay_allocate(allocator, field_bytes);
                if (fields == NULL) {
                    return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
                }
            }
            if (construction->argument_count != 0 && fields == NULL) {
                return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const MarimayMirRecordArgument *argument =
                    mmay_mir_record_argument(
                        function,
                        construction->argument_start + index
                    );
                if (argument == NULL
                    || argument->register_id >= register_count) {
                    mmay_deallocate(allocator, fields, field_bytes);
                    return MMAY_MIR_INTERPRET_INVALID_MIR;
                }
                fields[index] = registers[argument->register_id];
            }
            status = mmay_mir_execution_new_record(
                options.execution,
                record->type,
                fields,
                layout_fields,
                construction->argument_count,
                &target->as.reference
            );
            mmay_deallocate(allocator, fields, field_bytes);
            return status;
        }
        case MMAY_MIR_RECORD_GET:
        case MMAY_MIR_RECORD_SET: {
            const MarimayMirRecord *layout = mmay_mir_record(
                function,
                instruction->record
            );
            MarimayMirManagedRecord *record = options.execution != NULL
                ? mmay_mir_execution_record(
                    options.execution,
                    first->as.reference
                )
                : NULL;
            if (options.execution == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (record == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (layout == NULL || instruction->field >= record->field_count
                || !mmay_mir_managed_record_matches_layout(
                    function,
                    layout,
                    record
                )) {
                return record == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_REFERENCE
                    : MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (instruction->kind == MMAY_MIR_RECORD_SET) {
                record->fields[instruction->field] = *second;
                *target = *second;
            } else {
                *target = record->fields[instruction->field];
            }
            return MMAY_MIR_INTERPRET_OK;
        }
        case MMAY_MIR_ENUM_NEW: {
            const MarimayMirEnumConstruction *construction =
                mmay_mir_enum_construction(
                    function,
                    instruction->enum_construction
                );
            const MarimayMirEnum *enum_layout = construction != NULL
                ? mmay_mir_enum(function, construction->enum_id)
                : NULL;
            const MarimayMirEnumVariant *variant = construction != NULL
                ? mmay_mir_enum_variant(
                    function,
                    enum_layout,
                    construction->variant
                )
                : NULL;
            MarimayMirValue *payload = NULL;
            size_t payload_bytes = 0;
            size_t argument_end = 0;
            size_t index = 0;
            MarimayMirInterpretStatus status;
            if (construction == NULL || enum_layout == NULL || variant == NULL
                || instruction->enum_id != construction->enum_id
                || options.execution == NULL
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->enum_arguments.length
                || !mmay_size_multiply(
                    construction->argument_count,
                    sizeof(MarimayMirValue),
                    &payload_bytes
                )) {
                return options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (payload_bytes != 0) {
                payload = mmay_allocate(allocator, payload_bytes);
                if (payload == NULL) {
                    return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
                }
            }
            if (construction->argument_count != 0 && payload == NULL) {
                return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const MarimayMirEnumArgument *argument = mmay_mir_enum_argument(
                    function,
                    construction->argument_start + index
                );
                if (argument == NULL
                    || argument->register_id >= register_count) {
                    mmay_deallocate(allocator, payload, payload_bytes);
                    return MMAY_MIR_INTERPRET_INVALID_MIR;
                }
                payload[index] = registers[argument->register_id];
            }
            status = mmay_mir_execution_new_enum(
                options.execution,
                function,
                enum_layout,
                construction->variant,
                payload,
                construction->argument_count,
                &target->as.reference
            );
            mmay_deallocate(allocator, payload, payload_bytes);
            return status;
        }
        case MMAY_MIR_ENUM_IS_VARIANT:
        case MMAY_MIR_ENUM_REQUIRE_VARIANT:
        case MMAY_MIR_ENUM_GET_PAYLOAD: {
            const MarimayMirEnum *layout = mmay_mir_enum(
                function,
                instruction->enum_id
            );
            const MarimayMirEnumVariant *variant = mmay_mir_enum_variant(
                function,
                layout,
                instruction->variant
            );
            const MarimayMirManagedEnum *value = options.execution != NULL
                ? mmay_mir_execution_enum_value(options.execution, *first)
                : NULL;
            if (options.execution == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (value == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (layout == NULL || variant == NULL
                || !mmay_mir_managed_enum_matches_layout(
                    function,
                    layout,
                    value
                )) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (instruction->kind == MMAY_MIR_ENUM_IS_VARIANT) {
                target->as.boolean = value->variant == instruction->variant;
                return MMAY_MIR_INTERPRET_OK;
            }
            if (value->variant != instruction->variant) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (instruction->kind == MMAY_MIR_ENUM_REQUIRE_VARIANT) {
                *target = *first;
                return MMAY_MIR_INTERPRET_OK;
            }
            if (instruction->field >= value->selected_payload_count) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            *target = value->payload[instruction->field];
            return MMAY_MIR_INTERPRET_OK;
        }
        case MMAY_MIR_CALL: {
            const MarimayMirCall *call = mmay_mir_call(
                function,
                instruction->call
            );
            MarimayMirValue call_result;
            MarimayMirInterpretStatus status;
            memset(&call_result, 0, sizeof(call_result));
            status = mmay_mir_invoke_call(
                function,
                instruction->call,
                registers,
                register_count,
                allocator,
                options,
                NULL,
                NULL,
                &call_result
            );
            if (status != MMAY_MIR_INTERPRET_OK) {
                return status;
            }
            if (call == NULL || call_result.type != call->result_type) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (mmay_mir_type_is_managed(call_result.type)
                && !mmay_mir_execution_owns_reference(
                    options.execution,
                    call_result
                )) {
                return options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            *target = call_result;
            return MMAY_MIR_INTERPRET_OK;
        }
        case MMAY_MIR_INTERFACE_NEW: {
            const MarimayMirInterfaceConstruction *construction =
                mmay_mir_interface_construction(
                    function,
                    instruction->interface_construction
                );
            const MarimayMirInterface *layout = construction != NULL
                ? mmay_mir_interface(function, construction->interface_id)
                : NULL;
            const MarimaySymbolId *methods = construction != NULL
                    && construction->method_count != 0
                ? mmay_mir_interface_method(
                    function,
                    construction->method_start
                )
                : NULL;
            size_t method_end = 0;
            if (options.execution == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (construction == NULL || layout == NULL
                || construction->payload >= register_count
                || construction->method_count != layout->method_count
                || !mmay_size_add(
                    construction->method_start,
                    construction->method_count,
                    &method_end
                ) || method_end > function->interface_methods.length
                || (construction->method_count != 0 && methods == NULL)) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            return mmay_mir_execution_new_interface(
                options.execution,
                layout->type,
                registers[construction->payload],
                methods,
                construction->method_count,
                &target->as.reference
            );
        }
        case MMAY_MIR_CALL_INTERFACE: {
            const MarimayMirCall *call = mmay_mir_call(
                function,
                instruction->call
            );
            const MarimayMirCallArgument *receiver = call != NULL
                    && call->argument_count != 0
                ? mmay_mir_call_argument(function, call->argument_start)
                : NULL;
            const MarimayMirManagedInterface *interface_value =
                receiver != NULL && receiver->register_id < register_count
                    && options.execution != NULL
                ? mmay_mir_execution_interface_value(
                    options.execution,
                    registers[receiver->register_id]
                )
                : NULL;
            const MarimaySymbolId *selected = interface_value != NULL
                    && call->interface_slot < interface_value->method_count
                ? &interface_value->methods[call->interface_slot]
                : NULL;
            MarimayMirValue call_result;
            MarimayMirInterpretStatus status;
            if (options.execution == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (call == NULL || receiver == NULL
                || receiver->register_id >= register_count
                || call->interface_slot == UINT32_MAX) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (interface_value == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (selected == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            memset(&call_result, 0, sizeof(call_result));
            status = mmay_mir_invoke_call(
                function,
                instruction->call,
                registers,
                register_count,
                allocator,
                options,
                selected,
                &interface_value->payload,
                &call_result
            );
            if (status != MMAY_MIR_INTERPRET_OK) {
                return status;
            }
            if (call_result.type != call->result_type) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            if (mmay_mir_type_is_managed(call_result.type)
                && !mmay_mir_execution_owns_reference(
                    options.execution,
                    call_result
                )) {
                return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
            *target = call_result;
            return MMAY_MIR_INTERPRET_OK;
        }
        case MMAY_MIR_DEFER_REGISTER:
        case MMAY_MIR_DEFER_CALL:
            return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    return MMAY_MIR_INTERPRET_INVALID_MIR;
}

MarimayMirInterpretStatus mmay_mir_interpret(
    const MarimayMirFunction *function,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayMirInterpretOptions options,
    MarimayMirValue *result
) {
    MarimayMirVerifyStatus verify_status;
    MarimayMirValue *registers = NULL;
    MarimayMirCallId *defer_stack = NULL;
    size_t register_bytes = 0;
    size_t defer_bytes = 0;
    size_t defer_count = 0;
    size_t register_count = 0;
    size_t index = 0;
    size_t steps = 0;
    MarimayMirBlockId current = 0;
    MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_OK;
    if (function == NULL || !function->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || argument_count != function->parameter_count
        || (arguments == NULL && argument_count != 0)
        || options.max_steps == 0) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (options.panic != NULL) {
        mmay_mir_panic_dispose(options.panic);
    }
    verify_status = mmay_mir_verify(function);
    if (verify_status == MMAY_MIR_VERIFY_ALLOCATION_FAILED) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (mmay_mir_uses_f64(function) && !mmay_f64_binary64_supported()) {
        return MMAY_MIR_INTERPRET_UNSUPPORTED_HOST;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index].type != mmay_mir_register_type(
                function,
                (MarimayMirRegister)index
            )) {
            return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
        }
        if (mmay_mir_type_is_managed(arguments[index].type)
            && !mmay_mir_execution_owns_reference(
                options.execution,
                arguments[index]
            )) {
            return options.execution == NULL
                ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                : MMAY_MIR_INTERPRET_INVALID_REFERENCE;
        }
    }
    register_count = function->register_types.length;
    if (!mmay_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(MarimayMirValue),
            &register_bytes
        )) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    registers = mmay_allocate(&allocator, register_bytes);
    if (registers == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (function->calls.length != 0) {
        if (!mmay_size_multiply(
                function->calls.length, sizeof(*defer_stack), &defer_bytes
            )) {
            mmay_deallocate(&allocator, registers, register_bytes);
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        defer_stack = mmay_allocate(&allocator, defer_bytes);
        if (defer_stack == NULL) {
            mmay_deallocate(&allocator, registers, register_bytes);
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
    }
    memset(registers, 0, register_bytes);
    for (index = 0; index < register_count; index += 1) {
        registers[index].type = mmay_mir_register_type(
            function,
            (MarimayMirRegister)index
        );
    }
    for (index = 0; index < argument_count; index += 1) {
        registers[index] = arguments[index];
    }
    for (;;) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks,
            current
        );
        if (block == NULL) {
            status = MMAY_MIR_INTERPRET_INVALID_MIR;
            break;
        }
        for (index = 0; index < block->instructions.length; index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                index
            );
            if (steps >= options.max_steps
                || (options.remaining_shared_steps != NULL
                    && *options.remaining_shared_steps == 0)) {
                status = MMAY_MIR_INTERPRET_STEP_LIMIT;
                goto cleanup;
            }
            steps += 1;
            if (options.remaining_shared_steps != NULL) {
                *options.remaining_shared_steps -= 1;
            }
            if (instruction->kind == MMAY_MIR_DEFER_REGISTER) {
                if (defer_count >= function->calls.length) {
                    status = MMAY_MIR_INTERPRET_INVALID_MIR;
                    goto cleanup;
                }
                defer_stack[defer_count] = instruction->call;
                defer_count += 1;
                registers[instruction->target].as.boolean = true;
                status = MMAY_MIR_INTERPRET_OK;
            } else if (instruction->kind == MMAY_MIR_DEFER_CALL) {
                if (defer_count == 0
                    || defer_stack[defer_count - 1] != instruction->call) {
                    status = MMAY_MIR_INTERPRET_INVALID_MIR;
                    goto cleanup;
                }
                defer_count -= 1;
                status = mmay_mir_invoke_deferred_call(
                    function, instruction->call, registers, register_count,
                    &allocator, options
                );
            } else {
                status = mmay_mir_execute_instruction(
                    function, instruction, registers, register_count,
                    &allocator, options
                );
            }
            if (status != MMAY_MIR_INTERPRET_OK) {
                goto cleanup;
            }
        }
        if (steps >= options.max_steps
            || (options.remaining_shared_steps != NULL
                && *options.remaining_shared_steps == 0)) {
            status = MMAY_MIR_INTERPRET_STEP_LIMIT;
            break;
        }
        steps += 1;
        if (options.remaining_shared_steps != NULL) {
            *options.remaining_shared_steps -= 1;
        }
        if (block->terminator.kind == MMAY_MIR_RETURN) {
            if (function->result_type == MMAY_TYPE_VOID) {
                memset(result, 0, sizeof(*result));
                result->type = MMAY_TYPE_VOID;
            } else {
                *result = registers[block->terminator.value];
            }
            status = MMAY_MIR_INTERPRET_OK;
            break;
        }
        if (block->terminator.kind == MMAY_MIR_PANIC) {
            const MarimayMirManagedString *message =
                mmay_mir_execution_string_value(
                    options.execution,
                    registers[block->terminator.value]
                );
            if (message == NULL) {
                status = options.execution == NULL
                    ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                    : MMAY_MIR_INTERPRET_INVALID_REFERENCE;
                break;
            }
            if (options.panic != NULL) {
                if (!mmay_owned_bytes_copy(
                        &options.panic->message,
                        allocator,
                        message->bytes,
                        message->length
                    )) {
                    status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
                    break;
                }
                options.panic->code = "G4000";
                options.panic->symbol = function->symbol;
                options.panic->span = block->terminator.span;
            }
            status = MMAY_MIR_INTERPRET_PANIC;
            break;
        }
        if (block->terminator.kind == MMAY_MIR_CALL_NEVER) {
            MarimayMirValue unexpected_result;
            memset(&unexpected_result, 0, sizeof(unexpected_result));
            status = mmay_mir_invoke_call(
                function,
                block->terminator.call,
                registers,
                register_count,
                &allocator,
                options,
                NULL,
                NULL,
                &unexpected_result
            );
            if (status == MMAY_MIR_INTERPRET_OK) {
                status = MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            break;
        }
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            current = block->terminator.target;
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            current = registers[block->terminator.value].as.boolean
                ? block->terminator.target
                : block->terminator.otherwise;
        } else {
            status = MMAY_MIR_INTERPRET_INVALID_MIR;
            break;
        }
    }
cleanup:
    while (defer_count != 0) {
        MarimayMirInterpretStatus cleanup_status;
        MarimayMirPanic primary;
        bool preserve_primary = status == MMAY_MIR_INTERPRET_PANIC
            && options.panic != NULL;
        memset(&primary, 0, sizeof(primary));
        if (preserve_primary) {
            primary = *options.panic;
            memset(options.panic, 0, sizeof(*options.panic));
        }
        defer_count -= 1;
        cleanup_status = mmay_mir_invoke_deferred_call(
            function, defer_stack[defer_count], registers, register_count,
            &allocator, options
        );
        if (preserve_primary) {
            if (cleanup_status == MMAY_MIR_INTERPRET_PANIC
                && !mmay_mir_panic_append_suppressed(
                    &primary, options.panic, allocator
                )) {
                cleanup_status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            mmay_mir_panic_dispose(options.panic);
            *options.panic = primary;
            if (cleanup_status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED) {
                status = cleanup_status;
            }
        } else if (cleanup_status == MMAY_MIR_INTERPRET_PANIC) {
            status = MMAY_MIR_INTERPRET_PANIC;
        } else if (status == MMAY_MIR_INTERPRET_OK
            && cleanup_status != MMAY_MIR_INTERPRET_OK) {
            status = cleanup_status;
        }
    }
    mmay_deallocate(&allocator, defer_stack, defer_bytes);
    mmay_deallocate(&allocator, registers, register_bytes);
    return status;
}
