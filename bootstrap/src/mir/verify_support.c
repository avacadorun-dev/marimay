#include "mir_internal.h"
#include <limits.h>
#include <string.h>

static bool ruda_mir_type_is_integer(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_U64;
}

static bool ruda_mir_type_is_signed_integer(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_I64;
}
RudaMirVerifyStatus ruda_mir_validate_instruction_types(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaMap *managed_types
) {
    RudaTypeId target_type = ruda_mir_register_type(
        function,
        instruction->target
    );
    RudaTypeId first_type = ruda_mir_register_type(
        function,
        instruction->first
    );
    RudaTypeId second_type = ruda_mir_register_type(
        function,
        instruction->second
    );
    if (target_type == RUDA_TYPE_INVALID) {
        return RUDA_MIR_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->kind) {
        case RUDA_MIR_INTEGER_POLICY:
        case RUDA_MIR_INTEGER_CONVERT:
            return ruda_mir_verify_numeric_instruction(
                function, instruction
            );
        case RUDA_MIR_FORMAT_F32:
        case RUDA_MIR_FORMAT_F64:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_STRING
                    && first_type == (instruction->kind == RUDA_MIR_FORMAT_F32
                        ? RUDA_TYPE_F32 : RUDA_TYPE_F64)
                ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_IO_PRINTLN:
            return ruda_mir_verify_io_instruction(function, instruction);
        case RUDA_MIR_TEST_OUTPUT_BEGIN:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_STRING
                ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_TEST_OUTPUT_END:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_VOID
                    && first_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_I64:
            return target_type == RUDA_TYPE_I64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_INTEGER:
            return ruda_mir_type_is_integer(target_type)
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_F64:
            return target_type == RUDA_TYPE_F64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_BOOL:
            return target_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_STRING:
            if (ruda_mir_string_constant(
                    function,
                    instruction->string_constant
                ) == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            return target_type == RUDA_TYPE_STRING
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_COPY:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == first_type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_ADD_CHECKED_I64:
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
        case RUDA_MIR_DIVIDE_CHECKED_I64:
        case RUDA_MIR_REMAINDER_CHECKED_I64:
        case RUDA_MIR_BITWISE_AND_I64:
        case RUDA_MIR_BITWISE_XOR_I64:
        case RUDA_MIR_BITWISE_OR_I64:
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
        case RUDA_MIR_SHIFT_RIGHT_I64:
        case RUDA_MIR_ADD_F64:
        case RUDA_MIR_SUBTRACT_F64:
        case RUDA_MIR_MULTIPLY_F64:
        case RUDA_MIR_DIVIDE_F64:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (instruction->kind == RUDA_MIR_ADD_F64
                || instruction->kind == RUDA_MIR_SUBTRACT_F64
                || instruction->kind == RUDA_MIR_MULTIPLY_F64
                || instruction->kind == RUDA_MIR_DIVIDE_F64) {
                return target_type == RUDA_TYPE_F64
                        && first_type == RUDA_TYPE_F64
                        && second_type == RUDA_TYPE_F64
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return ruda_mir_type_is_integer(target_type)
                    && target_type == first_type
                    && target_type == second_type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_NEGATE_CHECKED_I64:
        case RUDA_MIR_BITWISE_NOT_I64:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return ruda_mir_type_is_integer(target_type)
                    && target_type == first_type
                    && (instruction->kind == RUDA_MIR_BITWISE_NOT_I64
                        || ruda_mir_type_is_signed_integer(target_type))
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_NEGATE_F64:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_F64 && first_type == RUDA_TYPE_F64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_NOT_BOOL:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_I64:
        case RUDA_MIR_LESS_I64:
        case RUDA_MIR_LESS_EQUAL_I64:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && ruda_mir_type_is_integer(first_type)
                    && first_type == second_type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_F64:
        case RUDA_MIR_LESS_F64:
        case RUDA_MIR_LESS_EQUAL_F64:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_F64
                    && second_type == RUDA_TYPE_F64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_BOOL:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_BOOL
                    && second_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_STRING:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_STRING
                    && second_type == RUDA_TYPE_STRING
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_RECORD_IDENTITY_SAME: {
            const RudaMirRecord *record = ruda_mir_record(
                function,
                instruction->record
            );
            if (record == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == record->type
                    && second_type == record->type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        case RUDA_MIR_RECORD_NEW: {
            const RudaMirRecordConstruction *construction =
                ruda_mir_record_construction(
                    function,
                    instruction->record_construction
                );
            const RudaMirRecord *record = construction != NULL
                ? ruda_mir_record(function, construction->record)
                : NULL;
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || record == NULL
                || instruction->record != construction->record
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->record_arguments.length
                || construction->argument_count != record->field_count) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != record->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const RudaMirRecordArgument *argument =
                    ruda_mir_record_argument(
                        function,
                        construction->argument_start + index
                    );
                const RudaMirRecordField *field = ruda_mir_record_field(
                    function,
                    record,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (field == NULL || actual != argument->type
                    || argument->type != field->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_RECORD_GET:
        case RUDA_MIR_RECORD_SET: {
            const RudaMirRecord *record = ruda_mir_record(
                function,
                instruction->record
            );
            const RudaMirRecordField *field = ruda_mir_record_field(
                function,
                record,
                instruction->field
            );
            if (record == NULL || field == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID
                || (instruction->kind == RUDA_MIR_RECORD_SET
                    && second_type == RUDA_TYPE_INVALID)) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != record->type || target_type != field->type
                || (instruction->kind == RUDA_MIR_RECORD_SET
                    && (second_type != field->type || !field->is_mutable))) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ARRAY_NEW: {
            const RudaMirArrayConstruction *construction =
                ruda_mir_array_construction(
                    function,
                    instruction->array_construction
                );
            const RudaMirArray *array = construction != NULL
                ? ruda_mir_array(function, construction->array)
                : NULL;
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || array == NULL
                || instruction->array != construction->array
                || target_type != array->type
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->array_arguments.length) {
                return construction != NULL && array != NULL
                        && target_type != array->type
                    ? RUDA_MIR_VERIFY_TYPE_MISMATCH
                    : RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            for (index = construction->argument_start; index < argument_end;
                index += 1) {
                const RudaMirArrayArgument *argument = ruda_mir_array_argument(
                    function,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(function, argument->register_id)
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (argument->type != array->element_type
                    || actual != argument->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ARRAY_PUSH: {
            const RudaMirArray *array = ruda_mir_array(
                function, instruction->array
            );
            if (array == NULL || first_type != array->type
                || second_type != array->element_type
                || target_type != array->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ARRAY_POP:
        case RUDA_MIR_ARRAY_TRUNCATE:
        case RUDA_MIR_ARRAY_CLEAR:
        case RUDA_MIR_ARRAY_RESERVE:
        case RUDA_MIR_ARRAY_GENERATION:
        case RUDA_MIR_ARRAY_REQUIRE_GENERATION: {
            const RudaMirArray *array = ruda_mir_array(
                function, instruction->array
            );
            if (array == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID
                || ((instruction->kind == RUDA_MIR_ARRAY_TRUNCATE
                        || instruction->kind == RUDA_MIR_ARRAY_RESERVE
                        || instruction->kind
                            == RUDA_MIR_ARRAY_REQUIRE_GENERATION)
                    && second_type == RUDA_TYPE_INVALID)) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != array->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_POP) {
                return target_type == array->element_type
                    ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_GENERATION) {
                return target_type == RUDA_TYPE_U64
                    ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_GENERATION) {
                return second_type == RUDA_TYPE_U64
                        && target_type == array->type
                    ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if ((instruction->kind == RUDA_MIR_ARRAY_TRUNCATE
                    || instruction->kind == RUDA_MIR_ARRAY_RESERVE)
                && second_type != RUDA_TYPE_I64) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == array->type
                ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        case RUDA_MIR_ARRAY_LENGTH:
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
        case RUDA_MIR_ARRAY_GET:
        case RUDA_MIR_ARRAY_SET: {
            const RudaMirArray *array = ruda_mir_array(
                function,
                instruction->array
            );
            if (array == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID
                || (instruction->kind != RUDA_MIR_ARRAY_LENGTH
                    && second_type == RUDA_TYPE_INVALID)
                || (instruction->kind == RUDA_MIR_ARRAY_SET
                    && ruda_mir_register_type(
                        function,
                        instruction->third
                    ) == RUDA_TYPE_INVALID)) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != array->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_LENGTH) {
                return target_type == RUDA_TYPE_I64
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX) {
                return second_type == RUDA_TYPE_I64
                        && target_type == array->type
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (second_type != RUDA_TYPE_I64
                || target_type != array->element_type
                || (instruction->kind == RUDA_MIR_ARRAY_SET
                    && ruda_mir_register_type(
                        function,
                        instruction->third
                    ) != array->element_type)) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ENUM_NEW: {
            const RudaMirEnumConstruction *construction =
                ruda_mir_enum_construction(
                    function,
                    instruction->enum_construction
                );
            const RudaMirEnum *enum_layout = construction != NULL
                ? ruda_mir_enum(function, construction->enum_id)
                : NULL;
            const RudaMirEnumVariant *variant = construction != NULL
                ? ruda_mir_enum_variant(
                    function,
                    enum_layout,
                    construction->variant
                )
                : NULL;
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || enum_layout == NULL || variant == NULL
                || instruction->enum_id != construction->enum_id
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->enum_arguments.length
                || construction->argument_count != variant->payload_count) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != enum_layout->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const RudaMirEnumArgument *argument = ruda_mir_enum_argument(
                    function,
                    construction->argument_start + index
                );
                const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
                    function,
                    variant,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (payload == NULL || actual != argument->type
                    || argument->type != payload->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ENUM_GET_PAYLOAD: {
            const RudaMirEnum *enum_layout = ruda_mir_enum(
                function,
                instruction->enum_id
            );
            const RudaMirEnumVariant *variant = ruda_mir_enum_variant(
                function,
                enum_layout,
                instruction->variant
            );
            const RudaMirEnumPayload *payload =
                instruction->kind == RUDA_MIR_ENUM_GET_PAYLOAD
                    ? ruda_mir_enum_payload(
                        function,
                        variant,
                        instruction->field
                    )
                    : NULL;
            if (enum_layout == NULL || variant == NULL
                || (instruction->kind == RUDA_MIR_ENUM_GET_PAYLOAD
                    && payload == NULL)) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != enum_layout->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ENUM_IS_VARIANT) {
                return target_type == RUDA_TYPE_BOOL
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ENUM_REQUIRE_VARIANT) {
                return target_type == enum_layout->type
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == payload->type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        case RUDA_MIR_CALL:
        case RUDA_MIR_DEFER_REGISTER:
        case RUDA_MIR_DEFER_CALL: {
            const RudaMirCall *call = ruda_mir_call(
                function,
                instruction->call
            );
            size_t argument_end = 0;
            size_t index = 0;
            bool deferred = instruction->kind != RUDA_MIR_CALL;
            if (call == NULL || call->result_type == RUDA_TYPE_NEVER
                || (!deferred && call->interface_slot != UINT32_MAX)
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if ((deferred && (target_type != RUDA_TYPE_BOOL
                    || call->result_type != RUDA_TYPE_VOID))
                || (!deferred && target_type != call->result_type)) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (deferred && call->interface_slot != UINT32_MAX) {
                const RudaMirCallArgument *receiver = call->argument_count != 0
                    ? ruda_mir_call_argument(function, call->argument_start)
                    : NULL;
                const RudaMirManagedLayoutInfo *receiver_layout =
                    receiver != NULL && managed_types != NULL
                    ? ruda_map_get_const(
                        managed_types, &receiver->type,
                        sizeof(receiver->type)
                    )
                    : NULL;
                const RudaMirInterface *interface_layout =
                    receiver_layout != NULL
                        && receiver_layout->kind
                            == RUDA_MIR_MANAGED_LAYOUT_INTERFACE
                        && receiver_layout->layout < function->interfaces.length
                    ? ruda_vector_at_const(
                        &function->interfaces, receiver_layout->layout
                    )
                    : NULL;
                if (interface_layout == NULL
                    || call->interface_slot >= interface_layout->method_count) {
                    return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                }
            }
            for (index = call->argument_start; index < argument_end;
                index += 1) {
                const RudaMirCallArgument *argument = ruda_mir_call_argument(
                    function,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (actual != argument->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_INTERFACE_NEW: {
            const RudaMirInterfaceConstruction *construction =
                instruction->interface_construction
                    < function->interface_constructions.length
                ? ruda_vector_at_const(
                    &function->interface_constructions,
                    instruction->interface_construction
                ) : NULL;
            const RudaMirInterface *layout = construction != NULL
                ? ruda_vector_at_const(
                    &function->interfaces, construction->interface_id
                ) : NULL;
            const RudaMirManagedLayoutInfo *payload_layout =
                construction != NULL && managed_types != NULL
                ? ruda_map_get_const(
                    managed_types,
                    &construction->payload_type,
                    sizeof(construction->payload_type)
                )
                : NULL;
            RudaTypeId payload_type = construction != NULL
                ? ruda_mir_register_type(function, construction->payload)
                : RUDA_TYPE_INVALID;
            if (construction == NULL || layout == NULL
                || construction->method_count != layout->method_count
                || payload_layout == NULL
                || (payload_layout->kind != RUDA_MIR_MANAGED_LAYOUT_RECORD
                    && payload_layout->kind != RUDA_MIR_MANAGED_LAYOUT_ENUM)) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (payload_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (payload_type != construction->payload_type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == layout->type ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        case RUDA_MIR_CALL_INTERFACE: {
            const RudaMirCall *call = ruda_mir_call(function, instruction->call);
            size_t argument_end = 0;
            size_t argument_index = 0;
            const RudaMirCallArgument *receiver = call != NULL
                    && call->argument_count != 0
                ? ruda_mir_call_argument(function, call->argument_start)
                : NULL;
            const RudaMirManagedLayoutInfo *receiver_layout =
                receiver != NULL && managed_types != NULL
                ? ruda_map_get_const(
                    managed_types,
                    &receiver->type,
                    sizeof(receiver->type)
                )
                : NULL;
            const RudaMirInterface *interface_layout =
                receiver_layout != NULL
                    && receiver_layout->kind
                        == RUDA_MIR_MANAGED_LAYOUT_INTERFACE
                    && receiver_layout->layout < function->interfaces.length
                ? ruda_vector_at_const(
                    &function->interfaces,
                    receiver_layout->layout
                )
                : NULL;
            if (call == NULL || call->interface_slot == UINT32_MAX
                || call->argument_count == 0
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                )
                || argument_end > function->call_arguments.length
                || interface_layout == NULL
                || call->interface_slot >= interface_layout->method_count) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != call->result_type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (argument_index = call->argument_start;
                argument_index < argument_end;
                argument_index += 1) {
                const RudaMirCallArgument *argument = ruda_mir_call_argument(
                    function,
                    argument_index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (actual != argument->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
    }
    return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
}
bool ruda_mir_instruction_uses_first(RudaMirInstructionKind kind) {
    return kind != RUDA_MIR_CONSTANT_I64
        && kind != RUDA_MIR_CONSTANT_INTEGER
        && kind != RUDA_MIR_CONSTANT_F64
        && kind != RUDA_MIR_CONSTANT_BOOL
        && kind != RUDA_MIR_CONSTANT_STRING && kind != RUDA_MIR_RECORD_NEW
        && kind != RUDA_MIR_ENUM_NEW && kind != RUDA_MIR_ARRAY_NEW
        && kind != RUDA_MIR_INTERFACE_NEW && kind != RUDA_MIR_CALL
        && kind != RUDA_MIR_DEFER_REGISTER
        && kind != RUDA_MIR_DEFER_CALL
        && kind != RUDA_MIR_CALL_INTERFACE;
}
bool ruda_mir_instruction_uses_second(RudaMirInstructionKind kind) {
    switch (kind) {
        case RUDA_MIR_ADD_CHECKED_I64:
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
        case RUDA_MIR_DIVIDE_CHECKED_I64:
        case RUDA_MIR_REMAINDER_CHECKED_I64:
        case RUDA_MIR_BITWISE_AND_I64:
        case RUDA_MIR_BITWISE_XOR_I64:
        case RUDA_MIR_BITWISE_OR_I64:
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
        case RUDA_MIR_SHIFT_RIGHT_I64:
        case RUDA_MIR_ADD_F64:
        case RUDA_MIR_SUBTRACT_F64:
        case RUDA_MIR_MULTIPLY_F64:
        case RUDA_MIR_DIVIDE_F64:
        case RUDA_MIR_EQUAL_I64:
        case RUDA_MIR_EQUAL_F64:
        case RUDA_MIR_EQUAL_BOOL:
        case RUDA_MIR_EQUAL_STRING:
        case RUDA_MIR_LESS_I64:
        case RUDA_MIR_LESS_EQUAL_I64:
        case RUDA_MIR_LESS_F64:
        case RUDA_MIR_LESS_EQUAL_F64:
        case RUDA_MIR_RECORD_SET:
        case RUDA_MIR_RECORD_IDENTITY_SAME:
        case RUDA_MIR_ARRAY_GET:
        case RUDA_MIR_ARRAY_SET:
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
        case RUDA_MIR_ARRAY_PUSH:
        case RUDA_MIR_ARRAY_TRUNCATE:
        case RUDA_MIR_ARRAY_RESERVE:
        case RUDA_MIR_ARRAY_REQUIRE_GENERATION:
        case RUDA_MIR_INTEGER_POLICY:
            return true;
        case RUDA_MIR_CONSTANT_I64:
        case RUDA_MIR_CONSTANT_INTEGER:
        case RUDA_MIR_CONSTANT_F64:
        case RUDA_MIR_CONSTANT_BOOL:
        case RUDA_MIR_CONSTANT_STRING:
        case RUDA_MIR_COPY:
        case RUDA_MIR_NEGATE_CHECKED_I64:
        case RUDA_MIR_NEGATE_F64:
        case RUDA_MIR_NOT_BOOL:
        case RUDA_MIR_BITWISE_NOT_I64:
        case RUDA_MIR_RECORD_NEW:
        case RUDA_MIR_RECORD_GET:
        case RUDA_MIR_ARRAY_NEW:
        case RUDA_MIR_ARRAY_LENGTH:
        case RUDA_MIR_ARRAY_POP:
        case RUDA_MIR_ARRAY_CLEAR:
        case RUDA_MIR_ARRAY_GENERATION:
        case RUDA_MIR_ENUM_NEW:
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ENUM_GET_PAYLOAD:
        case RUDA_MIR_CALL:
        case RUDA_MIR_INTERFACE_NEW:
        case RUDA_MIR_CALL_INTERFACE:
        case RUDA_MIR_INTEGER_CONVERT:
        case RUDA_MIR_FORMAT_F32:
        case RUDA_MIR_FORMAT_F64:
        case RUDA_MIR_IO_PRINTLN:
        case RUDA_MIR_TEST_OUTPUT_BEGIN:
        case RUDA_MIR_TEST_OUTPUT_END:
        case RUDA_MIR_DEFER_REGISTER:
        case RUDA_MIR_DEFER_CALL:
            return false;
    }
    return false;
}
bool ruda_mir_instruction_uses_third(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_ARRAY_SET;
}
bool ruda_mir_assigned_register(
    const unsigned char *assigned,
    size_t register_count,
    RudaMirRegister register_id
) {
    return register_id != RUDA_MIR_NO_REGISTER
        && register_id < register_count && assigned[register_id] != 0;
}
void ruda_mir_transfer_assignments(
    const RudaMirBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t index = 0;
    for (index = 0; index < block->instructions.length; index += 1) {
        const RudaMirInstruction *instruction = ruda_vector_at_const(
            &block->instructions,
            index
        );
        if (instruction != NULL && instruction->target < register_count) {
            assigned[instruction->target] = 1;
        }
    }
}
const uint64_t ruda_mir_checked_index_top = UINT64_MAX;
const uint64_t ruda_mir_checked_index_none = UINT64_MAX - 1;
void ruda_mir_transfer_checked_index_instruction(
    const RudaMirInstruction *instruction,
    uint64_t *checked,
    size_t register_count
) {
    size_t register_index = 0;
    if (instruction == NULL || checked == NULL
        || instruction->target >= register_count) {
        return;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_POP
        || instruction->kind == RUDA_MIR_ARRAY_TRUNCATE
        || instruction->kind == RUDA_MIR_ARRAY_CLEAR) {
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            checked[register_index] = ruda_mir_checked_index_none;
        }
        return;
    }
    for (register_index = 0; register_index < register_count;
        register_index += 1) {
        if (checked[register_index] == instruction->target) {
            checked[register_index] = ruda_mir_checked_index_none;
        }
    }
    checked[instruction->target] = instruction->kind
            == RUDA_MIR_ARRAY_REQUIRE_INDEX
        ? instruction->second
        : ruda_mir_checked_index_none;
}
void ruda_mir_transfer_checked_indices(
    const RudaMirBlock *block,
    uint64_t *checked,
    size_t register_count
) {
    size_t instruction_index = 0;
    if (block == NULL || checked == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index < block->instructions.length;
        instruction_index += 1) {
        ruda_mir_transfer_checked_index_instruction(
            ruda_vector_at_const(&block->instructions, instruction_index),
            checked,
            register_count
        );
    }
}
uint64_t ruda_mir_meet_checked_index(uint64_t left, uint64_t right) {
    if (left == ruda_mir_checked_index_top) {
        return right;
    }
    if (right == ruda_mir_checked_index_top || left == right) {
        return left;
    }
    return ruda_mir_checked_index_none;
}
bool ruda_mir_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (!ruda_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}
