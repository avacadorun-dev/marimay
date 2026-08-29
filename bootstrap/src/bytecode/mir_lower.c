#include "mmay/bytecode/mir_lower.h"

#include "mir_lower_internal.h"

#include "bytecode_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

MarimayMirBytecodeLowerOptions mmay_mir_bytecode_lower_default_options(void) {
    MarimayMirBytecodeLowerOptions options;
    options.source = 0;
    options.source_count = 1;
    options.function_options = mmay_bytecode_function_default_options();
    return options;
}

MarimayMirBytecodeProgramLowerOptions
mmay_mir_bytecode_program_lower_default_options(void) {
    MarimayMirBytecodeProgramLowerOptions options;
    options.source = 0;
    options.source_count = 1;
    options.program_options = mmay_bytecode_program_default_options();
    return options;
}

static bool mmay_mir_bytecode_options_valid(
    MarimayAllocator allocator,
    MarimayMirBytecodeLowerOptions options
) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL && options.source_count != 0
        && options.source < options.source_count
        && options.source_count <= options.function_options.max_source_files
        && options.function_options.max_registers != 0
        && options.function_options.max_blocks != 0
        && options.function_options.max_instructions != 0
        && options.function_options.max_calls != 0
        && options.function_options.max_call_arguments != 0
        && options.function_options.max_string_constants != 0
        && options.function_options.max_string_bytes != 0
        && options.function_options.max_array_constructions != 0
        && options.function_options.max_array_arguments != 0
        && options.function_options.max_collection_layouts != 0
        && options.function_options.max_source_files != 0
        && options.function_options.max_encoded_bytes >= 8U
        && options.function_options.max_verify_work != 0;
}

static bool mmay_mir_bytecode_span(
    MarimayMirSpan input,
    uint64_t source,
    MarimayBytecodeSpan *output
) {
#if SIZE_MAX > UINT64_MAX
    if (input.start > UINT64_MAX || input.end > UINT64_MAX) {
        return false;
    }
#endif
    output->source = source;
    output->start = (uint64_t)input.start;
    output->end = (uint64_t)input.end;
    return true;
}

