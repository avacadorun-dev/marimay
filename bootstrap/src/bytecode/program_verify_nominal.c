#include "program_internal.h"

#include <string.h>

#include "ruda/base/map.h"
#include "bytecode_internal.h"

typedef struct RudaBytecodeNominalDefinition {
    const RudaBytecodeFunction *function;
    RudaBytecodeType type;
    uint32_t layout;
} RudaBytecodeNominalDefinition;

static bool ruda_bytecode_program_consume_work(
    size_t *work, size_t amount, size_t limit
) {
    size_t next = 0;
    if (work == NULL || !ruda_size_add(*work, amount, &next)
        || next > limit) {
        return false;
    }
    *work = next;
    return true;
}

bool ruda_bytecode_program_consume_nominal_work(
    const RudaBytecodeFunction *function,
    size_t *work,
    size_t limit
) {
    const size_t counts[] = {
        function != NULL ? function->record_layouts.length : 0,
        function != NULL ? function->record_fields.length : 0,
        function != NULL ? function->record_constructions.length : 0,
        function != NULL ? function->record_arguments.length : 0,
        function != NULL ? function->enum_layouts.length : 0,
        function != NULL ? function->enum_variants.length : 0,
        function != NULL ? function->enum_payloads.length : 0,
        function != NULL ? function->enum_constructions.length : 0,
        function != NULL ? function->enum_arguments.length : 0,
        function != NULL ? function->interface_layouts.length : 0,
        function != NULL ? function->interface_constructions.length : 0,
        function != NULL ? function->interface_methods.length : 0
    };
    size_t index = 0;
    if (function == NULL || work == NULL) {
        return false;
    }
    for (index = 0; index < sizeof(counts) / sizeof(counts[0]); index += 1) {
        if (!ruda_bytecode_program_consume_work(work, counts[index], limit)) {
            return false;
        }
    }
    return true;
}

bool ruda_bytecode_program_same_type(
    const RudaBytecodeFunction *left_function,
    RudaBytecodeType left_type,
    uint32_t left_layout,
    const RudaBytecodeFunction *right_function,
    RudaBytecodeType right_type,
    uint32_t right_layout
) {
    RudaSymbolId left_symbol;
    RudaSymbolId right_symbol;
    bool nominal = left_type == RUDA_BYTECODE_TYPE_RECORD
        || left_type == RUDA_BYTECODE_TYPE_ENUM
        || left_type == RUDA_BYTECODE_TYPE_INTERFACE;
    if (left_type != right_type) {
        return false;
    }
    if (!nominal) {
        return left_layout == RUDA_BYTECODE_NO_LAYOUT
            && right_layout == RUDA_BYTECODE_NO_LAYOUT;
    }
    memset(&left_symbol, 0, sizeof(left_symbol));
    memset(&right_symbol, 0, sizeof(right_symbol));
    if (left_type == RUDA_BYTECODE_TYPE_RECORD) {
        const RudaBytecodeRecordLayout *left = ruda_bytecode_record_layout(
            left_function,
            left_layout
        );
        const RudaBytecodeRecordLayout *right = ruda_bytecode_record_layout(
            right_function,
            right_layout
        );
        if (left == NULL || right == NULL) {
            return false;
        }
        left_symbol = left->symbol;
        right_symbol = right->symbol;
    } else if (left_type == RUDA_BYTECODE_TYPE_ENUM) {
        const RudaBytecodeEnumLayout *left = ruda_bytecode_enum_layout(
            left_function,
            left_layout
        );
        const RudaBytecodeEnumLayout *right = ruda_bytecode_enum_layout(
            right_function,
            right_layout
        );
        if (left == NULL || right == NULL) {
            return false;
        }
        left_symbol = left->symbol;
        right_symbol = right->symbol;
    } else {
        const RudaBytecodeInterfaceLayout *left =
            ruda_bytecode_interface_layout(left_function, left_layout);
        const RudaBytecodeInterfaceLayout *right =
            ruda_bytecode_interface_layout(right_function, right_layout);
        if (left == NULL || right == NULL) {
            return false;
        }
        left_symbol = left->symbol;
        right_symbol = right->symbol;
    }
    return ruda_symbol_id_equal(left_symbol, right_symbol);
}

