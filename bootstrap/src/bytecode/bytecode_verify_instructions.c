#include "mmay/bytecode/bytecode.h"
#include <limits.h>
#include <string.h>
#include "bytecode_internal.h"
static const MarimayBytecodeType *mmay_bytecode_register_type(
    const MarimayBytecodeFunction *function,
    uint64_t register_id
) {
    if (function == NULL || register_id >= function->register_types.length) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->register_types,
        (size_t)register_id
    );
}
static MarimayBytecodeVerifyStatus mmay_bytecode_require_assigned(
    const unsigned char *assigned,
    size_t register_count,
    uint64_t register_id
);
static bool mmay_bytecode_register_matches(const MarimayBytecodeFunction *function, uint64_t register_id, MarimayBytecodeType expected, uint32_t expected_layout);
const MarimayBytecodeInstruction *mmay_bytecode_block_terminator(
    const MarimayBytecodeBlock *block
) {
    if (block == NULL || block->instructions.length == 0) {
        return NULL;
    }
    return mmay_vector_at_const(
        &block->instructions,
        block->instructions.length - 1
    );
}
bool mmay_bytecode_opcode_writes_register(MarimayOpcode opcode) {
    if (mmay_bytecode_opcode_is_numeric(opcode)
        || mmay_bytecode_opcode_is_text(opcode)
        || mmay_bytecode_opcode_is_fs(opcode)
        || opcode == MMAY_OPCODE_IO_READ_BYTES
        || opcode == MMAY_OPCODE_IO_WRITE_BYTES) {
        return true;
    }
    switch (opcode) {
        case MMAY_OPCODE_CONST_I64:
        case MMAY_OPCODE_CONST_INTEGER:
        case MMAY_OPCODE_CONST_F64:
        case MMAY_OPCODE_CONST_BOOL:
        case MMAY_OPCODE_COPY:
        case MMAY_OPCODE_I64_ADD_CHECKED: case MMAY_OPCODE_I64_ADD_PROVEN:
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED: case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED: case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
        case MMAY_OPCODE_I64_DIVIDE_CHECKED:
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
        case MMAY_OPCODE_I64_NEGATE_CHECKED:
        case MMAY_OPCODE_I64_BITWISE_NOT:
        case MMAY_OPCODE_F64_NEGATE:
        case MMAY_OPCODE_BOOL_NOT:
        case MMAY_OPCODE_I64_EQUAL:
        case MMAY_OPCODE_BOOL_EQUAL:
        case MMAY_OPCODE_I64_LESS:
        case MMAY_OPCODE_I64_LESS_EQUAL:
        case MMAY_OPCODE_I64_BITWISE_AND:
        case MMAY_OPCODE_I64_BITWISE_XOR:
        case MMAY_OPCODE_I64_BITWISE_OR:
        case MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case MMAY_OPCODE_I64_SHIFT_RIGHT:
        case MMAY_OPCODE_F64_ADD:
        case MMAY_OPCODE_F64_SUBTRACT:
        case MMAY_OPCODE_F64_MULTIPLY:
        case MMAY_OPCODE_F64_DIVIDE:
        case MMAY_OPCODE_F64_EQUAL:
        case MMAY_OPCODE_F64_LESS:
        case MMAY_OPCODE_F64_LESS_EQUAL:
        case MMAY_OPCODE_CONST_STRING:
        case MMAY_OPCODE_STRING_EQUAL:
        case MMAY_OPCODE_CALL_DIRECT:
        case MMAY_OPCODE_ARRAY_NEW:
        case MMAY_OPCODE_ARRAY_LENGTH:
        case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
        case MMAY_OPCODE_ARRAY_GET:
        case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_POP:
        case MMAY_OPCODE_ARRAY_GENERATION:
        case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
        case MMAY_OPCODE_RECORD_NEW:
        case MMAY_OPCODE_RECORD_GET:
        case MMAY_OPCODE_RECORD_IDENTITY_SAME:
        case MMAY_OPCODE_ENUM_NEW:
        case MMAY_OPCODE_ENUM_IS_VARIANT:
        case MMAY_OPCODE_ENUM_REQUIRE_VARIANT:
        case MMAY_OPCODE_ENUM_GET_PAYLOAD:
        case MMAY_OPCODE_INTERFACE_NEW:
        case MMAY_OPCODE_CALL_INTERFACE:
        case MMAY_OPCODE_INTEGER_CHECKED_ADD:
        case MMAY_OPCODE_INTEGER_CHECKED_SUBTRACT:
        case MMAY_OPCODE_INTEGER_CHECKED_MULTIPLY:
        case MMAY_OPCODE_INTEGER_CHECKED_DIVIDE:
        case MMAY_OPCODE_INTEGER_CHECKED_REMAINDER:
        case MMAY_OPCODE_INTEGER_CHECKED_NEGATE:
        case MMAY_OPCODE_INTEGER_CHECKED_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_WRAPPING_ADD:
        case MMAY_OPCODE_INTEGER_WRAPPING_SUBTRACT:
        case MMAY_OPCODE_INTEGER_WRAPPING_MULTIPLY:
        case MMAY_OPCODE_INTEGER_WRAPPING_DIVIDE:
        case MMAY_OPCODE_INTEGER_WRAPPING_REMAINDER:
        case MMAY_OPCODE_INTEGER_WRAPPING_NEGATE:
        case MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_SATURATING_ADD:
        case MMAY_OPCODE_INTEGER_SATURATING_SUBTRACT:
        case MMAY_OPCODE_INTEGER_SATURATING_MULTIPLY:
        case MMAY_OPCODE_INTEGER_SATURATING_DIVIDE:
        case MMAY_OPCODE_INTEGER_SATURATING_REMAINDER:
        case MMAY_OPCODE_INTEGER_SATURATING_NEGATE:
        case MMAY_OPCODE_INTEGER_SATURATING_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_CONVERT_TOTAL:
        case MMAY_OPCODE_INTEGER_CONVERT_OPTION:
        case MMAY_OPCODE_FORMAT_F32:
        case MMAY_OPCODE_FORMAT_F64:
        case MMAY_OPCODE_FORMAT_BOOL:
        case MMAY_OPCODE_FORMAT_INTEGER:
        case MMAY_OPCODE_STRING_CONCAT:
        case MMAY_OPCODE_PARSE_INTEGER:
        case MMAY_OPCODE_IO_PRINTLN:
        case MMAY_OPCODE_IO_WRITE:
        case MMAY_OPCODE_IO_FLUSH:
        case MMAY_OPCODE_IO_READ_LINE:
        case MMAY_OPCODE_TEST_OUTPUT_BEGIN:
        case MMAY_OPCODE_TEST_OUTPUT_END:
        case MMAY_OPCODE_DEFER_REGISTER:
        case MMAY_OPCODE_DEFER_CALL:
            return true;
        case MMAY_OPCODE_INVALID:
        case MMAY_OPCODE_JUMP:
        case MMAY_OPCODE_BRANCH_BOOL:
        case MMAY_OPCODE_RETURN:
        case MMAY_OPCODE_RETURN_VOID:
        case MMAY_OPCODE_CALL_DIRECT_VOID:
        case MMAY_OPCODE_PANIC_STRING:
        case MMAY_OPCODE_CALL_DIRECT_NEVER:
        case MMAY_OPCODE_ARRAY_SET:
        case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_PUSH:
        case MMAY_OPCODE_ARRAY_TRUNCATE:
        case MMAY_OPCODE_ARRAY_CLEAR:
        case MMAY_OPCODE_ARRAY_RESERVE:
        case MMAY_OPCODE_RECORD_SET:
            return false;
        default:
            return false;
    }
    return false;
}
static MarimayBytecodeVerifyStatus mmay_bytecode_verify_defer_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t marker = instruction->operands[0].unsigned_value;
    uint64_t call_wire = instruction->operands[1].unsigned_value;
    const MarimayBytecodeCall *call = call_wire < function->calls.length
        ? mmay_bytecode_call(function, (MarimayBytecodeCallId)call_wire)
        : NULL;
    const MarimayBytecodeType *marker_type = mmay_bytecode_register_type(
        function, marker
    );
    size_t argument_index = 0;
    if (call == NULL || marker_type == NULL
        || *marker_type != MMAY_BYTECODE_TYPE_BOOL
        || call->result_type != MMAY_BYTECODE_TYPE_VOID) {
        return marker_type == NULL
            ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (call->interface_slot != UINT32_MAX) {
        const MarimayBytecodeCallArgument *receiver = call->argument_count != 0
            ? mmay_bytecode_call_argument(function, call->argument_start)
            : NULL;
        const MarimayBytecodeInterfaceLayout *layout = receiver != NULL
                && receiver->type == MMAY_BYTECODE_TYPE_INTERFACE
            ? mmay_bytecode_interface_layout(function, receiver->layout)
            : NULL;
        if (layout == NULL || call->interface_slot >= layout->method_count) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_bytecode_call_argument(
            function, call->argument_start + argument_index
        );
        MarimayBytecodeVerifyStatus status = argument != NULL
            ? mmay_bytecode_require_assigned(
                assigned, function->register_types.length,
                argument->register_id
            )
            : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (!mmay_bytecode_register_matches(
                function, argument->register_id,
                argument->type, argument->layout
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    assigned[(size_t)marker] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
void mmay_bytecode_transfer_assignments(
    const MarimayBytecodeBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t instruction_index = 0;
    if (block == NULL || assigned == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index + 1 < block->instructions.length;
        instruction_index += 1) {
        const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
            &block->instructions,
            instruction_index
        );
        if (instruction != NULL
            && mmay_bytecode_opcode_writes_register(instruction->opcode)
            && instruction->operands[0].unsigned_value < register_count) {
            assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
        }
    }
}
static MarimayBytecodeVerifyStatus mmay_bytecode_require_assigned(
    const unsigned char *assigned,
    size_t register_count,
    uint64_t register_id
) {
    if (register_id >= register_count) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return assigned[(size_t)register_id] != 0
        ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}
static bool mmay_bytecode_register_matches(
    const MarimayBytecodeFunction *function,
    uint64_t register_id,
    MarimayBytecodeType type,
    uint32_t layout
) {
    const MarimayBytecodeType *actual = mmay_bytecode_register_type(
        function,
        register_id
    );
    return actual != NULL && *actual == type
        && mmay_bytecode_type_layout_is_valid(function, type, layout)
        && mmay_bytecode_register_layout(
            function,
            (uint32_t)register_id
        ) == layout;
}
static const MarimayBytecodeEnumVariant *mmay_bytecode_enum_variant(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant
) {
    return function != NULL && layout != NULL && variant < layout->variant_count
        ? mmay_vector_at_const(
            &function->enum_variants,
            layout->variant_start + variant
        )
        : NULL;
}
static const MarimayBytecodeEnumPayload *mmay_bytecode_enum_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant,
    size_t field
) {
    return function != NULL && layout != NULL && variant != NULL
            && field < variant->payload_count
            && variant->payload_start <= layout->payload_count
            && field <= layout->payload_count - variant->payload_start
        ? mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + variant->payload_start + field
        )
        : NULL;
}
static MarimayBytecodeVerifyStatus mmay_bytecode_verify_record_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t record_register = instruction->operands[1].unsigned_value;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (instruction->opcode == MMAY_OPCODE_RECORD_NEW) {
        uint64_t construction_wire = instruction->operands[1].unsigned_value;
        const MarimayBytecodeRecordConstruction *construction =
            construction_wire < function->record_constructions.length
            ? mmay_bytecode_record_construction(
                function,
                (MarimayBytecodeRecordConstructionId)construction_wire
            )
            : NULL;
        const MarimayBytecodeRecordLayout *layout = construction != NULL
            ? mmay_bytecode_record_layout(function, construction->record)
            : NULL;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL
            || !mmay_bytecode_register_matches(
                function,
                target,
                MMAY_BYTECODE_TYPE_RECORD,
                construction->record
            )) {
            return construction == NULL || layout == NULL
                ? MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayBytecodeRecordArgument *argument = mmay_vector_at_const(
                &function->record_arguments,
                construction->argument_start + argument_index
            );
            status = argument != NULL
                ? mmay_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    argument->register_id
                )
                : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            if (!mmay_bytecode_register_matches(
                    function,
                    argument->register_id,
                    argument->type,
                    argument->layout
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        }
        assigned[(size_t)target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    status = mmay_bytecode_require_assigned(
        assigned,
        function->register_types.length,
        record_register
    );
    if (status != MMAY_BYTECODE_VERIFY_OK) {
        return status;
    }
    {
        uint32_t record_id = mmay_bytecode_register_layout(
            function,
            (uint32_t)record_register
        );
        const MarimayBytecodeRecordLayout *layout = mmay_bytecode_record_layout(
            function,
            record_id
        );
        int64_t field_wire = instruction->operands[2].signed_value;
        const MarimayBytecodeRecordField *field =
            instruction->opcode != MMAY_OPCODE_RECORD_IDENTITY_SAME
                && field_wire >= 0
            ? mmay_bytecode_record_field(
                function,
                layout,
                (size_t)field_wire
            )
            : NULL;
        if (layout == NULL || !mmay_bytecode_register_matches(
                function,
                record_register,
                MMAY_BYTECODE_TYPE_RECORD,
                record_id
            )) {
            return layout == NULL
                ? MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == MMAY_OPCODE_RECORD_IDENTITY_SAME) {
            status = mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                instruction->operands[2].unsigned_value
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            if (!mmay_bytecode_register_matches(
                    function,
                    instruction->operands[2].unsigned_value,
                    MMAY_BYTECODE_TYPE_RECORD,
                    record_id
                ) || !mmay_bytecode_register_matches(
                    function,
                    target,
                    MMAY_BYTECODE_TYPE_BOOL,
                    MMAY_BYTECODE_NO_LAYOUT
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            assigned[(size_t)target] = 1;
            return MMAY_BYTECODE_VERIFY_OK;
        } else if (field == NULL) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        } else if (instruction->opcode == MMAY_OPCODE_RECORD_SET) {
            status = mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                target
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            return field->is_mutable && mmay_bytecode_register_matches(
                    function,
                    target,
                    field->type,
                    field->layout
                )
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == MMAY_OPCODE_RECORD_GET) {
            if (!mmay_bytecode_register_matches(
                    function,
                    target,
                    field->type,
                    field->layout
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else {
            return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
        }
    }
    assigned[(size_t)target] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
static MarimayBytecodeVerifyStatus mmay_bytecode_verify_enum_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    if (instruction->opcode == MMAY_OPCODE_ENUM_NEW) {
        uint64_t construction_wire = instruction->operands[1].unsigned_value;
        const MarimayBytecodeEnumConstruction *construction =
            construction_wire < function->enum_constructions.length
            ? mmay_bytecode_enum_construction(
                function,
                (MarimayBytecodeEnumConstructionId)construction_wire
            )
            : NULL;
        const MarimayBytecodeEnumLayout *layout = construction != NULL
            ? mmay_bytecode_enum_layout(function, construction->enum_id)
            : NULL;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL
            || !mmay_bytecode_register_matches(
                function,
                target,
                MMAY_BYTECODE_TYPE_ENUM,
                construction->enum_id
            )) {
            return construction == NULL || layout == NULL
                ? MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayBytecodeEnumArgument *argument = mmay_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + argument_index
            );
            MarimayBytecodeVerifyStatus status = argument != NULL
                ? mmay_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    argument->register_id
                )
                : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            if (!mmay_bytecode_register_matches(
                    function,
                    argument->register_id,
                    argument->type,
                    argument->layout
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        }
        assigned[(size_t)target] = 1;
        return MMAY_BYTECODE_VERIFY_OK;
    }
    {
        uint64_t enum_register = instruction->operands[1].unsigned_value;
        MarimayBytecodeVerifyStatus status = mmay_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            enum_register
        );
        uint32_t enum_id = mmay_bytecode_register_layout(
            function,
            (uint32_t)enum_register
        );
        const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
            function,
            enum_id
        );
        uint64_t selector = (uint64_t)instruction->operands[2].signed_value;
        size_t variant_index = instruction->opcode == MMAY_OPCODE_ENUM_GET_PAYLOAD
            ? (size_t)(selector >> 32)
            : (size_t)instruction->operands[2].signed_value;
        const MarimayBytecodeEnumVariant *variant =
            instruction->operands[2].signed_value >= 0
                || instruction->opcode == MMAY_OPCODE_ENUM_GET_PAYLOAD
            ? mmay_bytecode_enum_variant(function, layout, variant_index)
            : NULL;
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (layout == NULL || variant == NULL) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_bytecode_register_matches(
                function,
                enum_register,
                MMAY_BYTECODE_TYPE_ENUM,
                enum_id
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == MMAY_OPCODE_ENUM_IS_VARIANT) {
            if (!mmay_bytecode_register_matches(
                    function,
                    target,
                    MMAY_BYTECODE_TYPE_BOOL,
                    MMAY_BYTECODE_NO_LAYOUT
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else if (instruction->opcode == MMAY_OPCODE_ENUM_REQUIRE_VARIANT) {
            if (!mmay_bytecode_register_matches(
                    function,
                    target,
                    MMAY_BYTECODE_TYPE_ENUM,
                    enum_id
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else if (instruction->opcode == MMAY_OPCODE_ENUM_GET_PAYLOAD) {
            const MarimayBytecodeEnumPayload *payload =
                mmay_bytecode_enum_payload(
                    function,
                    layout,
                    variant,
                    (size_t)(selector & UINT32_MAX)
                );
            if (payload == NULL) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_bytecode_register_matches(
                    function,
                    target,
                    payload->type,
                    payload->layout
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else {
            return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
        }
    }
    assigned[(size_t)target] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
MarimayBytecodeVerifyStatus mmay_bytecode_verify_call_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    bool returns_void = instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID;
    bool returns_never = instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER;
    uint64_t call_wire = returns_void || returns_never
        ? instruction->operands[0].unsigned_value
        : instruction->operands[1].unsigned_value;
    const MarimayBytecodeCall *call = call_wire < function->calls.length
        ? mmay_vector_at_const(&function->calls, (size_t)call_wire)
        : NULL;
    size_t argument_index = 0;
    if (call == NULL || call->interface_slot != UINT32_MAX) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (returns_void != (call->result_type == MMAY_BYTECODE_TYPE_VOID)
        || returns_never != (call->result_type == MMAY_BYTECODE_TYPE_NEVER)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (!returns_void && !returns_never) {
        uint64_t target = instruction->operands[0].unsigned_value;
        const MarimayBytecodeType *target_type = mmay_bytecode_register_type(
            function,
            target
        );
        if (target_type == NULL) {
            return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
        }
        if (!mmay_bytecode_type_is_register(*target_type)
            || !mmay_bytecode_register_matches(
                function,
                target,
                call->result_type,
                call->result_layout
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_vector_at_const(
            &function->call_arguments,
            call->argument_start + argument_index
        );
        MarimayBytecodeVerifyStatus status = argument != NULL
            ? mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                argument->register_id
            )
            : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (!mmay_bytecode_register_matches(
                function,
                argument->register_id,
                argument->type,
                argument->layout
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    if (!returns_void && !returns_never) {
        assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
    }
    return MMAY_BYTECODE_VERIFY_OK;
}
static MarimayBytecodeVerifyStatus mmay_bytecode_verify_interface_new(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t construction_wire = instruction->operands[1].unsigned_value;
    const MarimayBytecodeInterfaceConstruction *construction =
        construction_wire < function->interface_constructions.length
        ? mmay_bytecode_interface_construction(
            function,
            (MarimayBytecodeInterfaceConstructionId)construction_wire
        )
        : NULL;
    const MarimayBytecodeInterfaceLayout *layout = construction != NULL
        ? mmay_bytecode_interface_layout(function, construction->interface_id)
        : NULL;
    MarimayBytecodeVerifyStatus status;
    if (construction == NULL || layout == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if ((construction->payload_type != MMAY_BYTECODE_TYPE_RECORD
            && construction->payload_type != MMAY_BYTECODE_TYPE_ENUM)
        || !mmay_bytecode_register_matches(
            function,
            target,
            MMAY_BYTECODE_TYPE_INTERFACE,
            construction->interface_id
        ) || !mmay_bytecode_register_matches(
            function,
            construction->payload_register,
            construction->payload_type,
            construction->payload_layout
        )) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    status = mmay_bytecode_require_assigned(
        assigned,
        function->register_types.length,
        construction->payload_register
    );
    if (status != MMAY_BYTECODE_VERIFY_OK) {
        return status;
    }
    assigned[(size_t)target] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
static MarimayBytecodeVerifyStatus mmay_bytecode_verify_interface_call(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t call_wire = instruction->operands[1].unsigned_value;
    const MarimayBytecodeCall *call = call_wire < function->calls.length
        ? mmay_bytecode_call(function, (MarimayBytecodeCallId)call_wire)
        : NULL;
    const MarimayBytecodeCallArgument *receiver = call != NULL
            && call->argument_count != 0
        ? mmay_bytecode_call_argument(function, call->argument_start)
        : NULL;
    const MarimayBytecodeInterfaceLayout *layout = receiver != NULL
            && receiver->type == MMAY_BYTECODE_TYPE_INTERFACE
        ? mmay_bytecode_interface_layout(function, receiver->layout)
        : NULL;
    size_t argument_index = 0;
    if (call == NULL || receiver == NULL || layout == NULL
        || call->interface_slot == UINT32_MAX
        || call->interface_slot >= layout->method_count
        || !mmay_bytecode_type_is_register(call->result_type)) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (!mmay_bytecode_register_matches(
            function,
            target,
            call->result_type,
            call->result_layout
        )) {
        return mmay_bytecode_register_type(function, target) == NULL
            ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_bytecode_call_argument(
            function,
            call->argument_start + argument_index
        );
        MarimayBytecodeVerifyStatus status = argument != NULL
            ? mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                argument->register_id
            )
            : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (!mmay_bytecode_register_matches(
                function,
                argument->register_id,
                argument->type,
                argument->layout
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    assigned[(size_t)target] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
MarimayBytecodeVerifyStatus mmay_bytecode_verify_value_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = 0;
    uint64_t second = 0;
    const MarimayBytecodeType *target_type = NULL;
    const MarimayBytecodeType *first_type = NULL;
    const MarimayBytecodeType *second_type = NULL;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (instruction->opcode == MMAY_OPCODE_CALL_DIRECT
        || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID
        || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER) {
        return mmay_bytecode_verify_call_instruction(
            function,
            instruction,
            assigned
        );
    }
    if (mmay_bytecode_opcode_is_numeric(instruction->opcode)) {
        return mmay_bytecode_verify_numeric_instruction(
            function, instruction, assigned
        );
    }
    if (instruction->opcode == MMAY_OPCODE_ARRAY_PUSH
        || instruction->opcode == MMAY_OPCODE_ARRAY_POP
        || instruction->opcode == MMAY_OPCODE_ARRAY_TRUNCATE
        || instruction->opcode == MMAY_OPCODE_ARRAY_CLEAR
        || instruction->opcode == MMAY_OPCODE_ARRAY_RESERVE
        || instruction->opcode == MMAY_OPCODE_ARRAY_GENERATION
        || instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_GENERATION) {
        bool writes_target = instruction->opcode == MMAY_OPCODE_ARRAY_POP
            || instruction->opcode == MMAY_OPCODE_ARRAY_GENERATION
            || instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_GENERATION;
        uint64_t array = instruction->operands[writes_target ? 1 : 0]
            .unsigned_value;
        uint64_t value = instruction->operands[writes_target ? 2 : 1]
            .unsigned_value;
        const MarimayBytecodeType *array_type = NULL;
        const MarimayBytecodeType *value_type = NULL;
        uint32_t element_layout = MMAY_BYTECODE_NO_LAYOUT;
        MarimayBytecodeType element_type = MMAY_BYTECODE_TYPE_INVALID;
        status = mmay_bytecode_require_assigned(
            assigned, function->register_types.length, array
        );
        if (status == MMAY_BYTECODE_VERIFY_OK
            && instruction->opcode != MMAY_OPCODE_ARRAY_CLEAR
            && instruction->opcode != MMAY_OPCODE_ARRAY_POP
            && instruction->opcode != MMAY_OPCODE_ARRAY_GENERATION) {
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, value
            );
        }
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        array_type = mmay_bytecode_register_type(function, array);
        value_type = mmay_bytecode_register_type(function, value);
        target_type = mmay_bytecode_register_type(
            function, instruction->operands[0].unsigned_value
        );
        if (array_type != NULL) {
            element_type = mmay_bytecode_function_array_element_type(
                function, *array_type,
                mmay_bytecode_register_layout(function, (uint32_t)array),
                &element_layout
            );
        }
        if (array_type == NULL || !mmay_bytecode_type_is_array(*array_type)) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == MMAY_OPCODE_ARRAY_PUSH) {
            return value_type != NULL && *value_type == element_type
                    && mmay_bytecode_register_layout(
                        function, (uint32_t)value
                    ) == element_layout
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == MMAY_OPCODE_ARRAY_POP) {
            status = target_type != NULL && *target_type == element_type
                    && mmay_bytecode_register_layout(
                        function,
                        (uint32_t)instruction->operands[0].unsigned_value
                    ) == element_layout
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            if (status == MMAY_BYTECODE_VERIFY_OK) {
                assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
            }
            return status;
        }
        if (instruction->opcode == MMAY_OPCODE_ARRAY_TRUNCATE
            || instruction->opcode == MMAY_OPCODE_ARRAY_RESERVE) {
            return value_type != NULL && *value_type == MMAY_BYTECODE_TYPE_I64
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == MMAY_OPCODE_ARRAY_CLEAR) {
            return MMAY_BYTECODE_VERIFY_OK;
        }
        if (instruction->opcode == MMAY_OPCODE_ARRAY_GENERATION) {
            status = target_type != NULL
                    && *target_type == MMAY_BYTECODE_TYPE_U64
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            if (status == MMAY_BYTECODE_VERIFY_OK) {
                assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
            }
            return status;
        }
        status = value_type != NULL && *value_type == MMAY_BYTECODE_TYPE_U64
                && target_type != NULL && *target_type == *array_type
                && mmay_bytecode_register_layout(
                    function,
                    (uint32_t)instruction->operands[0].unsigned_value
                ) == mmay_bytecode_register_layout(function, (uint32_t)array)
            ? MMAY_BYTECODE_VERIFY_OK
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        if (status == MMAY_BYTECODE_VERIFY_OK) {
            assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
        }
        return status;
    }
    if (instruction->opcode == MMAY_OPCODE_INTERFACE_NEW) {
        return mmay_bytecode_verify_interface_new(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == MMAY_OPCODE_CALL_INTERFACE) {
        return mmay_bytecode_verify_interface_call(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == MMAY_OPCODE_DEFER_REGISTER
        || instruction->opcode == MMAY_OPCODE_DEFER_CALL) {
        return mmay_bytecode_verify_defer_instruction(
            function, instruction, assigned
        );
    }
    if (instruction->opcode == MMAY_OPCODE_ARRAY_SET
        || instruction->opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS) {
        uint64_t array = instruction->operands[0].unsigned_value;
        uint64_t index = instruction->operands[1].unsigned_value;
        uint64_t value = instruction->operands[2].unsigned_value;
        const MarimayBytecodeType *array_type;
        const MarimayBytecodeType *index_type;
        const MarimayBytecodeType *value_type;
        status = mmay_bytecode_require_assigned(
            assigned, function->register_types.length, array
        );
        if (status == MMAY_BYTECODE_VERIFY_OK) {
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, index
            );
        }
        if (status == MMAY_BYTECODE_VERIFY_OK) {
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, value
            );
        }
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        array_type = mmay_bytecode_register_type(function, array);
        index_type = mmay_bytecode_register_type(function, index);
        value_type = mmay_bytecode_register_type(function, value);
        {
            uint32_t element_layout = MMAY_BYTECODE_NO_LAYOUT;
            MarimayBytecodeType element_type = array_type != NULL
                ? mmay_bytecode_function_array_element_type(
                    function,
                    *array_type,
                    mmay_bytecode_register_layout(function, (uint32_t)array),
                    &element_layout
                ) : MMAY_BYTECODE_TYPE_INVALID;
            return array_type != NULL && index_type != NULL
                && value_type != NULL
                && mmay_bytecode_type_is_array(*array_type)
                && *index_type == MMAY_BYTECODE_TYPE_I64
                && *value_type == element_type
                && mmay_bytecode_register_layout(
                    function, (uint32_t)value
                ) == element_layout
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    if (instruction->opcode == MMAY_OPCODE_RECORD_NEW
        || instruction->opcode == MMAY_OPCODE_RECORD_GET
        || instruction->opcode == MMAY_OPCODE_RECORD_SET
        || instruction->opcode == MMAY_OPCODE_RECORD_IDENTITY_SAME) {
        return mmay_bytecode_verify_record_instruction(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == MMAY_OPCODE_ENUM_NEW
        || instruction->opcode == MMAY_OPCODE_ENUM_IS_VARIANT
        || instruction->opcode == MMAY_OPCODE_ENUM_REQUIRE_VARIANT
        || instruction->opcode == MMAY_OPCODE_ENUM_GET_PAYLOAD) {
        return mmay_bytecode_verify_enum_instruction(
            function,
            instruction,
            assigned
        );
    }
    target_type = mmay_bytecode_register_type(function, target);
    if (target_type == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    if (mmay_bytecode_opcode_is_text(instruction->opcode)) {
        status = mmay_bytecode_verify_text_instruction(
            function, instruction, assigned
        );
        if (status == MMAY_BYTECODE_VERIFY_OK) assigned[(size_t)target] = 1;
        return status;
    }
    if (mmay_bytecode_opcode_is_fs(instruction->opcode)) {
        status = mmay_bytecode_verify_fs_instruction(
            function, instruction, assigned
        );
        if (status == MMAY_BYTECODE_VERIFY_OK) assigned[(size_t)target] = 1;
        return status;
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_CONST_I64:
            status = *target_type == MMAY_BYTECODE_TYPE_I64
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_CONST_INTEGER:
            status = mmay_bytecode_type_is_integer(*target_type)
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_CONST_F64:
            status = *target_type == MMAY_BYTECODE_TYPE_F64
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_CONST_BOOL:
            if (instruction->operands[1].unsigned_value > 1) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == MMAY_BYTECODE_TYPE_BOOL
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_CONST_STRING:
            if (instruction->operands[1].unsigned_value
                    >= function->string_constants.length) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_ARRAY_NEW: {
            uint64_t construction_wire = instruction->operands[1].unsigned_value;
            const MarimayBytecodeArrayConstruction *construction =
                construction_wire < function->array_constructions.length
                ? mmay_bytecode_array_construction(
                    function,
                    (MarimayBytecodeArrayConstructionId)construction_wire
                )
                : NULL;
            size_t argument_index = 0;
            if (construction == NULL
                || *target_type != construction->array_type
                || mmay_bytecode_register_layout(
                    function, (uint32_t)target
                ) != construction->array_layout) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < construction->argument_count;
                argument_index += 1) {
                const MarimayBytecodeArrayArgument *argument =
                    mmay_bytecode_array_argument(
                        function, construction->argument_start + argument_index
                    );
                status = argument != NULL
                    ? mmay_bytecode_require_assigned(
                        assigned, function->register_types.length,
                        argument->register_id
                    )
                    : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
                if (status != MMAY_BYTECODE_VERIFY_OK) {
                    return status;
                }
                if (argument->type != mmay_bytecode_function_array_element_type(
                        function, construction->array_type,
                        construction->array_layout, NULL
                    ) || (construction->array_type == MMAY_BYTECODE_TYPE_ARRAY
                        && argument->layout != mmay_bytecode_array_type(
                            function, construction->array_layout
                        )->element_layout)) {
                    return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
                }
            }
            break;
        }
        case MMAY_OPCODE_ARRAY_LENGTH:
        case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
        case MMAY_OPCODE_ARRAY_GET:
        case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS: {
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            if (instruction->opcode == MMAY_OPCODE_ARRAY_LENGTH) {
                status = first_type != NULL
                        && mmay_bytecode_type_is_array(*first_type)
                        && *target_type == MMAY_BYTECODE_TYPE_I64
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
                break;
            }
            second = instruction->operands[2].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, second
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            second_type = mmay_bytecode_register_type(function, second);
            {
                uint32_t element_layout = MMAY_BYTECODE_NO_LAYOUT;
                MarimayBytecodeType element_type = first_type != NULL
                    ? mmay_bytecode_function_array_element_type(
                        function, *first_type,
                        mmay_bytecode_register_layout(function, (uint32_t)first),
                        &element_layout
                    ) : MMAY_BYTECODE_TYPE_INVALID;
                status = first_type != NULL && second_type != NULL
                    && mmay_bytecode_type_is_array(*first_type)
                    && *second_type == MMAY_BYTECODE_TYPE_I64
                    && (instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX
                        ? *target_type == *first_type
                            && mmay_bytecode_register_layout(
                                function, (uint32_t)target
                            ) == mmay_bytecode_register_layout(
                                function, (uint32_t)first
                            )
                        : *target_type == element_type
                            && mmay_bytecode_register_layout(
                                function, (uint32_t)target
                            ) == element_layout)
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        }
        case MMAY_OPCODE_COPY:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            status = first_type != NULL && *target_type == *first_type
                    && mmay_bytecode_register_layout(
                        function,
                        (uint32_t)target
                    ) == mmay_bytecode_register_layout(
                        function,
                        (uint32_t)first
                    )
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_I64_NEGATE_CHECKED:
        case MMAY_OPCODE_I64_BITWISE_NOT:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == *first_type
                    && mmay_bytecode_type_is_integer(*target_type)
                    && (instruction->opcode == MMAY_OPCODE_I64_BITWISE_NOT
                        || mmay_bytecode_type_is_signed_integer(*target_type))
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_F64_NEGATE:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == MMAY_BYTECODE_TYPE_F64
                    && *first_type == MMAY_BYTECODE_TYPE_F64
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_FORMAT_F32:
        case MMAY_OPCODE_FORMAT_F64:
        case MMAY_OPCODE_FORMAT_BOOL:
        case MMAY_OPCODE_FORMAT_INTEGER:
        case MMAY_OPCODE_STRING_CONCAT:
            status = mmay_bytecode_verify_format_instruction(
                function, instruction, assigned
            );
            break;
        case MMAY_OPCODE_PARSE_INTEGER:
            status = mmay_bytecode_verify_parse_instruction(
                function, instruction, assigned
            );
            break;
        case MMAY_OPCODE_IO_PRINTLN:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            status = mmay_bytecode_verify_io_instruction(
                function, instruction
            );
            break;
        case MMAY_OPCODE_IO_WRITE:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) return status;
            status = mmay_bytecode_verify_io_instruction(function, instruction);
            break;
        case MMAY_OPCODE_IO_FLUSH:
            status = mmay_bytecode_verify_io_instruction(function, instruction);
            break;
        case MMAY_OPCODE_IO_READ_LINE:
        case MMAY_OPCODE_IO_READ_BYTES:
        case MMAY_OPCODE_IO_WRITE_BYTES:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) return status;
            status = mmay_bytecode_verify_io_instruction(function, instruction);
            break;
        case MMAY_OPCODE_TEST_OUTPUT_BEGIN:
        case MMAY_OPCODE_TEST_OUTPUT_END:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == (instruction->opcode
                            == MMAY_OPCODE_TEST_OUTPUT_BEGIN
                        ? MMAY_BYTECODE_TYPE_BOOL : MMAY_BYTECODE_TYPE_VOID)
                    && *first_type == (instruction->opcode
                            == MMAY_OPCODE_TEST_OUTPUT_BEGIN
                        ? MMAY_BYTECODE_TYPE_STRING : MMAY_BYTECODE_TYPE_BOOL)
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_BOOL_NOT:
            first = instruction->operands[1].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == MMAY_BYTECODE_TYPE_BOOL
                    && *first_type == MMAY_BYTECODE_TYPE_BOOL
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_I64_ADD_CHECKED: case MMAY_OPCODE_I64_ADD_PROVEN:
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED: case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED: case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
        case MMAY_OPCODE_I64_DIVIDE_CHECKED:
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
        case MMAY_OPCODE_I64_BITWISE_AND:
        case MMAY_OPCODE_I64_BITWISE_XOR:
        case MMAY_OPCODE_I64_BITWISE_OR:
        case MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case MMAY_OPCODE_I64_SHIFT_RIGHT:
        case MMAY_OPCODE_I64_EQUAL:
        case MMAY_OPCODE_I64_LESS:
        case MMAY_OPCODE_I64_LESS_EQUAL:
        case MMAY_OPCODE_BOOL_EQUAL:
        case MMAY_OPCODE_STRING_EQUAL:
        case MMAY_OPCODE_F64_ADD:
        case MMAY_OPCODE_F64_SUBTRACT:
        case MMAY_OPCODE_F64_MULTIPLY:
        case MMAY_OPCODE_F64_DIVIDE:
        case MMAY_OPCODE_F64_EQUAL:
        case MMAY_OPCODE_F64_LESS:
        case MMAY_OPCODE_F64_LESS_EQUAL:
            first = instruction->operands[1].unsigned_value;
            second = instruction->operands[2].unsigned_value;
            status = mmay_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status == MMAY_BYTECODE_VERIFY_OK) {
                status = mmay_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    second
                );
            }
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = mmay_bytecode_register_type(function, first);
            second_type = mmay_bytecode_register_type(function, second);
            if (instruction->opcode == MMAY_OPCODE_I64_ADD_PROVEN
                || instruction->opcode == MMAY_OPCODE_I64_SUBTRACT_PROVEN
                || instruction->opcode == MMAY_OPCODE_I64_MULTIPLY_PROVEN) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == MMAY_BYTECODE_TYPE_I64
                        && *first_type == MMAY_BYTECODE_TYPE_I64
                        && *second_type == MMAY_BYTECODE_TYPE_I64
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == MMAY_OPCODE_BOOL_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == MMAY_BYTECODE_TYPE_BOOL
                        && *first_type == MMAY_BYTECODE_TYPE_BOOL
                        && *second_type == MMAY_BYTECODE_TYPE_BOOL
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == MMAY_OPCODE_STRING_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == MMAY_BYTECODE_TYPE_BOOL
                        && *first_type == MMAY_BYTECODE_TYPE_STRING
                        && *second_type == MMAY_BYTECODE_TYPE_STRING
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == MMAY_OPCODE_I64_EQUAL
                || instruction->opcode == MMAY_OPCODE_I64_LESS
                || instruction->opcode == MMAY_OPCODE_I64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == MMAY_BYTECODE_TYPE_BOOL
                        && mmay_bytecode_type_is_integer(*first_type)
                        && *first_type == *second_type
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == MMAY_OPCODE_F64_EQUAL
                || instruction->opcode == MMAY_OPCODE_F64_LESS
                || instruction->opcode == MMAY_OPCODE_F64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == MMAY_BYTECODE_TYPE_BOOL
                        && *first_type == MMAY_BYTECODE_TYPE_F64
                        && *second_type == MMAY_BYTECODE_TYPE_F64
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == MMAY_OPCODE_F64_ADD
                || instruction->opcode == MMAY_OPCODE_F64_SUBTRACT
                || instruction->opcode == MMAY_OPCODE_F64_MULTIPLY
                || instruction->opcode == MMAY_OPCODE_F64_DIVIDE) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == MMAY_BYTECODE_TYPE_F64
                        && *first_type == MMAY_BYTECODE_TYPE_F64
                        && *second_type == MMAY_BYTECODE_TYPE_F64
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else {
                status = first_type != NULL && second_type != NULL
                        && *target_type == *first_type
                        && *first_type == *second_type
                        && mmay_bytecode_type_is_integer(*target_type)
                    ? MMAY_BYTECODE_VERIFY_OK
                    : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        case MMAY_OPCODE_INVALID:
        case MMAY_OPCODE_JUMP:
        case MMAY_OPCODE_BRANCH_BOOL:
        case MMAY_OPCODE_RETURN:
        case MMAY_OPCODE_RETURN_VOID:
        case MMAY_OPCODE_CALL_DIRECT:
        case MMAY_OPCODE_CALL_DIRECT_VOID:
        case MMAY_OPCODE_PANIC_STRING:
        case MMAY_OPCODE_CALL_DIRECT_NEVER:
        case MMAY_OPCODE_ARRAY_SET:
        case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_PUSH:
        case MMAY_OPCODE_ARRAY_POP:
        case MMAY_OPCODE_ARRAY_TRUNCATE:
        case MMAY_OPCODE_ARRAY_CLEAR:
        case MMAY_OPCODE_ARRAY_RESERVE:
        case MMAY_OPCODE_ARRAY_GENERATION:
        case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
        case MMAY_OPCODE_RECORD_NEW:
        case MMAY_OPCODE_RECORD_GET:
        case MMAY_OPCODE_RECORD_SET:
        case MMAY_OPCODE_RECORD_IDENTITY_SAME:
        case MMAY_OPCODE_ENUM_NEW:
        case MMAY_OPCODE_ENUM_IS_VARIANT:
        case MMAY_OPCODE_ENUM_REQUIRE_VARIANT:
        case MMAY_OPCODE_ENUM_GET_PAYLOAD:
        case MMAY_OPCODE_INTERFACE_NEW:
        case MMAY_OPCODE_CALL_INTERFACE:
        case MMAY_OPCODE_INTEGER_CHECKED_ADD:
        case MMAY_OPCODE_INTEGER_CHECKED_SUBTRACT:
        case MMAY_OPCODE_INTEGER_CHECKED_MULTIPLY:
        case MMAY_OPCODE_INTEGER_CHECKED_DIVIDE:
        case MMAY_OPCODE_INTEGER_CHECKED_REMAINDER:
        case MMAY_OPCODE_INTEGER_CHECKED_NEGATE:
        case MMAY_OPCODE_INTEGER_CHECKED_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_WRAPPING_ADD:
        case MMAY_OPCODE_INTEGER_WRAPPING_SUBTRACT:
        case MMAY_OPCODE_INTEGER_WRAPPING_MULTIPLY:
        case MMAY_OPCODE_INTEGER_WRAPPING_DIVIDE:
        case MMAY_OPCODE_INTEGER_WRAPPING_REMAINDER:
        case MMAY_OPCODE_INTEGER_WRAPPING_NEGATE:
        case MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_SATURATING_ADD:
        case MMAY_OPCODE_INTEGER_SATURATING_SUBTRACT:
        case MMAY_OPCODE_INTEGER_SATURATING_MULTIPLY:
        case MMAY_OPCODE_INTEGER_SATURATING_DIVIDE:
        case MMAY_OPCODE_INTEGER_SATURATING_REMAINDER:
        case MMAY_OPCODE_INTEGER_SATURATING_NEGATE:
        case MMAY_OPCODE_INTEGER_SATURATING_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_CONVERT_TOTAL:
        case MMAY_OPCODE_INTEGER_CONVERT_OPTION:
        case MMAY_OPCODE_DEFER_REGISTER:
        case MMAY_OPCODE_DEFER_CALL:
            return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
        default:
            return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
    }
    if (status == MMAY_BYTECODE_VERIFY_OK) {
        assigned[(size_t)target] = 1;
    }
    return status;
}