static MarimayOpcode mmay_mir_bytecode_integer_policy_opcode(
    MarimayMirIntegerPolicy policy,
    MarimayMirIntegerOperation operation
) {
    MarimayOpcode base;
    if (policy == MMAY_MIR_INTEGER_CHECKED_OPTION) {
        base = MMAY_OPCODE_INTEGER_CHECKED_ADD;
    } else if (policy == MMAY_MIR_INTEGER_WRAPPING) {
        base = MMAY_OPCODE_INTEGER_WRAPPING_ADD;
    } else if (policy == MMAY_MIR_INTEGER_SATURATING) {
        base = MMAY_OPCODE_INTEGER_SATURATING_ADD;
    } else {
        return MMAY_OPCODE_INVALID;
    }
    if (operation > MMAY_MIR_POLICY_SHIFT_RIGHT) {
        return MMAY_OPCODE_INVALID;
    }
    return (MarimayOpcode)((unsigned int)base + (unsigned int)operation);
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *input,
    uint64_t source,
    MarimayBytecodeInstruction *output
) {
    bool format_handled = false;
    bool text_handled = false;
    bool fs_handled = false;
    bool context_handled = false;
    MarimayMirBytecodeLowerStatus format_status;
    MarimayMirBytecodeLowerStatus text_status;
    MarimayMirBytecodeLowerStatus fs_status;
    MarimayMirBytecodeLowerStatus context_status;
    memset(output, 0, sizeof(*output));
    if (!mmay_mir_bytecode_span(input->span, source, &output->span)) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    output->operands[0].unsigned_value = input->target;
    format_status = mmay_mir_bytecode_lower_format_instruction(
        input, output, &format_handled
    );
    if (format_handled || format_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        return format_status;
    }
    text_status = mmay_mir_bytecode_lower_text_instruction(
        input, output, &text_handled
    );
    if (text_handled || text_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        return text_status;
    }
    fs_status = mmay_mir_bytecode_lower_fs_instruction(
        input, output, &fs_handled
    );
    if (fs_handled || fs_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        return fs_status;
    }
    context_status = mmay_mir_bytecode_lower_context_instruction(
        input, output, &context_handled
    );
    if (context_handled || context_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        return context_status;
    }
    switch (input->kind) {
        case MMAY_MIR_FORMAT_F32:
        case MMAY_MIR_FORMAT_F64:
        case MMAY_MIR_FORMAT_BOOL:
        case MMAY_MIR_FORMAT_INTEGER:
        case MMAY_MIR_STRING_CONCAT:
        case MMAY_MIR_PARSE_INTEGER:
        case MMAY_MIR_TEXT_UNARY:
        case MMAY_MIR_TEXT_BINARY:
        case MMAY_MIR_IO_READ_BYTES:
        case MMAY_MIR_IO_WRITE_BYTES:
        case MMAY_MIR_PATH:
        case MMAY_MIR_FS_UNARY:
        case MMAY_MIR_FS_BINARY:
        case MMAY_MIR_ENV_READ:
        case MMAY_MIR_PROCESS_CONTEXT:
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        case MMAY_MIR_CONSTANT_I64:
            output->opcode = MMAY_OPCODE_CONST_I64;
            output->operands[1].signed_value = input->integer;
            break;
        case MMAY_MIR_CONSTANT_INTEGER:
            output->opcode = MMAY_OPCODE_CONST_INTEGER;
            output->operands[1].unsigned_value = input->integer_bits;
            break;
        case MMAY_MIR_CONSTANT_BOOL:
            output->opcode = MMAY_OPCODE_CONST_BOOL;
            output->operands[1].unsigned_value = input->boolean ? 1U : 0U;
            break;
        case MMAY_MIR_CONSTANT_F64:
            output->opcode = MMAY_OPCODE_CONST_F64;
            output->operands[1].unsigned_value = input->f64_bits;
            break;
        case MMAY_MIR_CONSTANT_STRING:
            output->opcode = MMAY_OPCODE_CONST_STRING;
            output->operands[1].unsigned_value = input->string_constant;
            break;
        case MMAY_MIR_COPY:
            output->opcode = MMAY_OPCODE_COPY;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_ADD_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_ADD_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_ADD_PROVEN_I64:
            output->opcode = MMAY_OPCODE_I64_ADD_PROVEN;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_SUBTRACT_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
            output->opcode = MMAY_OPCODE_I64_SUBTRACT_PROVEN;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_MULTIPLY_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
            output->opcode = MMAY_OPCODE_I64_MULTIPLY_PROVEN;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_DIVIDE_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_DIVIDE_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_REMAINDER_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_REMAINDER_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_NEGATE_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_NEGATE_CHECKED;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_ADD_F64:
            output->opcode = MMAY_OPCODE_F64_ADD;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_SUBTRACT_F64:
            output->opcode = MMAY_OPCODE_F64_SUBTRACT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_MULTIPLY_F64:
            output->opcode = MMAY_OPCODE_F64_MULTIPLY;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_DIVIDE_F64:
            output->opcode = MMAY_OPCODE_F64_DIVIDE;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_NEGATE_F64:
            output->opcode = MMAY_OPCODE_F64_NEGATE;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_NOT_BOOL:
            output->opcode = MMAY_OPCODE_BOOL_NOT;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_BITWISE_NOT_I64:
            output->opcode = MMAY_OPCODE_I64_BITWISE_NOT;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_BITWISE_AND_I64:
            output->opcode = MMAY_OPCODE_I64_BITWISE_AND;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_BITWISE_XOR_I64:
            output->opcode = MMAY_OPCODE_I64_BITWISE_XOR;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_BITWISE_OR_I64:
            output->opcode = MMAY_OPCODE_I64_BITWISE_OR;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64:
            output->opcode = MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_SHIFT_RIGHT_I64:
            output->opcode = MMAY_OPCODE_I64_SHIFT_RIGHT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_EQUAL_I64:
            output->opcode = MMAY_OPCODE_I64_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_EQUAL_BOOL:
            output->opcode = MMAY_OPCODE_BOOL_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_EQUAL_STRING:
            output->opcode = MMAY_OPCODE_STRING_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_EQUAL_F64:
            output->opcode = MMAY_OPCODE_F64_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_LESS_I64:
            output->opcode = MMAY_OPCODE_I64_LESS;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_LESS_EQUAL_I64:
            output->opcode = MMAY_OPCODE_I64_LESS_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_LESS_F64:
            output->opcode = MMAY_OPCODE_F64_LESS;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_LESS_EQUAL_F64:
            output->opcode = MMAY_OPCODE_F64_LESS_EQUAL;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_RECORD_NEW:
            output->opcode = MMAY_OPCODE_RECORD_NEW;
            output->operands[1].unsigned_value = input->record_construction;
            break;
        case MMAY_MIR_RECORD_GET:
            output->opcode = MMAY_OPCODE_RECORD_GET;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->field;
            break;
        case MMAY_MIR_RECORD_SET:
            output->opcode = MMAY_OPCODE_RECORD_SET;
            output->operands[0].unsigned_value = input->second;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->field;
            break;
        case MMAY_MIR_RECORD_IDENTITY_SAME:
            output->opcode = MMAY_OPCODE_RECORD_IDENTITY_SAME;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_ENUM_NEW:
            output->opcode = MMAY_OPCODE_ENUM_NEW;
            output->operands[1].unsigned_value = input->enum_construction;
            break;
        case MMAY_MIR_ENUM_IS_VARIANT:
            output->opcode = MMAY_OPCODE_ENUM_IS_VARIANT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->variant;
            break;
        case MMAY_MIR_ENUM_REQUIRE_VARIANT:
            output->opcode = MMAY_OPCODE_ENUM_REQUIRE_VARIANT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->variant;
            break;
        case MMAY_MIR_ENUM_GET_PAYLOAD:
            output->opcode = MMAY_OPCODE_ENUM_GET_PAYLOAD;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)(
                ((uint64_t)input->variant << 32) | (uint64_t)input->field
            );
            break;
        case MMAY_MIR_ARRAY_NEW:
            output->opcode = MMAY_OPCODE_ARRAY_NEW;
            output->operands[1].unsigned_value = input->array_construction;
            break;
        case MMAY_MIR_ARRAY_LENGTH:
            output->opcode = MMAY_OPCODE_ARRAY_LENGTH;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_ARRAY_REQUIRE_INDEX:
        case MMAY_MIR_ARRAY_ASSUME_INDEX:
            output->opcode = MMAY_OPCODE_ARRAY_REQUIRE_INDEX;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_ARRAY_GET:
            output->opcode = MMAY_OPCODE_ARRAY_GET;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
            output->opcode = MMAY_OPCODE_ARRAY_GET_IN_BOUNDS;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_ARRAY_SET:
            output->opcode = MMAY_OPCODE_ARRAY_SET;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            output->operands[2].unsigned_value = input->third;
            break;
        case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
            output->opcode = MMAY_OPCODE_ARRAY_SET_IN_BOUNDS;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            output->operands[2].unsigned_value = input->third;
            break;
        case MMAY_MIR_ARRAY_PUSH:
            output->opcode = MMAY_OPCODE_ARRAY_PUSH;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            break;
        case MMAY_MIR_ARRAY_POP:
            output->opcode = MMAY_OPCODE_ARRAY_POP;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_ARRAY_TRUNCATE:
            output->opcode = MMAY_OPCODE_ARRAY_TRUNCATE;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            break;
        case MMAY_MIR_ARRAY_CLEAR:
            output->opcode = MMAY_OPCODE_ARRAY_CLEAR;
            output->operands[0].unsigned_value = input->first;
            break;
        case MMAY_MIR_ARRAY_RESERVE:
            output->opcode = MMAY_OPCODE_ARRAY_RESERVE;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            break;
        case MMAY_MIR_ARRAY_GENERATION:
            output->opcode = MMAY_OPCODE_ARRAY_GENERATION;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
            output->opcode = MMAY_OPCODE_ARRAY_REQUIRE_GENERATION;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_NEW:
            output->opcode = MMAY_OPCODE_COLLECTION_NEW;
            break;
        case MMAY_MIR_COLLECTION_LENGTH:
            output->opcode = MMAY_OPCODE_COLLECTION_LENGTH;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_COLLECTION_CONTAINS:
            output->opcode = MMAY_OPCODE_COLLECTION_CONTAINS;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_GET:
            output->opcode = MMAY_OPCODE_COLLECTION_GET;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_PUT:
            output->opcode = MMAY_OPCODE_COLLECTION_PUT;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            output->operands[2].unsigned_value = input->third;
            break;
        case MMAY_MIR_COLLECTION_ADD:
            output->opcode = MMAY_OPCODE_COLLECTION_ADD;
            output->operands[0].unsigned_value = input->first;
            output->operands[1].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_REMOVE:
            output->opcode = MMAY_OPCODE_COLLECTION_REMOVE;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_CLEAR:
            output->opcode = MMAY_OPCODE_COLLECTION_CLEAR;
            output->operands[0].unsigned_value = input->first;
            break;
        case MMAY_MIR_COLLECTION_GENERATION:
            output->opcode = MMAY_OPCODE_COLLECTION_GENERATION;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_COLLECTION_REQUIRE_GENERATION:
            output->opcode = MMAY_OPCODE_COLLECTION_REQUIRE_GENERATION;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_ENTRY_KEY:
            output->opcode = MMAY_OPCODE_COLLECTION_ENTRY_KEY;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_COLLECTION_ENTRY_VALUE:
            output->opcode = MMAY_OPCODE_COLLECTION_ENTRY_VALUE;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            break;
        case MMAY_MIR_CALL: {
            const MarimayMirCall *call = input->call < function->calls.length
                ? mmay_vector_at_const(&function->calls, input->call)
                : NULL;
            if (call == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
            if (call->result_type == MMAY_TYPE_VOID) {
                output->opcode = MMAY_OPCODE_CALL_DIRECT_VOID;
                output->operands[0].unsigned_value = input->call;
            } else {
                output->opcode = MMAY_OPCODE_CALL_DIRECT;
                output->operands[0].unsigned_value = input->target;
                output->operands[1].unsigned_value = input->call;
            }
            break;
        }
        case MMAY_MIR_INTERFACE_NEW:
            output->opcode = MMAY_OPCODE_INTERFACE_NEW;
            output->operands[1].unsigned_value = input->interface_construction;
            break;
        case MMAY_MIR_CALL_INTERFACE:
            output->opcode = MMAY_OPCODE_CALL_INTERFACE;
            output->operands[0].unsigned_value = input->target;
            output->operands[1].unsigned_value = input->call;
            break;
        case MMAY_MIR_DEFER_REGISTER:
            output->opcode = MMAY_OPCODE_DEFER_REGISTER;
            output->operands[1].unsigned_value = input->call;
            break;
        case MMAY_MIR_DEFER_CALL:
            output->opcode = MMAY_OPCODE_DEFER_CALL;
            output->operands[1].unsigned_value = input->call;
            break;
        case MMAY_MIR_INTEGER_POLICY:
            output->opcode = mmay_mir_bytecode_integer_policy_opcode(
                input->integer_policy, input->integer_operation
            );
            if (output->opcode == MMAY_OPCODE_INVALID) {
                return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
            output->operands[1].unsigned_value = input->first;
            if (input->integer_operation != MMAY_MIR_POLICY_NEGATE) {
                output->operands[2].unsigned_value = input->second;
            }
            break;
        case MMAY_MIR_INTEGER_CONVERT: {
            const MarimayTypeId *target_type = mmay_vector_at_const(
                &function->register_types, input->target
            );
            if (target_type == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
            output->opcode = *target_type == input->integer_target_type
                ? MMAY_OPCODE_INTEGER_CONVERT_TOTAL
                : MMAY_OPCODE_INTEGER_CONVERT_OPTION;
            output->operands[1].unsigned_value = input->first;
            break;
        }
        case MMAY_MIR_IO_PRINTLN:
            output->opcode = MMAY_OPCODE_IO_PRINTLN;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_IO_WRITE:
            output->opcode = MMAY_OPCODE_IO_WRITE;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->io_operation;
            break;
        case MMAY_MIR_IO_FLUSH:
            output->opcode = MMAY_OPCODE_IO_FLUSH;
            output->operands[1].signed_value = (int64_t)input->io_operation;
            break;
        case MMAY_MIR_IO_READ_LINE:
            output->opcode = MMAY_OPCODE_IO_READ_LINE;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].signed_value = (int64_t)input->option_enum_id;
            break;
        case MMAY_MIR_TEST_OUTPUT_BEGIN:
            output->opcode = MMAY_OPCODE_TEST_OUTPUT_BEGIN;
            output->operands[1].unsigned_value = input->first;
            break;
        case MMAY_MIR_TEST_OUTPUT_END:
            output->opcode = MMAY_OPCODE_TEST_OUTPUT_END;
            output->operands[1].unsigned_value = input->first;
            break;
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_terminator(
    const MarimayMirFunction *function,
    const MarimayMirTerminator *input,
    uint64_t source,
    MarimayBytecodeInstruction *output
) {
    memset(output, 0, sizeof(*output));
    if (!mmay_mir_bytecode_span(input->span, source, &output->span)) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    switch (input->kind) {
        case MMAY_MIR_RETURN:
            if (function->result_type == MMAY_TYPE_VOID) {
                output->opcode = MMAY_OPCODE_RETURN_VOID;
            } else {
                output->opcode = MMAY_OPCODE_RETURN;
                output->operands[0].unsigned_value = input->value;
            }
            return MMAY_MIR_BYTECODE_LOWER_OK;
        case MMAY_MIR_GOTO:
            output->opcode = MMAY_OPCODE_JUMP;
            output->operands[0].unsigned_value = input->target;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        case MMAY_MIR_BRANCH:
            output->opcode = MMAY_OPCODE_BRANCH_BOOL;
            output->operands[0].unsigned_value = input->value;
            output->operands[1].unsigned_value = input->target;
            output->operands[2].unsigned_value = input->otherwise;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        case MMAY_MIR_PANIC:
            output->opcode = MMAY_OPCODE_PANIC_STRING;
            output->operands[0].unsigned_value = input->value;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        case MMAY_MIR_CALL_NEVER:
            output->opcode = MMAY_OPCODE_CALL_DIRECT_NEVER;
            output->operands[0].unsigned_value = input->call;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        case MMAY_MIR_TERMINATOR_UNSET:
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
    }
    return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
}

MarimayMirBytecodeLowerStatus mmay_mir_lower_bytecode_function(
    const MarimayMirFunction *input,
    MarimayAllocator allocator,
    MarimayMirBytecodeLowerOptions options,
    MarimayBytecodeFunction *output
) {
    MarimayBytecodeFunction lowered;
    MarimayBytecodeType result_type;
    MarimayBytecodeType *parameter_types = NULL;
    size_t parameter_bytes = 0;
    size_t lowered_instruction_count = 0;
    size_t array_set_count = 0;
    size_t index = 0;
    bool generic_arrays = false;
    MarimayMirBytecodeLowerStatus status = MMAY_MIR_BYTECODE_LOWER_OK;
    if (output == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (output->initialized) {
        mmay_bytecode_function_dispose(output);
    } else {
        memset(output, 0, sizeof(*output));
    }
    memset(&lowered, 0, sizeof(lowered));
    if (input == NULL || !input->initialized
        || !mmay_mir_bytecode_options_valid(allocator, options)) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    {
        MarimayMirVerifyStatus verify_status = mmay_mir_verify(input);
        if (verify_status == MMAY_MIR_VERIFY_ALLOCATION_FAILED) {
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        if (verify_status == MMAY_MIR_VERIFY_LIMIT_REACHED) {
            return MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED;
        }
        if (verify_status != MMAY_MIR_VERIFY_OK) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
    }
    if (!mmay_mir_bytecode_type(
            input,
            input->result_type,
            true,
            true,
            &result_type
        )) {
        return MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
    }
    for (index = 0; index < input->register_types.length; index += 1) {
        const MarimayTypeId *input_type = mmay_vector_at_const(
            &input->register_types,
            index
        );
        MarimayBytecodeType ignored;
        if (input_type == NULL
            || !mmay_mir_bytecode_type(
                input,
                *input_type,
                index >= input->parameter_count,
                false,
                &ignored
            )) {
            return MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
        }
    }
    for (index = 0; index < input->blocks.length; index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&input->blocks, index);
        size_t instruction_index = 0;
        if (block == NULL) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && (instruction->kind == MMAY_MIR_ARRAY_SET
                    || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
                    || instruction->kind == MMAY_MIR_RECORD_SET)
                && !mmay_size_add(array_set_count, 1, &array_set_count)) {
                return MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED;
            }
        }
    }
    if (!mmay_size_add(
            input->instruction_count,
            input->blocks.length,
            &lowered_instruction_count
        ) || !mmay_size_add(
            lowered_instruction_count, array_set_count,
            &lowered_instruction_count
        ) || input->register_types.length > options.function_options.max_registers
        || input->blocks.length > options.function_options.max_blocks
        || lowered_instruction_count > options.function_options.max_instructions
        || input->calls.length > options.function_options.max_calls
        || input->call_arguments.length
            > options.function_options.max_call_arguments
        || input->string_constants.length
            > options.function_options.max_string_constants
        || input->string_bytes > options.function_options.max_string_bytes
        || input->array_constructions.length
            > options.function_options.max_array_constructions
        || input->array_arguments.length
            > options.function_options.max_array_arguments
        || input->collections.length
            > options.function_options.max_collection_layouts) {
        return MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED;
    }
    if (!mmay_size_multiply(
            input->parameter_count,
            sizeof(MarimayBytecodeType),
            &parameter_bytes
        )) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (parameter_bytes != 0) {
        parameter_types = mmay_allocate(&allocator, parameter_bytes);
        if (parameter_types == NULL) {
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        for (index = 0; index < input->parameter_count; index += 1) {
            const MarimayTypeId *input_type = mmay_vector_at_const(
                &input->register_types,
                index
            );
            if (!mmay_mir_bytecode_type(
                    input,
                    *input_type,
                    false,
                    false,
                    &parameter_types[index]
                )) {
                status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
                goto cleanup;
            }
        }
    }
    if (!mmay_bytecode_function_init(
            &lowered,
            allocator,
            result_type,
            options.source_count,
            parameter_types,
            input->parameter_count,
            options.function_options
        )) {
        status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = 0; index < input->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &input->arrays, index
        );
        MarimayBytecodeType array_type = MMAY_BYTECODE_TYPE_INVALID;
        if (array == NULL || !mmay_mir_bytecode_type(
                input, array->type, false, false, &array_type
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (array_type == MMAY_BYTECODE_TYPE_ARRAY) {
            generic_arrays = true;
        }
    }
    if (generic_arrays && !mmay_vector_reserve(
            &lowered.array_types, input->arrays.length
        )) {
        status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = 0; index < input->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &input->arrays, index
        );
        MarimayBytecodeType element_type = MMAY_BYTECODE_TYPE_INVALID;
        uint32_t array_layout = MMAY_BYTECODE_NO_LAYOUT;
        uint32_t stored_layout = UINT32_MAX;
        if (!generic_arrays) {
            continue;
        }
        if (array == NULL || !mmay_mir_bytecode_type(
                input, array->element_type, false, false, &element_type
            ) || (mmay_bytecode_type_requires_layout(element_type)
                && (array_layout = mmay_mir_bytecode_layout_for_type(
                    input, array->element_type
                )) == MMAY_BYTECODE_NO_LAYOUT)
            || !mmay_bytecode_add_array_type(
                &lowered, element_type, array_layout, &stored_layout
            ) || stored_layout != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    lowered.result_layout = mmay_mir_bytecode_layout_for_type(
        input,
        input->result_type
    );
    mmay_deallocate(&allocator, parameter_types, parameter_bytes);
    parameter_types = NULL;
    parameter_bytes = 0;
    for (index = 0; index < input->parameter_count; index += 1) {
        const MarimayTypeId *input_type = mmay_vector_at_const(
            &input->register_types, index
        );
        if (input_type == NULL || !mmay_bytecode_set_register_layout(
                &lowered,
                (uint32_t)index,
                mmay_mir_bytecode_layout_for_type(input, *input_type)
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->string_constants.length; index += 1) {
        const MarimayMirStringConstant *constant = mmay_vector_at_const(
            &input->string_constants,
            index
        );
        uint32_t string_id = UINT32_MAX;
        if (constant == NULL || !mmay_bytecode_add_string_constant(
                &lowered,
                constant->value.data,
                constant->value.length,
                &string_id
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if ((size_t)string_id != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = input->parameter_count;
        index < input->register_types.length; index += 1) {
        const MarimayTypeId *input_type = mmay_vector_at_const(
            &input->register_types,
            index
        );
        MarimayBytecodeType type;
        uint32_t register_id = UINT32_MAX;
        if (!mmay_mir_bytecode_type(
                input, *input_type, true, false, &type
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (!mmay_bytecode_add_register(&lowered, type, &register_id)) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (register_id != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (!mmay_bytecode_set_register_layout(
                &lowered,
                register_id,
                mmay_mir_bytecode_layout_for_type(input, *input_type)
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    status = mmay_mir_bytecode_lower_nominals(input, &lowered, allocator);
    if (status != MMAY_MIR_BYTECODE_LOWER_OK) {
        goto cleanup;
    }
    if (!mmay_vector_reserve(
            &lowered.collection_types, input->collections.length
        )) {
        status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = 0; index < input->collections.length; index += 1) {
        const MarimayMirCollection *collection = mmay_vector_at_const(
            &input->collections, index
        );
        MarimayBytecodeType key_type = MMAY_BYTECODE_TYPE_INVALID;
        MarimayBytecodeType value_type = MMAY_BYTECODE_TYPE_INVALID;
        uint32_t stored = UINT32_MAX;
        if (collection == NULL || !mmay_mir_bytecode_type(
                input, collection->key_type, false, false, &key_type
            ) || (!collection->is_set && !mmay_mir_bytecode_type(
                input, collection->value_type, false, false, &value_type
            )) || !mmay_bytecode_add_collection_type(
                &lowered,
                key_type,
                mmay_mir_bytecode_layout_for_type(
                    input, collection->key_type
                ),
                value_type,
                collection->is_set ? MMAY_BYTECODE_NO_LAYOUT
                    : mmay_mir_bytecode_layout_for_type(
                        input, collection->value_type
                    ),
                collection->is_set,
                &stored
            ) || stored != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->array_constructions.length; index += 1) {
        const MarimayMirArrayConstruction *construction = mmay_vector_at_const(
            &input->array_constructions, index
        );
        const MarimayMirArray *array = construction != NULL
                && construction->array < input->arrays.length
            ? mmay_vector_at_const(&input->arrays, construction->array)
            : NULL;
        MarimayBytecodeArrayArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        MarimayBytecodeType array_type;
        MarimayBytecodeArrayConstructionId construction_id =
            MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
        if (construction == NULL || array == NULL
            || !mmay_mir_bytecode_type(
                input, array->type, false, false, &array_type
            ) || !mmay_size_multiply(
                construction->argument_count,
                sizeof(MarimayBytecodeArrayArgument),
                &argument_bytes
            )) {
            status = array != NULL
                ? MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE
                : MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (argument_bytes != 0) {
            arguments = mmay_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayMirArrayArgument *argument = mmay_vector_at_const(
                &input->array_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || arguments == NULL
                || !mmay_mir_bytecode_type(
                    input, argument->type, false, false,
                    &arguments[argument_index].type
                )) {
                status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
                break;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout = mmay_mir_bytecode_layout_for_type(
                input, argument->type
            );
        }
        if (status == MMAY_MIR_BYTECODE_LOWER_OK
            && !mmay_bytecode_add_array_construction_with_layout(
                &lowered,
                array_type,
                array_type == MMAY_BYTECODE_TYPE_ARRAY
                    ? mmay_mir_bytecode_layout_for_type(input, array->type)
                    : MMAY_BYTECODE_NO_LAYOUT,
                arguments,
                construction->argument_count, &construction_id
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_deallocate(&allocator, arguments, argument_bytes);
        if (status != MMAY_MIR_BYTECODE_LOWER_OK) {
            goto cleanup;
        }
        if ((size_t)construction_id != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->calls.length; index += 1) {
        const MarimayMirCall *call = mmay_vector_at_const(&input->calls, index);
        MarimayBytecodeCallArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        MarimayBytecodeType call_result_type;
        MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
        if (call == NULL
            || !mmay_mir_bytecode_type(
                input,
                call->result_type,
                true,
                true,
                &call_result_type
            ) || !mmay_size_multiply(
                call->argument_count,
                sizeof(MarimayBytecodeCallArgument),
                &argument_bytes
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        if (argument_bytes != 0) {
            arguments = mmay_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
        for (argument_index = 0;
            argument_index < call->argument_count;
            argument_index += 1) {
            const MarimayMirCallArgument *argument = mmay_vector_at_const(
                &input->call_arguments,
                call->argument_start + argument_index
            );
            if (arguments == NULL || argument == NULL
                || !mmay_mir_bytecode_type(
                    input,
                    argument->type,
                    false,
                    false,
                    &arguments[argument_index].type
                )) {
                status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
                break;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout = mmay_mir_bytecode_layout_for_type(
                input, argument->type
            );
        }
        if (status == MMAY_MIR_BYTECODE_LOWER_OK
            && !mmay_bytecode_add_call(
                &lowered,
                call->callee,
                call_result_type,
                arguments,
                call->argument_count,
                &call_id
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_deallocate(&allocator, arguments, argument_bytes);
        if (status != MMAY_MIR_BYTECODE_LOWER_OK) {
            goto cleanup;
        }
        if ((size_t)call_id != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
        {
            MarimayBytecodeCall *lowered_call = mmay_vector_at(&lowered.calls, call_id);
            if (lowered_call == NULL) {
                status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
                goto cleanup;
            }
            lowered_call->result_layout = mmay_mir_bytecode_layout_for_type(
                input, call->result_type
            );
            lowered_call->interface_slot = call->interface_slot;
        }
    }
    for (index = 0; index < input->blocks.length; index += 1) {
        uint32_t block_id = UINT32_MAX;
        if (!mmay_bytecode_add_block(&lowered, &block_id)) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (block_id != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    for (index = 0; index < input->blocks.length; index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&input->blocks, index);
        size_t instruction_index = 0;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                instruction_index
            );
            MarimayBytecodeInstruction lowered_instruction;
            status = mmay_mir_bytecode_instruction(
                input,
                instruction,
                options.source,
                &lowered_instruction
            );
            if (status != MMAY_MIR_BYTECODE_LOWER_OK
                || !mmay_bytecode_append_instruction(
                    &lowered,
                    (uint32_t)index,
                    &lowered_instruction
                )) {
                if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
                    status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                }
                goto cleanup;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_SET
                || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
                || instruction->kind == MMAY_MIR_RECORD_SET) {
                MarimayBytecodeInstruction copy;
                memset(&copy, 0, sizeof(copy));
                copy.opcode = MMAY_OPCODE_COPY;
                copy.operands[0].unsigned_value = instruction->target;
                copy.operands[1].unsigned_value = instruction->kind
                    == MMAY_MIR_ARRAY_SET
                        || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
                    ? instruction->third
                    : instruction->second;
                copy.span = lowered_instruction.span;
                if (!mmay_bytecode_append_instruction(
                        &lowered, (uint32_t)index, &copy
                    )) {
                    status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
        }
        {
            MarimayBytecodeInstruction terminator;
            status = mmay_mir_bytecode_terminator(
                input,
                &block->terminator,
                options.source,
                &terminator
            );
            if (status != MMAY_MIR_BYTECODE_LOWER_OK
                || !mmay_bytecode_append_instruction(
                    &lowered,
                    (uint32_t)index,
                    &terminator
                )) {
                if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
                    status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
                }
                goto cleanup;
            }
        }
    }
    {
        MarimayBytecodeVerifyStatus verify_status = mmay_bytecode_verify(&lowered);
        if (verify_status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (verify_status == MMAY_BYTECODE_VERIFY_LIMIT_REACHED) {
            status = MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED;
            goto cleanup;
        }
        if (verify_status != MMAY_BYTECODE_VERIFY_OK) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup;
        }
    }
    *output = lowered;
    memset(&lowered, 0, sizeof(lowered));

cleanup:
    mmay_deallocate(&allocator, parameter_types, parameter_bytes);
    mmay_bytecode_function_dispose(&lowered);
    return status;
}

static int mmay_mir_bytecode_compare_function_symbols(
    const void *left,
    const void *right
) {
    const MarimayMirFunction *const left_function =
        *(const MarimayMirFunction *const *)left;
    const MarimayMirFunction *const right_function =
        *(const MarimayMirFunction *const *)right;
    return memcmp(
        left_function->symbol.bytes,
        right_function->symbol.bytes,
        sizeof(left_function->symbol.bytes)
    );
}

MarimayMirBytecodeLowerStatus mmay_mir_lower_bytecode_program(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimaySymbolId entry,
    MarimayAllocator allocator,
    MarimayMirBytecodeProgramLowerOptions options,
    MarimayBytecodeProgram *output
) {
    MarimayBytecodeProgram lowered;
    const MarimayMirFunction **ordered = NULL;
    size_t ordered_bytes = 0;
    size_t index = 0;
    MarimayMirBytecodeLowerStatus status = MMAY_MIR_BYTECODE_LOWER_OK;
    if (output == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (output->initialized) {
        mmay_bytecode_program_dispose(output);
    } else {
        memset(output, 0, sizeof(*output));
    }
    memset(&lowered, 0, sizeof(lowered));
    if ((functions == NULL && function_count != 0) || function_count == 0
        || function_count > options.program_options.max_functions
        || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL
        || options.source_count == 0 || options.source >= options.source_count
        || options.source_count
            > options.program_options.function_options.max_source_files
        || !mmay_size_multiply(
            function_count,
            sizeof(const MarimayMirFunction *),
            &ordered_bytes
        )) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    if (!mmay_bytecode_program_init(
            &lowered,
            allocator,
            options.source_count,
            options.program_options
        )) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    ordered = mmay_allocate(&allocator, ordered_bytes);
    if (ordered == NULL) {
        mmay_bytecode_program_dispose(&lowered);
        return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
    }
    for (index = 0; index < function_count; index += 1) {
        if (functions[index] == NULL || !functions[index]->initialized) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
            goto cleanup_program;
        }
        ordered[index] = functions[index];
    }
    qsort(
        ordered,
        function_count,
        sizeof(const MarimayMirFunction *),
        mmay_mir_bytecode_compare_function_symbols
    );
    for (index = 1; index < function_count; index += 1) {
        if (mmay_mir_bytecode_compare_function_symbols(
                &ordered[index - 1],
                &ordered[index]
            ) == 0) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup_program;
        }
    }
    for (index = 0; index < function_count; index += 1) {
        MarimayBytecodeFunction function;
        MarimayMirBytecodeLowerOptions function_options;
        uint32_t function_id = UINT32_MAX;
        memset(&function, 0, sizeof(function));
        function_options.source = options.source;
        function_options.source_count = options.source_count;
        function_options.function_options =
            options.program_options.function_options;
        status = mmay_mir_lower_bytecode_function(
            ordered[index],
            allocator,
            function_options,
            &function
        );
        if (status == MMAY_MIR_BYTECODE_LOWER_OK
            && !mmay_bytecode_program_take_function(
                &lowered,
                ordered[index]->symbol,
                &function,
                &function_id
            )) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_bytecode_function_dispose(&function);
        if (status != MMAY_MIR_BYTECODE_LOWER_OK) {
            goto cleanup_program;
        }
        if ((size_t)function_id != index) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            goto cleanup_program;
        }
    }
    if (!mmay_bytecode_program_set_entry(&lowered, entry)) {
        status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        goto cleanup_program;
    }
    {
        MarimayBytecodeProgramVerifyStatus verify_status =
            mmay_bytecode_program_verify(&lowered);
        if (verify_status == MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED) {
            status = MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        } else if (verify_status == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED) {
            status = MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED;
        } else if (verify_status != MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
            status = MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (status != MMAY_MIR_BYTECODE_LOWER_OK) {
            goto cleanup_program;
        }
    }
    *output = lowered;
    memset(&lowered, 0, sizeof(lowered));

cleanup_program:
    mmay_bytecode_program_dispose(&lowered);
    mmay_deallocate(&allocator, ordered, ordered_bytes);
    return status;
}