static RudaBytecodeProgramVerifyStatus ruda_bytecode_program_compare_record(
    const RudaBytecodeNominalDefinition *left,
    const RudaBytecodeNominalDefinition *right,
    size_t *work,
    size_t limit
) {
    const RudaBytecodeRecordLayout *left_layout = ruda_bytecode_record_layout(
        left->function,
        left->layout
    );
    const RudaBytecodeRecordLayout *right_layout = ruda_bytecode_record_layout(
        right->function,
        right->layout
    );
    size_t field = 0;
    if (left_layout == NULL || right_layout == NULL
        || left_layout->field_count != right_layout->field_count) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    for (field = 0; field < left_layout->field_count; field += 1) {
        const RudaBytecodeRecordField *left_field = ruda_bytecode_record_field(
            left->function,
            left_layout,
            field
        );
        const RudaBytecodeRecordField *right_field = ruda_bytecode_record_field(
            right->function,
            right_layout,
            field
        );
        if (!ruda_bytecode_program_consume_work(work, 1, limit)) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (left_field == NULL || right_field == NULL
            || left_field->is_mutable != right_field->is_mutable
            || !ruda_bytecode_program_same_type(
                left->function,
                left_field->type,
                left_field->layout,
                right->function,
                right_field->type,
                right_field->layout
            )) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
        }
    }
    return RUDA_BYTECODE_PROGRAM_VERIFY_OK;
}

static RudaBytecodeProgramVerifyStatus ruda_bytecode_program_compare_enum(
    const RudaBytecodeNominalDefinition *left,
    const RudaBytecodeNominalDefinition *right,
    size_t *work,
    size_t limit
) {
    const RudaBytecodeEnumLayout *left_layout = ruda_bytecode_enum_layout(
        left->function,
        left->layout
    );
    const RudaBytecodeEnumLayout *right_layout = ruda_bytecode_enum_layout(
        right->function,
        right->layout
    );
    size_t variant = 0;
    size_t payload = 0;
    if (left_layout == NULL || right_layout == NULL
        || left_layout->variant_count != right_layout->variant_count
        || left_layout->payload_count != right_layout->payload_count) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    for (variant = 0; variant < left_layout->variant_count; variant += 1) {
        const RudaBytecodeEnumVariant *left_variant = ruda_vector_at_const(
            &left->function->enum_variants,
            left_layout->variant_start + variant
        );
        const RudaBytecodeEnumVariant *right_variant = ruda_vector_at_const(
            &right->function->enum_variants,
            right_layout->variant_start + variant
        );
        if (!ruda_bytecode_program_consume_work(work, 1, limit)) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (left_variant == NULL || right_variant == NULL
            || left_variant->payload_count != right_variant->payload_count) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
        }
    }
    for (payload = 0; payload < left_layout->payload_count; payload += 1) {
        const RudaBytecodeEnumPayload *left_payload = ruda_vector_at_const(
            &left->function->enum_payloads,
            left_layout->payload_start + payload
        );
        const RudaBytecodeEnumPayload *right_payload = ruda_vector_at_const(
            &right->function->enum_payloads,
            right_layout->payload_start + payload
        );
        if (!ruda_bytecode_program_consume_work(work, 1, limit)) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (left_payload == NULL || right_payload == NULL
            || !ruda_bytecode_program_same_type(
                left->function,
                left_payload->type,
                left_payload->layout,
                right->function,
                right_payload->type,
                right_payload->layout
            )) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
        }
    }
    return RUDA_BYTECODE_PROGRAM_VERIFY_OK;
}

