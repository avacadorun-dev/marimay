#include "ruda/bytecode/bytecode.h"

#include <limits.h>
#include <string.h>

#include "bytecode_internal.h"

static const RudaBytecodeType *ruda_bytecode_register_type(
    const RudaBytecodeFunction *function,
    uint64_t register_id
) {
    if (function == NULL || register_id >= function->register_types.length) {
        return NULL;
    }
    return ruda_vector_at_const(
        &function->register_types,
        (size_t)register_id
    );
}

const RudaBytecodeInstruction *ruda_bytecode_block_terminator(
    const RudaBytecodeBlock *block
) {
    if (block == NULL || block->instructions.length == 0) {
        return NULL;
    }
    return ruda_vector_at_const(
        &block->instructions,
        block->instructions.length - 1
    );
}

bool ruda_bytecode_opcode_writes_register(RudaOpcode opcode) {
    switch (opcode) {
        case RUDA_OPCODE_CONST_I64:
        case RUDA_OPCODE_CONST_F64:
        case RUDA_OPCODE_CONST_BOOL:
        case RUDA_OPCODE_COPY:
        case RUDA_OPCODE_I64_ADD_CHECKED:
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
        case RUDA_OPCODE_I64_DIVIDE_CHECKED:
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
        case RUDA_OPCODE_I64_NEGATE_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_NOT:
        case RUDA_OPCODE_F64_NEGATE:
        case RUDA_OPCODE_BOOL_NOT:
        case RUDA_OPCODE_I64_EQUAL:
        case RUDA_OPCODE_BOOL_EQUAL:
        case RUDA_OPCODE_I64_LESS:
        case RUDA_OPCODE_I64_LESS_EQUAL:
        case RUDA_OPCODE_I64_BITWISE_AND:
        case RUDA_OPCODE_I64_BITWISE_XOR:
        case RUDA_OPCODE_I64_BITWISE_OR:
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case RUDA_OPCODE_I64_SHIFT_RIGHT:
        case RUDA_OPCODE_F64_ADD:
        case RUDA_OPCODE_F64_SUBTRACT:
        case RUDA_OPCODE_F64_MULTIPLY:
        case RUDA_OPCODE_F64_DIVIDE:
        case RUDA_OPCODE_F64_EQUAL:
        case RUDA_OPCODE_F64_LESS:
        case RUDA_OPCODE_F64_LESS_EQUAL:
        case RUDA_OPCODE_CONST_STRING:
        case RUDA_OPCODE_STRING_EQUAL:
        case RUDA_OPCODE_CALL_DIRECT:
        case RUDA_OPCODE_ARRAY_NEW:
        case RUDA_OPCODE_ARRAY_LENGTH:
        case RUDA_OPCODE_ARRAY_REQUIRE_INDEX:
        case RUDA_OPCODE_ARRAY_GET:
        case RUDA_OPCODE_RECORD_NEW:
        case RUDA_OPCODE_RECORD_GET:
        case RUDA_OPCODE_RECORD_IDENTITY_SAME:
        case RUDA_OPCODE_ENUM_NEW:
        case RUDA_OPCODE_ENUM_IS_VARIANT:
        case RUDA_OPCODE_ENUM_REQUIRE_VARIANT:
        case RUDA_OPCODE_ENUM_GET_PAYLOAD:
        case RUDA_OPCODE_INTERFACE_NEW:
        case RUDA_OPCODE_CALL_INTERFACE:
            return true;
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
        case RUDA_OPCODE_CALL_DIRECT_VOID:
        case RUDA_OPCODE_PANIC_STRING:
        case RUDA_OPCODE_CALL_DIRECT_NEVER:
        case RUDA_OPCODE_ARRAY_SET:
        case RUDA_OPCODE_RECORD_SET:
            return false;
    }
    return false;
}

