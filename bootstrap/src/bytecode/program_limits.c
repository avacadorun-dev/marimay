#include "program_internal.h"

#include <limits.h>

#include "bytecode_internal.h"

static bool mmay_bytecode_program_add_total(
    size_t *total,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (total == NULL || !mmay_size_add(*total, amount, &next) || next > limit) {
        return false;
    }
    *total = next;
    return true;
}

bool mmay_bytecode_program_valid_options(MarimayBytecodeProgramOptions options) {
    return options.max_functions != 0 && options.max_functions <= UINT32_MAX
        && options.max_total_registers != 0 && options.max_total_blocks != 0
        && options.max_total_instructions != 0 && options.max_total_calls != 0
        && options.max_total_call_arguments != 0
        && options.max_total_string_constants != 0
        && options.max_total_string_bytes != 0
        && options.max_total_array_constructions != 0
        && options.max_total_array_arguments != 0
        && options.max_total_record_layouts != 0
        && options.max_total_record_fields != 0
        && options.max_total_record_constructions != 0
        && options.max_total_record_arguments != 0
        && options.max_total_enum_layouts != 0
        && options.max_total_enum_variants != 0
        && options.max_total_enum_payloads != 0
        && options.max_total_enum_constructions != 0
        && options.max_total_enum_arguments != 0
        && options.max_total_interface_layouts != 0
        && options.max_total_interface_constructions != 0
        && options.max_total_interface_methods != 0
        && options.max_encoded_bytes
            >= MMAY_BYTECODE_PROGRAM_MINIMUM_ENCODED_BYTES
        && options.max_verify_work != 0
        && options.function_options.max_registers != 0
        && options.function_options.max_blocks != 0
        && options.function_options.max_instructions != 0
        && options.function_options.max_calls != 0
        && options.function_options.max_call_arguments != 0
        && options.function_options.max_string_constants != 0
        && options.function_options.max_string_bytes != 0
        && options.function_options.max_array_constructions != 0
        && options.function_options.max_array_arguments != 0
        && options.function_options.max_record_layouts != 0
        && options.function_options.max_record_fields != 0
        && options.function_options.max_record_constructions != 0
        && options.function_options.max_record_arguments != 0
        && options.function_options.max_enum_layouts != 0
        && options.function_options.max_enum_variants != 0
        && options.function_options.max_enum_payloads != 0
        && options.function_options.max_enum_constructions != 0
        && options.function_options.max_enum_arguments != 0
        && options.function_options.max_interface_layouts != 0
        && options.function_options.max_interface_constructions != 0
        && options.function_options.max_interface_methods != 0
        && options.function_options.max_source_files != 0
        && options.function_options.max_encoded_bytes != 0
        && options.function_options.max_verify_work != 0;
}

bool mmay_bytecode_program_function_options_within(
    MarimayBytecodeFunctionOptions actual,
    MarimayBytecodeFunctionOptions limit
) {
    return actual.max_registers <= limit.max_registers
        && actual.max_blocks <= limit.max_blocks
        && actual.max_instructions <= limit.max_instructions
        && actual.max_calls <= limit.max_calls
        && actual.max_call_arguments <= limit.max_call_arguments
        && actual.max_string_constants <= limit.max_string_constants
        && actual.max_string_bytes <= limit.max_string_bytes
        && actual.max_array_constructions <= limit.max_array_constructions
        && actual.max_array_arguments <= limit.max_array_arguments
        && actual.max_record_layouts <= limit.max_record_layouts
        && actual.max_record_fields <= limit.max_record_fields
        && actual.max_record_constructions <= limit.max_record_constructions
        && actual.max_record_arguments <= limit.max_record_arguments
        && actual.max_enum_layouts <= limit.max_enum_layouts
        && actual.max_enum_variants <= limit.max_enum_variants
        && actual.max_enum_payloads <= limit.max_enum_payloads
        && actual.max_enum_constructions <= limit.max_enum_constructions
        && actual.max_enum_arguments <= limit.max_enum_arguments
        && actual.max_interface_layouts <= limit.max_interface_layouts
        && actual.max_interface_constructions
            <= limit.max_interface_constructions
        && actual.max_interface_methods <= limit.max_interface_methods
        && actual.max_source_files <= limit.max_source_files
        && actual.max_encoded_bytes <= limit.max_encoded_bytes
        && actual.max_verify_work <= limit.max_verify_work;
}

bool mmay_bytecode_program_accumulate_function(
    MarimayBytecodeProgramTotals *totals,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeProgramOptions options
) {
#define MMAY_ADD_TOTAL(member, vector, limit) \
    mmay_bytecode_program_add_total( \
        &totals->member, function->vector.length, options.limit \
    )
    bool valid = totals != NULL && function != NULL
        && MMAY_ADD_TOTAL(registers, register_types, max_total_registers)
        && MMAY_ADD_TOTAL(blocks, blocks, max_total_blocks)
        && mmay_bytecode_program_add_total(
            &totals->instructions,
            function->instruction_count,
            options.max_total_instructions
        )
        && MMAY_ADD_TOTAL(calls, calls, max_total_calls)
        && MMAY_ADD_TOTAL(call_arguments, call_arguments, max_total_call_arguments)
        && MMAY_ADD_TOTAL(
            string_constants, string_constants, max_total_string_constants
        )
        && mmay_bytecode_program_add_total(
            &totals->string_bytes,
            function->string_bytes,
            options.max_total_string_bytes
        )
        && MMAY_ADD_TOTAL(
            array_constructions,
            array_constructions,
            max_total_array_constructions
        )
        && MMAY_ADD_TOTAL(array_arguments, array_arguments, max_total_array_arguments)
        && MMAY_ADD_TOTAL(record_layouts, record_layouts, max_total_record_layouts)
        && MMAY_ADD_TOTAL(record_fields, record_fields, max_total_record_fields)
        && MMAY_ADD_TOTAL(
            record_constructions,
            record_constructions,
            max_total_record_constructions
        )
        && MMAY_ADD_TOTAL(record_arguments, record_arguments, max_total_record_arguments)
        && MMAY_ADD_TOTAL(enum_layouts, enum_layouts, max_total_enum_layouts)
        && MMAY_ADD_TOTAL(enum_variants, enum_variants, max_total_enum_variants)
        && MMAY_ADD_TOTAL(enum_payloads, enum_payloads, max_total_enum_payloads)
        && MMAY_ADD_TOTAL(
            enum_constructions,
            enum_constructions,
            max_total_enum_constructions
        )
        && MMAY_ADD_TOTAL(enum_arguments, enum_arguments, max_total_enum_arguments)
        && MMAY_ADD_TOTAL(
            interface_layouts,
            interface_layouts,
            max_total_interface_layouts
        )
        && MMAY_ADD_TOTAL(
            interface_constructions,
            interface_constructions,
            max_total_interface_constructions
        )
        && MMAY_ADD_TOTAL(
            interface_methods,
            interface_methods,
            max_total_interface_methods
        );
#undef MMAY_ADD_TOTAL
    return valid;
}