static RudaBytecodeProgramVerifyStatus ruda_bytecode_program_compare_schema(
    const RudaBytecodeNominalDefinition *left,
    const RudaBytecodeNominalDefinition *right,
    size_t *work,
    size_t limit
) {
    if (left->type != right->type) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    if (left->type == RUDA_BYTECODE_TYPE_RECORD) {
        return ruda_bytecode_program_compare_record(left, right, work, limit);
    }
    if (left->type == RUDA_BYTECODE_TYPE_ENUM) {
        return ruda_bytecode_program_compare_enum(left, right, work, limit);
    }
    if (left->type == RUDA_BYTECODE_TYPE_INTERFACE) {
        const RudaBytecodeInterfaceLayout *left_layout =
            ruda_bytecode_interface_layout(left->function, left->layout);
        const RudaBytecodeInterfaceLayout *right_layout =
            ruda_bytecode_interface_layout(right->function, right->layout);
        return left_layout != NULL && right_layout != NULL
                && left_layout->method_count == right_layout->method_count
            ? RUDA_BYTECODE_PROGRAM_VERIFY_OK
            : RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    return RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
}

static RudaBytecodeProgramVerifyStatus ruda_bytecode_program_add_schema(
    RudaMap *definitions,
    RudaSymbolId symbol,
    RudaBytecodeNominalDefinition definition,
    size_t *work,
    size_t limit
) {
    const RudaBytecodeNominalDefinition *existing = ruda_map_get_const(
        definitions,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    if (!ruda_bytecode_program_consume_work(work, 1, limit)) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
    }
    if (existing != NULL) {
        return ruda_bytecode_program_compare_schema(
            existing,
            &definition,
            work,
            limit
        );
    }
    return ruda_map_put(
            definitions,
            symbol.bytes,
            sizeof(symbol.bytes),
            &definition
        )
        ? RUDA_BYTECODE_PROGRAM_VERIFY_OK
        : RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED;
}

static bool ruda_bytecode_program_nominal_symbol(
    const RudaBytecodeNominalDefinition *definition,
    RudaSymbolId *symbol
) {
    if (definition == NULL || definition->function == NULL || symbol == NULL) {
        return false;
    }
    if (definition->type == RUDA_BYTECODE_TYPE_RECORD) {
        const RudaBytecodeRecordLayout *layout = ruda_bytecode_record_layout(
            definition->function,
            definition->layout
        );
        if (layout == NULL) {
            return false;
        }
        *symbol = layout->symbol;
        return true;
    }
    if (definition->type == RUDA_BYTECODE_TYPE_ENUM) {
        const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
            definition->function,
            definition->layout
        );
        if (layout == NULL) {
            return false;
        }
        *symbol = layout->symbol;
        return true;
    }
    if (definition->type == RUDA_BYTECODE_TYPE_INTERFACE) {
        const RudaBytecodeInterfaceLayout *layout =
            ruda_bytecode_interface_layout(
                definition->function,
                definition->layout
            );
        if (layout == NULL) {
            return false;
        }
        *symbol = layout->symbol;
        return true;
    }
    return false;
}

RudaBytecodeProgramVerifyStatus ruda_bytecode_program_verify_nominal_schemas(
    const RudaBytecodeProgram *program,
    size_t *work
) {
    RudaMap definitions;
    RudaBytecodeProgramVerifyStatus status = RUDA_BYTECODE_PROGRAM_VERIFY_OK;
    size_t function_index = 0;
    memset(&definitions, 0, sizeof(definitions));
    if (program == NULL || work == NULL || !ruda_map_init(
            &definitions,
            program->allocator,
            sizeof(RudaBytecodeNominalDefinition),
            UINT64_C(0x243f6a8885a308d3),
            UINT64_C(0x13198a2e03707344)
        )) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED;
    }
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        const RudaBytecodeProgramFunction *owner =
            ruda_bytecode_program_function(program, function_index);
        const RudaBytecodeType types[] = {
            RUDA_BYTECODE_TYPE_RECORD,
            RUDA_BYTECODE_TYPE_ENUM,
            RUDA_BYTECODE_TYPE_INTERFACE
        };
        const size_t counts[] = {
            owner != NULL ? owner->body.record_layouts.length : 0,
            owner != NULL ? owner->body.enum_layouts.length : 0,
            owner != NULL ? owner->body.interface_layouts.length : 0
        };
        size_t type_index = 0;
        if (owner == NULL) {
            status = RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
            break;
        }
        for (type_index = 0; type_index < sizeof(types) / sizeof(types[0]);
            type_index += 1) {
            size_t layout = 0;
            for (layout = 0; layout < counts[type_index]; layout += 1) {
                RudaBytecodeNominalDefinition definition;
                RudaSymbolId symbol;
                definition.function = &owner->body;
                definition.type = types[type_index];
                definition.layout = (uint32_t)layout;
                if (!ruda_bytecode_program_nominal_symbol(
                        &definition,
                        &symbol
                    )) {
                    status = RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
                    break;
                }
                status = ruda_bytecode_program_add_schema(
                    &definitions,
                    symbol,
                    definition,
                    work,
                    program->options.max_verify_work
                );
                if (status != RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
                    break;
                }
            }
            if (status != RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
                break;
            }
        }
        if (status != RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
            break;
        }
    }
    ruda_map_dispose(&definitions);
    return status;
}

