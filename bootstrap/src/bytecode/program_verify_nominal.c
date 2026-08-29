#include "program_internal.h"

#include <string.h>

#include "mmay/base/map.h"
#include "bytecode_internal.h"

typedef struct MarimayBytecodeNominalDefinition {
    const MarimayBytecodeFunction *function;
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeNominalDefinition;

static bool mmay_bytecode_program_consume_work(
    size_t *work, size_t amount, size_t limit
) {
    size_t next = 0;
    if (work == NULL || !mmay_size_add(*work, amount, &next)
        || next > limit) {
        return false;
    }
    *work = next;
    return true;
}

bool mmay_bytecode_program_consume_nominal_work(
    const MarimayBytecodeFunction *function,
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
        if (!mmay_bytecode_program_consume_work(work, counts[index], limit)) {
            return false;
        }
    }
    return true;
}

bool mmay_bytecode_program_same_type(
    const MarimayBytecodeFunction *left_function,
    MarimayBytecodeType left_type,
    uint32_t left_layout,
    const MarimayBytecodeFunction *right_function,
    MarimayBytecodeType right_type,
    uint32_t right_layout
) {
    MarimaySymbolId left_symbol;
    MarimaySymbolId right_symbol;
    bool nominal = left_type == MMAY_BYTECODE_TYPE_RECORD
        || left_type == MMAY_BYTECODE_TYPE_ENUM
        || left_type == MMAY_BYTECODE_TYPE_INTERFACE;
    if (left_type != right_type) {
        return false;
    }
    if (left_type == MMAY_BYTECODE_TYPE_ARRAY) {
        const MarimayBytecodeArrayType *left = mmay_bytecode_array_type(
            left_function, left_layout
        );
        const MarimayBytecodeArrayType *right = mmay_bytecode_array_type(
            right_function, right_layout
        );
        return left != NULL && right != NULL
            && mmay_bytecode_program_same_type(
                left_function,
                left->element_type,
                left->element_layout,
                right_function,
                right->element_type,
                right->element_layout
            );
    }
    if (left_type == MMAY_BYTECODE_TYPE_MAP
        || left_type == MMAY_BYTECODE_TYPE_SET) {
        const MarimayBytecodeCollectionType *left =
            mmay_bytecode_collection_type(left_function, left_layout);
        const MarimayBytecodeCollectionType *right =
            mmay_bytecode_collection_type(right_function, right_layout);
        return left != NULL && right != NULL
            && left->is_set == right->is_set
            && mmay_bytecode_program_same_type(
                left_function, left->key_type, left->key_layout,
                right_function, right->key_type, right->key_layout
            ) && (left->is_set || mmay_bytecode_program_same_type(
                left_function, left->value_type, left->value_layout,
                right_function, right->value_type, right->value_layout
            ));
    }
    if (!nominal) {
        return left_layout == MMAY_BYTECODE_NO_LAYOUT
            && right_layout == MMAY_BYTECODE_NO_LAYOUT;
    }
    memset(&left_symbol, 0, sizeof(left_symbol));
    memset(&right_symbol, 0, sizeof(right_symbol));
    if (left_type == MMAY_BYTECODE_TYPE_RECORD) {
        const MarimayBytecodeRecordLayout *left = mmay_bytecode_record_layout(
            left_function,
            left_layout
        );
        const MarimayBytecodeRecordLayout *right = mmay_bytecode_record_layout(
            right_function,
            right_layout
        );
        if (left == NULL || right == NULL) {
            return false;
        }
        left_symbol = left->symbol;
        right_symbol = right->symbol;
    } else if (left_type == MMAY_BYTECODE_TYPE_ENUM) {
        const MarimayBytecodeEnumLayout *left = mmay_bytecode_enum_layout(
            left_function,
            left_layout
        );
        const MarimayBytecodeEnumLayout *right = mmay_bytecode_enum_layout(
            right_function,
            right_layout
        );
        if (left == NULL || right == NULL) {
            return false;
        }
        left_symbol = left->symbol;
        right_symbol = right->symbol;
    } else {
        const MarimayBytecodeInterfaceLayout *left =
            mmay_bytecode_interface_layout(left_function, left_layout);
        const MarimayBytecodeInterfaceLayout *right =
            mmay_bytecode_interface_layout(right_function, right_layout);
        if (left == NULL || right == NULL) {
            return false;
        }
        left_symbol = left->symbol;
        right_symbol = right->symbol;
    }
    return mmay_symbol_id_equal(left_symbol, right_symbol);
}

static MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_compare_record(
    const MarimayBytecodeNominalDefinition *left,
    const MarimayBytecodeNominalDefinition *right,
    size_t *work,
    size_t limit
) {
    const MarimayBytecodeRecordLayout *left_layout = mmay_bytecode_record_layout(
        left->function,
        left->layout
    );
    const MarimayBytecodeRecordLayout *right_layout = mmay_bytecode_record_layout(
        right->function,
        right->layout
    );
    size_t field = 0;
    if (left_layout == NULL || right_layout == NULL
        || left_layout->field_count != right_layout->field_count) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    for (field = 0; field < left_layout->field_count; field += 1) {
        const MarimayBytecodeRecordField *left_field = mmay_bytecode_record_field(
            left->function,
            left_layout,
            field
        );
        const MarimayBytecodeRecordField *right_field = mmay_bytecode_record_field(
            right->function,
            right_layout,
            field
        );
        if (!mmay_bytecode_program_consume_work(work, 1, limit)) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (left_field == NULL || right_field == NULL
            || left_field->is_mutable != right_field->is_mutable
            || !mmay_bytecode_program_same_type(
                left->function,
                left_field->type,
                left_field->layout,
                right->function,
                right_field->type,
                right_field->layout
            )) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
        }
    }
    return MMAY_BYTECODE_PROGRAM_VERIFY_OK;
}

static MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_compare_enum(
    const MarimayBytecodeNominalDefinition *left,
    const MarimayBytecodeNominalDefinition *right,
    size_t *work,
    size_t limit
) {
    const MarimayBytecodeEnumLayout *left_layout = mmay_bytecode_enum_layout(
        left->function,
        left->layout
    );
    const MarimayBytecodeEnumLayout *right_layout = mmay_bytecode_enum_layout(
        right->function,
        right->layout
    );
    size_t variant = 0;
    size_t payload = 0;
    if (left_layout == NULL || right_layout == NULL
        || left_layout->variant_count != right_layout->variant_count
        || left_layout->payload_count != right_layout->payload_count) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    for (variant = 0; variant < left_layout->variant_count; variant += 1) {
        const MarimayBytecodeEnumVariant *left_variant = mmay_vector_at_const(
            &left->function->enum_variants,
            left_layout->variant_start + variant
        );
        const MarimayBytecodeEnumVariant *right_variant = mmay_vector_at_const(
            &right->function->enum_variants,
            right_layout->variant_start + variant
        );
        if (!mmay_bytecode_program_consume_work(work, 1, limit)) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (left_variant == NULL || right_variant == NULL
            || left_variant->payload_count != right_variant->payload_count) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
        }
    }
    for (payload = 0; payload < left_layout->payload_count; payload += 1) {
        const MarimayBytecodeEnumPayload *left_payload = mmay_vector_at_const(
            &left->function->enum_payloads,
            left_layout->payload_start + payload
        );
        const MarimayBytecodeEnumPayload *right_payload = mmay_vector_at_const(
            &right->function->enum_payloads,
            right_layout->payload_start + payload
        );
        if (!mmay_bytecode_program_consume_work(work, 1, limit)) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (left_payload == NULL || right_payload == NULL
            || !mmay_bytecode_program_same_type(
                left->function,
                left_payload->type,
                left_payload->layout,
                right->function,
                right_payload->type,
                right_payload->layout
            )) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
        }
    }
    return MMAY_BYTECODE_PROGRAM_VERIFY_OK;
}

static MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_compare_schema(
    const MarimayBytecodeNominalDefinition *left,
    const MarimayBytecodeNominalDefinition *right,
    size_t *work,
    size_t limit
) {
    if (left->type != right->type) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    if (left->type == MMAY_BYTECODE_TYPE_RECORD) {
        return mmay_bytecode_program_compare_record(left, right, work, limit);
    }
    if (left->type == MMAY_BYTECODE_TYPE_ENUM) {
        return mmay_bytecode_program_compare_enum(left, right, work, limit);
    }
    if (left->type == MMAY_BYTECODE_TYPE_INTERFACE) {
        const MarimayBytecodeInterfaceLayout *left_layout =
            mmay_bytecode_interface_layout(left->function, left->layout);
        const MarimayBytecodeInterfaceLayout *right_layout =
            mmay_bytecode_interface_layout(right->function, right->layout);
        return left_layout != NULL && right_layout != NULL
                && left_layout->method_count == right_layout->method_count
            ? MMAY_BYTECODE_PROGRAM_VERIFY_OK
            : MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    return MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
}

static MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_add_schema(
    MarimayMap *definitions,
    MarimaySymbolId symbol,
    MarimayBytecodeNominalDefinition definition,
    size_t *work,
    size_t limit
) {
    const MarimayBytecodeNominalDefinition *existing = mmay_map_get_const(
        definitions,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    if (!mmay_bytecode_program_consume_work(work, 1, limit)) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
    }
    if (existing != NULL) {
        return mmay_bytecode_program_compare_schema(
            existing,
            &definition,
            work,
            limit
        );
    }
    return mmay_map_put(
            definitions,
            symbol.bytes,
            sizeof(symbol.bytes),
            &definition
        )
        ? MMAY_BYTECODE_PROGRAM_VERIFY_OK
        : MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED;
}

static bool mmay_bytecode_program_nominal_symbol(
    const MarimayBytecodeNominalDefinition *definition,
    MarimaySymbolId *symbol
) {
    if (definition == NULL || definition->function == NULL || symbol == NULL) {
        return false;
    }
    if (definition->type == MMAY_BYTECODE_TYPE_RECORD) {
        const MarimayBytecodeRecordLayout *layout = mmay_bytecode_record_layout(
            definition->function,
            definition->layout
        );
        if (layout == NULL) {
            return false;
        }
        *symbol = layout->symbol;
        return true;
    }
    if (definition->type == MMAY_BYTECODE_TYPE_ENUM) {
        const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
            definition->function,
            definition->layout
        );
        if (layout == NULL) {
            return false;
        }
        *symbol = layout->symbol;
        return true;
    }
    if (definition->type == MMAY_BYTECODE_TYPE_INTERFACE) {
        const MarimayBytecodeInterfaceLayout *layout =
            mmay_bytecode_interface_layout(
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

MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_verify_nominal_schemas(
    const MarimayBytecodeProgram *program,
    size_t *work
) {
    MarimayMap definitions;
    MarimayBytecodeProgramVerifyStatus status = MMAY_BYTECODE_PROGRAM_VERIFY_OK;
    size_t function_index = 0;
    memset(&definitions, 0, sizeof(definitions));
    if (program == NULL || work == NULL || !mmay_map_init(
            &definitions,
            program->allocator,
            sizeof(MarimayBytecodeNominalDefinition),
            UINT64_C(0x243f6a8885a308d3),
            UINT64_C(0x13198a2e03707344)
        )) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED;
    }
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        const MarimayBytecodeProgramFunction *owner =
            mmay_bytecode_program_function(program, function_index);
        const MarimayBytecodeType types[] = {
            MMAY_BYTECODE_TYPE_RECORD,
            MMAY_BYTECODE_TYPE_ENUM,
            MMAY_BYTECODE_TYPE_INTERFACE
        };
        const size_t counts[] = {
            owner != NULL ? owner->body.record_layouts.length : 0,
            owner != NULL ? owner->body.enum_layouts.length : 0,
            owner != NULL ? owner->body.interface_layouts.length : 0
        };
        size_t type_index = 0;
        if (owner == NULL) {
            status = MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
            break;
        }
        for (type_index = 0; type_index < sizeof(types) / sizeof(types[0]);
            type_index += 1) {
            size_t layout = 0;
            for (layout = 0; layout < counts[type_index]; layout += 1) {
                MarimayBytecodeNominalDefinition definition;
                MarimaySymbolId symbol;
                definition.function = &owner->body;
                definition.type = types[type_index];
                definition.layout = (uint32_t)layout;
                if (!mmay_bytecode_program_nominal_symbol(
                        &definition,
                        &symbol
                    )) {
                    status = MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
                    break;
                }
                status = mmay_bytecode_program_add_schema(
                    &definitions,
                    symbol,
                    definition,
                    work,
                    program->options.max_verify_work
                );
                if (status != MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
                    break;
                }
            }
            if (status != MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
                break;
            }
        }
        if (status != MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
            break;
        }
    }
    mmay_map_dispose(&definitions);
    return status;
}

MarimayBytecodeProgramVerifyStatus
mmay_bytecode_program_verify_interface_call(
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeProgramFunction *caller,
    const MarimayBytecodeCall *call,
    size_t *work
) {
    const MarimayBytecodeCallArgument *receiver = call->argument_count != 0
        ? mmay_bytecode_call_argument(&caller->body, call->argument_start)
        : NULL;
    const MarimayBytecodeInterfaceLayout *required = receiver != NULL
            && receiver->type == MMAY_BYTECODE_TYPE_INTERFACE
        ? mmay_bytecode_interface_layout(&caller->body, receiver->layout)
        : NULL;
    size_t function_index = 0;
    bool found = false;
    if (receiver == NULL || required == NULL
        || call->interface_slot >= required->method_count) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
    }
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        const MarimayBytecodeProgramFunction *owner =
            mmay_bytecode_program_function(program, function_index);
        size_t construction_index = 0;
        if (!mmay_bytecode_program_consume_work(
                work,
                1,
                program->options.max_verify_work
            )) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (owner == NULL) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
        }
        for (construction_index = 0;
            construction_index < owner->body.interface_constructions.length;
            construction_index += 1) {
            const MarimayBytecodeInterfaceConstruction *construction =
                mmay_bytecode_interface_construction(
                    &owner->body,
                    (MarimayBytecodeInterfaceConstructionId)construction_index
                );
            const MarimayBytecodeInterfaceLayout *actual = construction != NULL
                ? mmay_bytecode_interface_layout(
                    &owner->body,
                    construction->interface_id
                )
                : NULL;
            const MarimaySymbolId *method = construction != NULL
                    && call->interface_slot < construction->method_count
                ? mmay_vector_at_const(
                    &owner->body.interface_methods,
                    construction->method_start + call->interface_slot
                )
                : NULL;
            const MarimayBytecodeProgramFunction *callee = NULL;
            size_t argument_index = 0;
            if (!mmay_bytecode_program_consume_work(
                    work,
                    1,
                    program->options.max_verify_work
                )) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
            }
            if (actual == NULL || !mmay_symbol_id_equal(
                    actual->symbol,
                    required->symbol
                )) {
                continue;
            }
            found = true;
            if (method == NULL) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
            }
            callee = mmay_bytecode_program_function_by_symbol(program, *method);
            if (callee == NULL) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE;
            }
            if (call->argument_count != callee->body.parameter_count
                || !mmay_bytecode_program_same_type(
                    &caller->body,
                    call->result_type,
                    call->result_layout,
                    &callee->body,
                    callee->body.result_type,
                    callee->body.result_layout
                )) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const MarimayBytecodeCallArgument *argument =
                    mmay_bytecode_call_argument(
                        &caller->body,
                        call->argument_start + argument_index
                    );
                const MarimayBytecodeType *parameter = mmay_vector_at_const(
                    &callee->body.register_types,
                    argument_index
                );
                MarimayBytecodeType expected_type = argument_index == 0
                    ? construction->payload_type
                    : (argument != NULL
                        ? argument->type
                        : MMAY_BYTECODE_TYPE_INVALID);
                uint32_t expected_layout = argument_index == 0
                    ? construction->payload_layout
                    : (argument != NULL
                        ? argument->layout
                        : MMAY_BYTECODE_NO_LAYOUT);
                const MarimayBytecodeFunction *expected_owner =
                    argument_index == 0 ? &owner->body : &caller->body;
                if (!mmay_bytecode_program_consume_work(
                        work,
                        1,
                        program->options.max_verify_work
                    )) {
                    return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
                }
                if (parameter == NULL || !mmay_bytecode_program_same_type(
                        expected_owner,
                        expected_type,
                        expected_layout,
                        &callee->body,
                        *parameter,
                        mmay_bytecode_register_layout(
                            &callee->body,
                            (uint32_t)argument_index
                        )
                    )) {
                    return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
                }
            }
        }
    }
    return found
        ? MMAY_BYTECODE_PROGRAM_VERIFY_OK
        : MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
}