void ruda_bytecode_transfer_assignments(
    const RudaBytecodeBlock *block,
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
        const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
            &block->instructions,
            instruction_index
        );
        if (instruction != NULL
            && ruda_bytecode_opcode_writes_register(instruction->opcode)
            && instruction->operands[0].unsigned_value < register_count) {
            assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
        }
    }
}

static RudaBytecodeVerifyStatus ruda_bytecode_require_assigned(
    const unsigned char *assigned,
    size_t register_count,
    uint64_t register_id
) {
    if (register_id >= register_count) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return assigned[(size_t)register_id] != 0
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}

static bool ruda_bytecode_register_matches(
    const RudaBytecodeFunction *function,
    uint64_t register_id,
    RudaBytecodeType type,
    uint32_t layout
) {
    const RudaBytecodeType *actual = ruda_bytecode_register_type(
        function,
        register_id
    );
    return actual != NULL && *actual == type
        && ruda_bytecode_type_layout_is_valid(function, type, layout)
        && ruda_bytecode_register_layout(
            function,
            (uint32_t)register_id
        ) == layout;
}

static const RudaBytecodeEnumVariant *ruda_bytecode_enum_variant(
    const RudaBytecodeFunction *function,
    const RudaBytecodeEnumLayout *layout,
    size_t variant
) {
    return function != NULL && layout != NULL && variant < layout->variant_count
        ? ruda_vector_at_const(
            &function->enum_variants,
            layout->variant_start + variant
        )
        : NULL;
}