RudaBytecodeProgramVerifyStatus
ruda_bytecode_program_verify_interface_call(
    const RudaBytecodeProgram *program,
    const RudaBytecodeProgramFunction *caller,
    const RudaBytecodeCall *call,
    size_t *work
) {
    const RudaBytecodeCallArgument *receiver = call->argument_count != 0
        ? ruda_bytecode_call_argument(&caller->body, call->argument_start)
        : NULL;
    const RudaBytecodeInterfaceLayout *required = receiver != NULL
            && receiver->type == RUDA_BYTECODE_TYPE_INTERFACE
        ? ruda_bytecode_interface_layout(&caller->body, receiver->layout)
        : NULL;
    size_t function_index = 0;
    bool found = false;
    if (receiver == NULL || required == NULL
        || call->interface_slot >= required->method_count) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        const RudaBytecodeProgramFunction *owner =
            ruda_bytecode_program_function(program, function_index);
        size_t construction_index = 0;
        if (!ruda_bytecode_program_consume_work(
                work,
                1,
                program->options.max_verify_work
            )) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (owner == NULL) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
        }
        for (construction_index = 0;
            construction_index < owner->body.interface_constructions.length;
            construction_index += 1) {
            const RudaBytecodeInterfaceConstruction *construction =
                ruda_bytecode_interface_construction(
                    &owner->body,
                    (RudaBytecodeInterfaceConstructionId)construction_index
                );
            const RudaBytecodeInterfaceLayout *actual = construction != NULL
                ? ruda_bytecode_interface_layout(
                    &owner->body,
                    construction->interface_id
                )
                : NULL;
            const RudaSymbolId *method = construction != NULL
                    && call->interface_slot < construction->method_count
                ? ruda_vector_at_const(
                    &owner->body.interface_methods,
                    construction->method_start + call->interface_slot
                )
                : NULL;
            const RudaBytecodeProgramFunction *callee = NULL;
            size_t argument_index = 0;
            if (!ruda_bytecode_program_consume_work(
                    work,
                    1,
                    program->options.max_verify_work
                )) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
            }
            if (actual == NULL || !ruda_symbol_id_equal(
                    actual->symbol,
                    required->symbol
                )) {
                continue;
            }
            found = true;
            if (method == NULL) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
            }
            callee = ruda_bytecode_program_function_by_symbol(program, *method);
            if (callee == NULL) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE;
            }
            if (call->argument_count != callee->body.parameter_count
                || !ruda_bytecode_program_same_type(
                    &caller->body,
                    call->result_type,
                    call->result_layout,
                    &callee->body,
                    callee->body.result_type,
                    callee->body.result_layout
                )) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const RudaBytecodeCallArgument *argument =
                    ruda_bytecode_call_argument(
                        &caller->body,
                        call->argument_start + argument_index
                    );
                const RudaBytecodeType *parameter = ruda_vector_at_const(
                    &callee->body.register_types,
                    argument_index
                );
                RudaBytecodeType expected_type = argument_index == 0
                    ? construction->payload_type
                    : (argument != NULL
                        ? argument->type
                        : RUDA_BYTECODE_TYPE_INVALID);
                uint32_t expected_layout = argument_index == 0
                    ? construction->payload_layout
                    : (argument != NULL
                        ? argument->layout
                        : RUDA_BYTECODE_NO_LAYOUT);
                const RudaBytecodeFunction *expected_owner =
                    argument_index == 0 ? &owner->body : &caller->body;
                if (!ruda_bytecode_program_consume_work(
                        work,
                        1,
                        program->options.max_verify_work
                    )) {
                    return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
                }
                if (parameter == NULL || !ruda_bytecode_program_same_type(
                        expected_owner,
                        expected_type,
                        expected_layout,
                        &callee->body,
                        *parameter,
                        ruda_bytecode_register_layout(
                            &callee->body,
                            (uint32_t)argument_index
                        )
                    )) {
                    return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
                }
            }
        }
    }
    return found
        ? RUDA_BYTECODE_PROGRAM_VERIFY_OK
        : RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
}