static const RudaBytecodeEnumPayload *ruda_bytecode_enum_payload(
    const RudaBytecodeFunction *function,
    const RudaBytecodeEnumLayout *layout,
    const RudaBytecodeEnumVariant *variant,
    size_t field
) {
    return function != NULL && layout != NULL && variant != NULL
            && field < variant->payload_count
            && variant->payload_start <= layout->payload_count
            && field <= layout->payload_count - variant->payload_start
        ? ruda_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + variant->payload_start + field
        )
        : NULL;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_record_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t record_register = instruction->operands[1].unsigned_value;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (instruction->opcode == RUDA_OPCODE_RECORD_NEW) {
        uint64_t construction_wire = instruction->operands[1].unsigned_value;
        const RudaBytecodeRecordConstruction *construction =
            construction_wire < function->record_constructions.length
            ? ruda_bytecode_record_construction(
                function,
                (RudaBytecodeRecordConstructionId)construction_wire
            )
            : NULL;
        const RudaBytecodeRecordLayout *layout = construction != NULL
            ? ruda_bytecode_record_layout(function, construction->record)
            : NULL;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL
            || !ruda_bytecode_register_matches(
                function,
                target,
                RUDA_BYTECODE_TYPE_RECORD,
                construction->record
            )) {
            return construction == NULL || layout == NULL
                ? RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaBytecodeRecordArgument *argument = ruda_vector_at_const(
                &function->record_arguments,
                construction->argument_start + argument_index
            );
            status = argument != NULL
                ? ruda_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    argument->register_id
                )
                : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            if (!ruda_bytecode_register_matches(
                    function,
                    argument->register_id,
                    argument->type,
                    argument->layout
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        }
        assigned[(size_t)target] = 1;
        return RUDA_BYTECODE_VERIFY_OK;
    }
    status = ruda_bytecode_require_assigned(
        assigned,
        function->register_types.length,
        record_register
    );
    if (status != RUDA_BYTECODE_VERIFY_OK) {
        return status;
    }
    {
        uint32_t record_id = ruda_bytecode_register_layout(
            function,
            (uint32_t)record_register
        );
        const RudaBytecodeRecordLayout *layout = ruda_bytecode_record_layout(
            function,
            record_id
        );
        int64_t field_wire = instruction->operands[2].signed_value;
        const RudaBytecodeRecordField *field =
            instruction->opcode != RUDA_OPCODE_RECORD_IDENTITY_SAME
                && field_wire >= 0
            ? ruda_bytecode_record_field(
                function,
                layout,
                (size_t)field_wire
            )
            : NULL;
        if (layout == NULL || !ruda_bytecode_register_matches(
                function,
                record_register,
                RUDA_BYTECODE_TYPE_RECORD,
                record_id
            )) {
            return layout == NULL
                ? RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == RUDA_OPCODE_RECORD_IDENTITY_SAME) {
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                instruction->operands[2].unsigned_value
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            if (!ruda_bytecode_register_matches(
                    function,
                    instruction->operands[2].unsigned_value,
                    RUDA_BYTECODE_TYPE_RECORD,
                    record_id
                ) || !ruda_bytecode_register_matches(
                    function,
                    target,
                    RUDA_BYTECODE_TYPE_BOOL,
                    RUDA_BYTECODE_NO_LAYOUT
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            assigned[(size_t)target] = 1;
            return RUDA_BYTECODE_VERIFY_OK;
        } else if (field == NULL) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        } else if (instruction->opcode == RUDA_OPCODE_RECORD_SET) {
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                target
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            return field->is_mutable && ruda_bytecode_register_matches(
                    function,
                    target,
                    field->type,
                    field->layout
                )
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == RUDA_OPCODE_RECORD_GET) {
            if (!ruda_bytecode_register_matches(
                    function,
                    target,
                    field->type,
                    field->layout
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else {
            return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
        }
    }
    assigned[(size_t)target] = 1;
    return RUDA_BYTECODE_VERIFY_OK;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_enum_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    if (instruction->opcode == RUDA_OPCODE_ENUM_NEW) {
        uint64_t construction_wire = instruction->operands[1].unsigned_value;
        const RudaBytecodeEnumConstruction *construction =
            construction_wire < function->enum_constructions.length
            ? ruda_bytecode_enum_construction(
                function,
                (RudaBytecodeEnumConstructionId)construction_wire
            )
            : NULL;
        const RudaBytecodeEnumLayout *layout = construction != NULL
            ? ruda_bytecode_enum_layout(function, construction->enum_id)
            : NULL;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL
            || !ruda_bytecode_register_matches(
                function,
                target,
                RUDA_BYTECODE_TYPE_ENUM,
                construction->enum_id
            )) {
            return construction == NULL || layout == NULL
                ? RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaBytecodeEnumArgument *argument = ruda_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + argument_index
            );
            RudaBytecodeVerifyStatus status = argument != NULL
                ? ruda_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    argument->register_id
                )
                : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            if (!ruda_bytecode_register_matches(
                    function,
                    argument->register_id,
                    argument->type,
                    argument->layout
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        }
        assigned[(size_t)target] = 1;
        return RUDA_BYTECODE_VERIFY_OK;
    }
    {
        uint64_t enum_register = instruction->operands[1].unsigned_value;
        RudaBytecodeVerifyStatus status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            enum_register
        );
        uint32_t enum_id = ruda_bytecode_register_layout(
            function,
            (uint32_t)enum_register
        );
        const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
            function,
            enum_id
        );
        uint64_t selector = (uint64_t)instruction->operands[2].signed_value;
        size_t variant_index = instruction->opcode == RUDA_OPCODE_ENUM_GET_PAYLOAD
            ? (size_t)(selector >> 32)
            : (size_t)instruction->operands[2].signed_value;
        const RudaBytecodeEnumVariant *variant =
            instruction->operands[2].signed_value >= 0
                || instruction->opcode == RUDA_OPCODE_ENUM_GET_PAYLOAD
            ? ruda_bytecode_enum_variant(function, layout, variant_index)
            : NULL;
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (layout == NULL || variant == NULL) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_bytecode_register_matches(
                function,
                enum_register,
                RUDA_BYTECODE_TYPE_ENUM,
                enum_id
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (instruction->opcode == RUDA_OPCODE_ENUM_IS_VARIANT) {
            if (!ruda_bytecode_register_matches(
                    function,
                    target,
                    RUDA_BYTECODE_TYPE_BOOL,
                    RUDA_BYTECODE_NO_LAYOUT
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else if (instruction->opcode == RUDA_OPCODE_ENUM_REQUIRE_VARIANT) {
            if (!ruda_bytecode_register_matches(
                    function,
                    target,
                    RUDA_BYTECODE_TYPE_ENUM,
                    enum_id
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else if (instruction->opcode == RUDA_OPCODE_ENUM_GET_PAYLOAD) {
            const RudaBytecodeEnumPayload *payload =
                ruda_bytecode_enum_payload(
                    function,
                    layout,
                    variant,
                    (size_t)(selector & UINT32_MAX)
                );
            if (payload == NULL) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            if (!ruda_bytecode_register_matches(
                    function,
                    target,
                    payload->type,
                    payload->layout
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else {
            return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
        }
    }
    assigned[(size_t)target] = 1;
    return RUDA_BYTECODE_VERIFY_OK;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_call_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    bool returns_void = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID;
    bool returns_never = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER;
    uint64_t call_wire = returns_void || returns_never
        ? instruction->operands[0].unsigned_value
        : instruction->operands[1].unsigned_value;
    const RudaBytecodeCall *call = call_wire < function->calls.length
        ? ruda_vector_at_const(&function->calls, (size_t)call_wire)
        : NULL;
    size_t argument_index = 0;
    if (call == NULL || call->interface_slot != UINT32_MAX) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (returns_void != (call->result_type == RUDA_BYTECODE_TYPE_VOID)
        || returns_never != (call->result_type == RUDA_BYTECODE_TYPE_NEVER)) {
        return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (!returns_void && !returns_never) {
        uint64_t target = instruction->operands[0].unsigned_value;
        const RudaBytecodeType *target_type = ruda_bytecode_register_type(
            function,
            target
        );
        if (target_type == NULL) {
            return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
        }
        if (!ruda_bytecode_type_is_register(*target_type)
            || !ruda_bytecode_register_matches(
                function,
                target,
                call->result_type,
                call->result_layout
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const RudaBytecodeCallArgument *argument = ruda_vector_at_const(
            &function->call_arguments,
            call->argument_start + argument_index
        );
        RudaBytecodeVerifyStatus status = argument != NULL
            ? ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                argument->register_id
            )
            : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (!ruda_bytecode_register_matches(
                function,
                argument->register_id,
                argument->type,
                argument->layout
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    if (!returns_void && !returns_never) {
        assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
    }
    return RUDA_BYTECODE_VERIFY_OK;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_interface_new(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t construction_wire = instruction->operands[1].unsigned_value;
    const RudaBytecodeInterfaceConstruction *construction =
        construction_wire < function->interface_constructions.length
        ? ruda_bytecode_interface_construction(
            function,
            (RudaBytecodeInterfaceConstructionId)construction_wire
        )
        : NULL;
    const RudaBytecodeInterfaceLayout *layout = construction != NULL
        ? ruda_bytecode_interface_layout(function, construction->interface_id)
        : NULL;
    RudaBytecodeVerifyStatus status;
    if (construction == NULL || layout == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if ((construction->payload_type != RUDA_BYTECODE_TYPE_RECORD
            && construction->payload_type != RUDA_BYTECODE_TYPE_ENUM)
        || !ruda_bytecode_register_matches(
            function,
            target,
            RUDA_BYTECODE_TYPE_INTERFACE,
            construction->interface_id
        ) || !ruda_bytecode_register_matches(
            function,
            construction->payload_register,
            construction->payload_type,
            construction->payload_layout
        )) {
        return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    status = ruda_bytecode_require_assigned(
        assigned,
        function->register_types.length,
        construction->payload_register
    );
    if (status != RUDA_BYTECODE_VERIFY_OK) {
        return status;
    }
    assigned[(size_t)target] = 1;
    return RUDA_BYTECODE_VERIFY_OK;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_interface_call(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t call_wire = instruction->operands[1].unsigned_value;
    const RudaBytecodeCall *call = call_wire < function->calls.length
        ? ruda_bytecode_call(function, (RudaBytecodeCallId)call_wire)
        : NULL;
    const RudaBytecodeCallArgument *receiver = call != NULL
            && call->argument_count != 0
        ? ruda_bytecode_call_argument(function, call->argument_start)
        : NULL;
    const RudaBytecodeInterfaceLayout *layout = receiver != NULL
            && receiver->type == RUDA_BYTECODE_TYPE_INTERFACE
        ? ruda_bytecode_interface_layout(function, receiver->layout)
        : NULL;
    size_t argument_index = 0;
    if (call == NULL || receiver == NULL || layout == NULL
        || call->interface_slot == UINT32_MAX
        || call->interface_slot >= layout->method_count
        || !ruda_bytecode_type_is_register(call->result_type)) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (!ruda_bytecode_register_matches(
            function,
            target,
            call->result_type,
            call->result_layout
        )) {
        return ruda_bytecode_register_type(function, target) == NULL
            ? RUDA_BYTECODE_VERIFY_INVALID_REGISTER
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const RudaBytecodeCallArgument *argument = ruda_bytecode_call_argument(
            function,
            call->argument_start + argument_index
        );
        RudaBytecodeVerifyStatus status = argument != NULL
            ? ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                argument->register_id
            )
            : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        if (!ruda_bytecode_register_matches(
                function,
                argument->register_id,
                argument->type,
                argument->layout
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    assigned[(size_t)target] = 1;
    return RUDA_BYTECODE_VERIFY_OK;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_value_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = 0;
    uint64_t second = 0;
    const RudaBytecodeType *target_type = NULL;
    const RudaBytecodeType *first_type = NULL;
    const RudaBytecodeType *second_type = NULL;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (instruction->opcode == RUDA_OPCODE_CALL_DIRECT
        || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID
        || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER) {
        return ruda_bytecode_verify_call_instruction(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == RUDA_OPCODE_INTERFACE_NEW) {
        return ruda_bytecode_verify_interface_new(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == RUDA_OPCODE_CALL_INTERFACE) {
        return ruda_bytecode_verify_interface_call(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == RUDA_OPCODE_ARRAY_SET) {
        uint64_t array = instruction->operands[0].unsigned_value;
        uint64_t index = instruction->operands[1].unsigned_value;
        uint64_t value = instruction->operands[2].unsigned_value;
        const RudaBytecodeType *array_type;
        const RudaBytecodeType *index_type;
        const RudaBytecodeType *value_type;
        status = ruda_bytecode_require_assigned(
            assigned, function->register_types.length, array
        );
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, index
            );
        }
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, value
            );
        }
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        array_type = ruda_bytecode_register_type(function, array);
        index_type = ruda_bytecode_register_type(function, index);
        value_type = ruda_bytecode_register_type(function, value);
        return array_type != NULL && index_type != NULL && value_type != NULL
                && ruda_bytecode_type_is_array(*array_type)
                && *index_type == RUDA_BYTECODE_TYPE_I64
                && *value_type == ruda_bytecode_array_element_type(*array_type)
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RECORD_NEW
        || instruction->opcode == RUDA_OPCODE_RECORD_GET
        || instruction->opcode == RUDA_OPCODE_RECORD_SET
        || instruction->opcode == RUDA_OPCODE_RECORD_IDENTITY_SAME) {
        return ruda_bytecode_verify_record_instruction(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == RUDA_OPCODE_ENUM_NEW
        || instruction->opcode == RUDA_OPCODE_ENUM_IS_VARIANT
        || instruction->opcode == RUDA_OPCODE_ENUM_REQUIRE_VARIANT
        || instruction->opcode == RUDA_OPCODE_ENUM_GET_PAYLOAD) {
        return ruda_bytecode_verify_enum_instruction(
            function,
            instruction,
            assigned
        );
    }
    target_type = ruda_bytecode_register_type(function, target);
    if (target_type == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->opcode) {
        case RUDA_OPCODE_CONST_I64:
            status = *target_type == RUDA_BYTECODE_TYPE_I64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_F64:
            status = *target_type == RUDA_BYTECODE_TYPE_F64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_BOOL:
            if (instruction->operands[1].unsigned_value > 1) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == RUDA_BYTECODE_TYPE_BOOL
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_STRING:
            if (instruction->operands[1].unsigned_value
                    >= function->string_constants.length) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == RUDA_BYTECODE_TYPE_STRING
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_ARRAY_NEW: {
            uint64_t construction_wire = instruction->operands[1].unsigned_value;
            const RudaBytecodeArrayConstruction *construction =
                construction_wire < function->array_constructions.length
                ? ruda_bytecode_array_construction(
                    function,
                    (RudaBytecodeArrayConstructionId)construction_wire
                )
                : NULL;
            size_t argument_index = 0;
            if (construction == NULL
                || *target_type != construction->array_type) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < construction->argument_count;
                argument_index += 1) {
                const RudaBytecodeArrayArgument *argument =
                    ruda_bytecode_array_argument(
                        function, construction->argument_start + argument_index
                    );
                status = argument != NULL
                    ? ruda_bytecode_require_assigned(
                        assigned, function->register_types.length,
                        argument->register_id
                    )
                    : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
                if (status != RUDA_BYTECODE_VERIFY_OK) {
                    return status;
                }
            }
            break;
        }
        case RUDA_OPCODE_ARRAY_LENGTH:
        case RUDA_OPCODE_ARRAY_REQUIRE_INDEX:
        case RUDA_OPCODE_ARRAY_GET: {
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            if (instruction->opcode == RUDA_OPCODE_ARRAY_LENGTH) {
                status = first_type != NULL
                        && ruda_bytecode_type_is_array(*first_type)
                        && *target_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
                break;
            }
            second = instruction->operands[2].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, second
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            second_type = ruda_bytecode_register_type(function, second);
            status = first_type != NULL && second_type != NULL
                    && ruda_bytecode_type_is_array(*first_type)
                    && *second_type == RUDA_BYTECODE_TYPE_I64
                    && (instruction->opcode == RUDA_OPCODE_ARRAY_REQUIRE_INDEX
                        ? *target_type == *first_type
                        : *target_type
                            == ruda_bytecode_array_element_type(*first_type))
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        }
        case RUDA_OPCODE_COPY:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL && *target_type == *first_type
                    && ruda_bytecode_register_layout(
                        function,
                        (uint32_t)target
                    ) == ruda_bytecode_register_layout(
                        function,
                        (uint32_t)first
                    )
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_I64_NEGATE_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_NOT:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_I64
                    && *first_type == RUDA_BYTECODE_TYPE_I64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_F64_NEGATE:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_F64
                    && *first_type == RUDA_BYTECODE_TYPE_F64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_BOOL_NOT:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_BOOL
                    && *first_type == RUDA_BYTECODE_TYPE_BOOL
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_I64_ADD_CHECKED:
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
        case RUDA_OPCODE_I64_DIVIDE_CHECKED:
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_AND:
        case RUDA_OPCODE_I64_BITWISE_XOR:
        case RUDA_OPCODE_I64_BITWISE_OR:
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case RUDA_OPCODE_I64_SHIFT_RIGHT:
        case RUDA_OPCODE_I64_EQUAL:
        case RUDA_OPCODE_I64_LESS:
        case RUDA_OPCODE_I64_LESS_EQUAL:
        case RUDA_OPCODE_BOOL_EQUAL:
        case RUDA_OPCODE_STRING_EQUAL:
        case RUDA_OPCODE_F64_ADD:
        case RUDA_OPCODE_F64_SUBTRACT:
        case RUDA_OPCODE_F64_MULTIPLY:
        case RUDA_OPCODE_F64_DIVIDE:
        case RUDA_OPCODE_F64_EQUAL:
        case RUDA_OPCODE_F64_LESS:
        case RUDA_OPCODE_F64_LESS_EQUAL:
            first = instruction->operands[1].unsigned_value;
            second = instruction->operands[2].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status == RUDA_BYTECODE_VERIFY_OK) {
                status = ruda_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    second
                );
            }
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            second_type = ruda_bytecode_register_type(function, second);
            if (instruction->opcode == RUDA_OPCODE_BOOL_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_BOOL
                        && *second_type == RUDA_BYTECODE_TYPE_BOOL
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_STRING_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_STRING
                        && *second_type == RUDA_BYTECODE_TYPE_STRING
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_I64_EQUAL
                || instruction->opcode == RUDA_OPCODE_I64_LESS
                || instruction->opcode == RUDA_OPCODE_I64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_I64
                        && *second_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_F64_EQUAL
                || instruction->opcode == RUDA_OPCODE_F64_LESS
                || instruction->opcode == RUDA_OPCODE_F64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_F64
                        && *second_type == RUDA_BYTECODE_TYPE_F64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_F64_ADD
                || instruction->opcode == RUDA_OPCODE_F64_SUBTRACT
                || instruction->opcode == RUDA_OPCODE_F64_MULTIPLY
                || instruction->opcode == RUDA_OPCODE_F64_DIVIDE) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_F64
                        && *first_type == RUDA_BYTECODE_TYPE_F64
                        && *second_type == RUDA_BYTECODE_TYPE_F64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_I64
                        && *first_type == RUDA_BYTECODE_TYPE_I64
                        && *second_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
        case RUDA_OPCODE_CALL_DIRECT:
        case RUDA_OPCODE_CALL_DIRECT_VOID:
        case RUDA_OPCODE_PANIC_STRING:
        case RUDA_OPCODE_CALL_DIRECT_NEVER:
        case RUDA_OPCODE_ARRAY_SET:
        case RUDA_OPCODE_RECORD_NEW:
        case RUDA_OPCODE_RECORD_GET:
        case RUDA_OPCODE_RECORD_SET:
        case RUDA_OPCODE_RECORD_IDENTITY_SAME:
        case RUDA_OPCODE_ENUM_NEW:
        case RUDA_OPCODE_ENUM_IS_VARIANT:
        case RUDA_OPCODE_ENUM_REQUIRE_VARIANT:
        case RUDA_OPCODE_ENUM_GET_PAYLOAD:
        case RUDA_OPCODE_INTERFACE_NEW:
        case RUDA_OPCODE_CALL_INTERFACE:
            return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
    }
    if (status == RUDA_BYTECODE_VERIFY_OK) {
        assigned[(size_t)target] = 1;
    }
    return status;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_terminator(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    const RudaBytecodeType *type = NULL;
    if (instruction->opcode == RUDA_OPCODE_JUMP) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == RUDA_OPCODE_BRANCH_BOOL) {
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        type = ruda_bytecode_register_type(
            function,
            instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == RUDA_BYTECODE_TYPE_BOOL
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RETURN) {
        if (function->result_type == RUDA_BYTECODE_TYPE_VOID
            || function->result_type == RUDA_BYTECODE_TYPE_NEVER) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        type = ruda_bytecode_register_type(
            function,
            instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == function->result_type
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RETURN_VOID) {
        return function->result_type == RUDA_BYTECODE_TYPE_VOID
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_PANIC_STRING) {
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        type = status == RUDA_BYTECODE_VERIFY_OK
            ? ruda_bytecode_register_type(
                function,
                instruction->operands[0].unsigned_value
            )
            : NULL;
        return status != RUDA_BYTECODE_VERIFY_OK ? status
            : type != NULL && *type == RUDA_BYTECODE_TYPE_STRING
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER) {
        return ruda_bytecode_verify_call_instruction(
            function,
            instruction,
            (unsigned char *)assigned
        );
    }
    return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
}
