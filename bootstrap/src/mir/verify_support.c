#include "mir_internal.h"
#include "mir_collections_internal.h"
#include "fs_internal.h"
#include "context_internal.h"
#include "text_internal.h"
#include <limits.h>
#include <string.h>

static bool mmay_mir_type_is_integer(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64;
}

static bool mmay_mir_type_is_signed_integer(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_I64;
}
MarimayMirVerifyStatus mmay_mir_validate_instruction_types(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMap *managed_types
) {
    MarimayTypeId target_type = mmay_mir_register_type(
        function,
        instruction->target
    );
    MarimayTypeId first_type = mmay_mir_register_type(
        function,
        instruction->first
    );
    MarimayTypeId second_type = mmay_mir_register_type(
        function,
        instruction->second
    );
    MarimayTypeId third_type = mmay_mir_register_type(
        function,
        instruction->third
    );
    if (target_type == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->kind) {
        case MMAY_MIR_INTEGER_POLICY:
        case MMAY_MIR_INTEGER_CONVERT:
            return mmay_mir_verify_numeric_instruction(
                function, instruction
            );
        case MMAY_MIR_FORMAT_F32:
        case MMAY_MIR_FORMAT_F64:
        case MMAY_MIR_FORMAT_BOOL:
        case MMAY_MIR_FORMAT_INTEGER:
        case MMAY_MIR_STRING_CONCAT:
            return mmay_mir_verify_format_instruction(function, instruction);
        case MMAY_MIR_PARSE_INTEGER:
            return mmay_mir_verify_parse_instruction(function, instruction);
        case MMAY_MIR_TEXT_UNARY:
        case MMAY_MIR_TEXT_BINARY:
            return mmay_mir_verify_text_instruction(function, instruction);
        case MMAY_MIR_IO_PRINTLN:
        case MMAY_MIR_IO_WRITE:
        case MMAY_MIR_IO_FLUSH:
        case MMAY_MIR_IO_READ_LINE:
        case MMAY_MIR_IO_READ_BYTES:
        case MMAY_MIR_IO_WRITE_BYTES:
            return mmay_mir_verify_io_instruction(function, instruction);
        case MMAY_MIR_PATH:
        case MMAY_MIR_FS_UNARY:
        case MMAY_MIR_FS_BINARY:
            return mmay_mir_verify_fs_instruction(function, instruction);
        case MMAY_MIR_ENV_READ:
        case MMAY_MIR_PROCESS_CONTEXT:
            return mmay_mir_verify_context_instruction(function, instruction);
        case MMAY_MIR_TEST_ASSERT:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_VOID
                    && first_type == MMAY_TYPE_BOOL
                    && second_type == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEST_OUTPUT_BEGIN:
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && first_type == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEST_OUTPUT_END:
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_VOID
                    && first_type == MMAY_TYPE_BOOL
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_CONSTANT_I64:
            return target_type == MMAY_TYPE_I64
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_CONSTANT_INTEGER:
            return mmay_mir_type_is_integer(target_type)
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_CONSTANT_F64:
            return target_type == MMAY_TYPE_F64
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_CONSTANT_BOOL:
            return target_type == MMAY_TYPE_BOOL
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_CONSTANT_STRING:
            if (mmay_mir_string_constant(
                    function,
                    instruction->string_constant
                ) == NULL) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            return target_type == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_COPY:
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == first_type
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_ADD_PROVEN_I64:
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_I64
                    && first_type == MMAY_TYPE_I64
                    && second_type == MMAY_TYPE_I64
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_ADD_CHECKED_I64:
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
        case MMAY_MIR_DIVIDE_CHECKED_I64:
        case MMAY_MIR_REMAINDER_CHECKED_I64:
        case MMAY_MIR_BITWISE_AND_I64:
        case MMAY_MIR_BITWISE_XOR_I64:
        case MMAY_MIR_BITWISE_OR_I64:
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64:
        case MMAY_MIR_SHIFT_RIGHT_I64:
        case MMAY_MIR_ADD_F64:
        case MMAY_MIR_SUBTRACT_F64:
        case MMAY_MIR_MULTIPLY_F64:
        case MMAY_MIR_DIVIDE_F64:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (instruction->kind == MMAY_MIR_ADD_F64
                || instruction->kind == MMAY_MIR_SUBTRACT_F64
                || instruction->kind == MMAY_MIR_MULTIPLY_F64
                || instruction->kind == MMAY_MIR_DIVIDE_F64) {
                return target_type == MMAY_TYPE_F64
                        && first_type == MMAY_TYPE_F64
                        && second_type == MMAY_TYPE_F64
                    ? MMAY_MIR_VERIFY_OK
                    : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return mmay_mir_type_is_integer(target_type)
                    && target_type == first_type
                    && target_type == second_type
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_NEGATE_CHECKED_I64:
        case MMAY_MIR_BITWISE_NOT_I64:
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return mmay_mir_type_is_integer(target_type)
                    && target_type == first_type
                    && (instruction->kind == MMAY_MIR_BITWISE_NOT_I64
                        || mmay_mir_type_is_signed_integer(target_type))
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_NEGATE_F64:
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_F64 && first_type == MMAY_TYPE_F64
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_NOT_BOOL:
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && first_type == MMAY_TYPE_BOOL
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_EQUAL_I64:
        case MMAY_MIR_LESS_I64:
        case MMAY_MIR_LESS_EQUAL_I64:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && mmay_mir_type_is_integer(first_type)
                    && first_type == second_type
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_EQUAL_F64:
        case MMAY_MIR_LESS_F64:
        case MMAY_MIR_LESS_EQUAL_F64:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && first_type == MMAY_TYPE_F64
                    && second_type == MMAY_TYPE_F64
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_EQUAL_BOOL:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && first_type == MMAY_TYPE_BOOL
                    && second_type == MMAY_TYPE_BOOL
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_EQUAL_STRING:
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && first_type == MMAY_TYPE_STRING
                    && second_type == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_RECORD_IDENTITY_SAME: {
            const MarimayMirRecord *record = mmay_mir_record(
                function,
                instruction->record
            );
            if (record == NULL) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == MMAY_TYPE_INVALID
                || second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == MMAY_TYPE_BOOL
                    && first_type == record->type
                    && second_type == record->type
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        case MMAY_MIR_RECORD_NEW: {
            const MarimayMirRecordConstruction *construction =
                mmay_mir_record_construction(
                    function,
                    instruction->record_construction
                );
            const MarimayMirRecord *record = construction != NULL
                ? mmay_mir_record(function, construction->record)
                : NULL;
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || record == NULL
                || instruction->record != construction->record
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->record_arguments.length
                || construction->argument_count != record->field_count) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != record->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const MarimayMirRecordArgument *argument =
                    mmay_mir_record_argument(
                        function,
                        construction->argument_start + index
                    );
                const MarimayMirRecordField *field = mmay_mir_record_field(
                    function,
                    record,
                    index
                );
                MarimayTypeId actual = argument != NULL
                    ? mmay_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : MMAY_TYPE_INVALID;
                if (actual == MMAY_TYPE_INVALID) {
                    return MMAY_MIR_VERIFY_INVALID_REGISTER;
                }
                if (field == NULL || actual != argument->type
                    || argument->type != field->type) {
                    return MMAY_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return MMAY_MIR_VERIFY_OK;
        }
        case MMAY_MIR_RECORD_GET:
        case MMAY_MIR_RECORD_SET: {
            const MarimayMirRecord *record = mmay_mir_record(
                function,
                instruction->record
            );
            const MarimayMirRecordField *field = mmay_mir_record_field(
                function,
                record,
                instruction->field
            );
            if (record == NULL || field == NULL) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == MMAY_TYPE_INVALID
                || (instruction->kind == MMAY_MIR_RECORD_SET
                    && second_type == MMAY_TYPE_INVALID)) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != record->type || target_type != field->type
                || (instruction->kind == MMAY_MIR_RECORD_SET
                    && (second_type != field->type || !field->is_mutable))) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return MMAY_MIR_VERIFY_OK;
        }
        case MMAY_MIR_COLLECTION_NEW:
        case MMAY_MIR_COLLECTION_LENGTH:
        case MMAY_MIR_COLLECTION_CONTAINS:
        case MMAY_MIR_COLLECTION_GET:
        case MMAY_MIR_COLLECTION_PUT:
        case MMAY_MIR_COLLECTION_ADD:
        case MMAY_MIR_COLLECTION_REMOVE:
        case MMAY_MIR_COLLECTION_CLEAR:
        case MMAY_MIR_COLLECTION_GENERATION:
        case MMAY_MIR_COLLECTION_REQUIRE_GENERATION:
        case MMAY_MIR_COLLECTION_ENTRY_KEY:
        case MMAY_MIR_COLLECTION_ENTRY_VALUE: {
            const MarimayMirCollection *collection = mmay_mir_collection(
                function, instruction->collection
            );
            if (collection == NULL) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_NEW) {
                return target_type == collection->type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != collection->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_LENGTH) {
                return target_type == MMAY_TYPE_I64
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_CLEAR) {
                return target_type == collection->type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_GENERATION) {
                return target_type == MMAY_TYPE_U64
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (second_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_REQUIRE_GENERATION) {
                return second_type == MMAY_TYPE_U64
                        && target_type == collection->type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_ENTRY_KEY
                || instruction->kind == MMAY_MIR_COLLECTION_ENTRY_VALUE) {
                const MarimayTypeId entry_type = instruction->kind
                        == MMAY_MIR_COLLECTION_ENTRY_KEY
                    ? collection->key_type : collection->value_type;
                return second_type == MMAY_TYPE_I64
                        && entry_type != MMAY_TYPE_INVALID
                        && target_type == entry_type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (second_type != collection->key_type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_CONTAINS
                || instruction->kind == MMAY_MIR_COLLECTION_REMOVE) {
                return target_type == MMAY_TYPE_BOOL
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_ADD) {
                return collection->is_set && target_type == collection->type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (collection->is_set) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_COLLECTION_GET) {
                return target_type == collection->value_type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return third_type != MMAY_TYPE_INVALID
                    && third_type == collection->value_type
                    && target_type == collection->type
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        case MMAY_MIR_ARRAY_NEW: {
            const MarimayMirArrayConstruction *construction =
                mmay_mir_array_construction(
                    function,
                    instruction->array_construction
                );
            const MarimayMirArray *array = construction != NULL
                ? mmay_mir_array(function, construction->array)
                : NULL;
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || array == NULL
                || instruction->array != construction->array
                || target_type != array->type
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->array_arguments.length) {
                return construction != NULL && array != NULL
                        && target_type != array->type
                    ? MMAY_MIR_VERIFY_TYPE_MISMATCH
                    : MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            for (index = construction->argument_start; index < argument_end;
                index += 1) {
                const MarimayMirArrayArgument *argument = mmay_mir_array_argument(
                    function,
                    index
                );
                MarimayTypeId actual = argument != NULL
                    ? mmay_mir_register_type(function, argument->register_id)
                    : MMAY_TYPE_INVALID;
                if (actual == MMAY_TYPE_INVALID) {
                    return MMAY_MIR_VERIFY_INVALID_REGISTER;
                }
                if (argument->type != array->element_type
                    || actual != argument->type) {
                    return MMAY_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return MMAY_MIR_VERIFY_OK;
        }
        case MMAY_MIR_ARRAY_PUSH: {
            const MarimayMirArray *array = mmay_mir_array(
                function, instruction->array
            );
            if (array == NULL || first_type != array->type
                || second_type != array->element_type
                || target_type != array->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return MMAY_MIR_VERIFY_OK;
        }
        case MMAY_MIR_ARRAY_POP:
        case MMAY_MIR_ARRAY_TRUNCATE:
        case MMAY_MIR_ARRAY_CLEAR:
        case MMAY_MIR_ARRAY_RESERVE:
        case MMAY_MIR_ARRAY_GENERATION:
        case MMAY_MIR_ARRAY_REQUIRE_GENERATION: {
            const MarimayMirArray *array = mmay_mir_array(
                function, instruction->array
            );
            if (array == NULL) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == MMAY_TYPE_INVALID
                || ((instruction->kind == MMAY_MIR_ARRAY_TRUNCATE
                        || instruction->kind == MMAY_MIR_ARRAY_RESERVE
                        || instruction->kind
                            == MMAY_MIR_ARRAY_REQUIRE_GENERATION)
                    && second_type == MMAY_TYPE_INVALID)) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != array->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_POP) {
                return target_type == array->element_type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_GENERATION) {
                return target_type == MMAY_TYPE_U64
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_REQUIRE_GENERATION) {
                return second_type == MMAY_TYPE_U64
                        && target_type == array->type
                    ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if ((instruction->kind == MMAY_MIR_ARRAY_TRUNCATE
                    || instruction->kind == MMAY_MIR_ARRAY_RESERVE)
                && second_type != MMAY_TYPE_I64) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == array->type
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        case MMAY_MIR_ARRAY_LENGTH:
        case MMAY_MIR_ARRAY_REQUIRE_INDEX:
        case MMAY_MIR_ARRAY_ASSUME_INDEX:
        case MMAY_MIR_ARRAY_GET:
        case MMAY_MIR_ARRAY_SET:
        case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
        case MMAY_MIR_ARRAY_SET_IN_BOUNDS: {
            const MarimayMirArray *array = mmay_mir_array(
                function,
                instruction->array
            );
            if (array == NULL) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == MMAY_TYPE_INVALID
                || (instruction->kind != MMAY_MIR_ARRAY_LENGTH
                    && second_type == MMAY_TYPE_INVALID)
                || ((instruction->kind == MMAY_MIR_ARRAY_SET
                        || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS)
                    && mmay_mir_register_type(
                        function,
                        instruction->third
                    ) == MMAY_TYPE_INVALID)) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != array->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_LENGTH) {
                return target_type == MMAY_TYPE_I64
                    ? MMAY_MIR_VERIFY_OK
                    : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                || instruction->kind == MMAY_MIR_ARRAY_ASSUME_INDEX) {
                return second_type == MMAY_TYPE_I64
                        && target_type == array->type
                    ? MMAY_MIR_VERIFY_OK
                    : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (second_type != MMAY_TYPE_I64
                || target_type != array->element_type
                || ((instruction->kind == MMAY_MIR_ARRAY_SET
                        || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS)
                    && mmay_mir_register_type(
                        function,
                        instruction->third
                    ) != array->element_type)) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return MMAY_MIR_VERIFY_OK;
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
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || enum_layout == NULL || variant == NULL
                || instruction->enum_id != construction->enum_id
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->enum_arguments.length
                || construction->argument_count != variant->payload_count) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != enum_layout->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const MarimayMirEnumArgument *argument = mmay_mir_enum_argument(
                    function,
                    construction->argument_start + index
                );
                const MarimayMirEnumPayload *payload = mmay_mir_enum_payload(
                    function,
                    variant,
                    index
                );
                MarimayTypeId actual = argument != NULL
                    ? mmay_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : MMAY_TYPE_INVALID;
                if (actual == MMAY_TYPE_INVALID) {
                    return MMAY_MIR_VERIFY_INVALID_REGISTER;
                }
                if (payload == NULL || actual != argument->type
                    || argument->type != payload->type) {
                    return MMAY_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return MMAY_MIR_VERIFY_OK;
        }
        case MMAY_MIR_ENUM_IS_VARIANT:
        case MMAY_MIR_ENUM_REQUIRE_VARIANT:
        case MMAY_MIR_ENUM_GET_PAYLOAD: {
            const MarimayMirEnum *enum_layout = mmay_mir_enum(
                function,
                instruction->enum_id
            );
            const MarimayMirEnumVariant *variant = mmay_mir_enum_variant(
                function,
                enum_layout,
                instruction->variant
            );
            const MarimayMirEnumPayload *payload =
                instruction->kind == MMAY_MIR_ENUM_GET_PAYLOAD
                    ? mmay_mir_enum_payload(
                        function,
                        variant,
                        instruction->field
                    )
                    : NULL;
            if (enum_layout == NULL || variant == NULL
                || (instruction->kind == MMAY_MIR_ENUM_GET_PAYLOAD
                    && payload == NULL)) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != enum_layout->type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ENUM_IS_VARIANT) {
                return target_type == MMAY_TYPE_BOOL
                    ? MMAY_MIR_VERIFY_OK
                    : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == MMAY_MIR_ENUM_REQUIRE_VARIANT) {
                return target_type == enum_layout->type
                    ? MMAY_MIR_VERIFY_OK
                    : MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == payload->type
                ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        case MMAY_MIR_CALL:
        case MMAY_MIR_DEFER_REGISTER:
        case MMAY_MIR_DEFER_CALL: {
            const MarimayMirCall *call = mmay_mir_call(
                function,
                instruction->call
            );
            size_t argument_end = 0;
            size_t index = 0;
            bool deferred = instruction->kind != MMAY_MIR_CALL;
            if (call == NULL || call->result_type == MMAY_TYPE_NEVER
                || (!deferred && call->interface_slot != UINT32_MAX)
                || !mmay_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if ((deferred && (target_type != MMAY_TYPE_BOOL
                    || call->result_type != MMAY_TYPE_VOID))
                || (!deferred && target_type != call->result_type)) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (deferred && call->interface_slot != UINT32_MAX) {
                const MarimayMirCallArgument *receiver = call->argument_count != 0
                    ? mmay_mir_call_argument(function, call->argument_start)
                    : NULL;
                const MarimayMirManagedLayoutInfo *receiver_layout =
                    receiver != NULL && managed_types != NULL
                    ? mmay_map_get_const(
                        managed_types, &receiver->type,
                        sizeof(receiver->type)
                    )
                    : NULL;
                const MarimayMirInterface *interface_layout =
                    receiver_layout != NULL
                        && receiver_layout->kind
                            == MMAY_MIR_MANAGED_LAYOUT_INTERFACE
                        && receiver_layout->layout < function->interfaces.length
                    ? mmay_vector_at_const(
                        &function->interfaces, receiver_layout->layout
                    )
                    : NULL;
                if (interface_layout == NULL
                    || call->interface_slot >= interface_layout->method_count) {
                    return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                }
            }
            for (index = call->argument_start; index < argument_end;
                index += 1) {
                const MarimayMirCallArgument *argument = mmay_mir_call_argument(
                    function,
                    index
                );
                MarimayTypeId actual = argument != NULL
                    ? mmay_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : MMAY_TYPE_INVALID;
                if (actual == MMAY_TYPE_INVALID) {
                    return MMAY_MIR_VERIFY_INVALID_REGISTER;
                }
                if (actual != argument->type) {
                    return MMAY_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return MMAY_MIR_VERIFY_OK;
        }
        case MMAY_MIR_INTERFACE_NEW: {
            const MarimayMirInterfaceConstruction *construction =
                instruction->interface_construction
                    < function->interface_constructions.length
                ? mmay_vector_at_const(
                    &function->interface_constructions,
                    instruction->interface_construction
                ) : NULL;
            const MarimayMirInterface *layout = construction != NULL
                ? mmay_vector_at_const(
                    &function->interfaces, construction->interface_id
                ) : NULL;
            const MarimayMirManagedLayoutInfo *payload_layout =
                construction != NULL && managed_types != NULL
                ? mmay_map_get_const(
                    managed_types,
                    &construction->payload_type,
                    sizeof(construction->payload_type)
                )
                : NULL;
            MarimayTypeId payload_type = construction != NULL
                ? mmay_mir_register_type(function, construction->payload)
                : MMAY_TYPE_INVALID;
            if (construction == NULL || layout == NULL
                || construction->method_count != layout->method_count
                || payload_layout == NULL
                || (payload_layout->kind != MMAY_MIR_MANAGED_LAYOUT_RECORD
                    && payload_layout->kind != MMAY_MIR_MANAGED_LAYOUT_ENUM)) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (payload_type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_REGISTER;
            }
            if (payload_type != construction->payload_type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == layout->type ? MMAY_MIR_VERIFY_OK
                : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        case MMAY_MIR_CALL_INTERFACE: {
            const MarimayMirCall *call = mmay_mir_call(function, instruction->call);
            size_t argument_end = 0;
            size_t argument_index = 0;
            const MarimayMirCallArgument *receiver = call != NULL
                    && call->argument_count != 0
                ? mmay_mir_call_argument(function, call->argument_start)
                : NULL;
            const MarimayMirManagedLayoutInfo *receiver_layout =
                receiver != NULL && managed_types != NULL
                ? mmay_map_get_const(
                    managed_types,
                    &receiver->type,
                    sizeof(receiver->type)
                )
                : NULL;
            const MarimayMirInterface *interface_layout =
                receiver_layout != NULL
                    && receiver_layout->kind
                        == MMAY_MIR_MANAGED_LAYOUT_INTERFACE
                    && receiver_layout->layout < function->interfaces.length
                ? mmay_vector_at_const(
                    &function->interfaces,
                    receiver_layout->layout
                )
                : NULL;
            if (call == NULL || call->interface_slot == UINT32_MAX
                || call->argument_count == 0
                || !mmay_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                )
                || argument_end > function->call_arguments.length
                || interface_layout == NULL
                || call->interface_slot >= interface_layout->method_count) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != call->result_type) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (argument_index = call->argument_start;
                argument_index < argument_end;
                argument_index += 1) {
                const MarimayMirCallArgument *argument = mmay_mir_call_argument(
                    function,
                    argument_index
                );
                MarimayTypeId actual = argument != NULL
                    ? mmay_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : MMAY_TYPE_INVALID;
                if (actual == MMAY_TYPE_INVALID) {
                    return MMAY_MIR_VERIFY_INVALID_REGISTER;
                }
                if (actual != argument->type) {
                    return MMAY_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return MMAY_MIR_VERIFY_OK;
        }
    }
    return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
}
bool mmay_mir_instruction_uses_first(MarimayMirInstructionKind kind) {
    return kind != MMAY_MIR_CONSTANT_I64
        && kind != MMAY_MIR_CONSTANT_INTEGER
        && kind != MMAY_MIR_CONSTANT_F64
        && kind != MMAY_MIR_CONSTANT_BOOL
        && kind != MMAY_MIR_CONSTANT_STRING && kind != MMAY_MIR_RECORD_NEW
        && kind != MMAY_MIR_ENUM_NEW && kind != MMAY_MIR_ARRAY_NEW
        && kind != MMAY_MIR_COLLECTION_NEW
        && kind != MMAY_MIR_INTERFACE_NEW && kind != MMAY_MIR_CALL
        && kind != MMAY_MIR_DEFER_REGISTER
        && kind != MMAY_MIR_DEFER_CALL
        && kind != MMAY_MIR_IO_FLUSH
        && kind != MMAY_MIR_PROCESS_CONTEXT
        && kind != MMAY_MIR_CALL_INTERFACE;
}
bool mmay_mir_instruction_uses_second(MarimayMirInstructionKind kind) {
    switch (kind) {
        case MMAY_MIR_ADD_CHECKED_I64:
        case MMAY_MIR_ADD_PROVEN_I64:
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
        case MMAY_MIR_DIVIDE_CHECKED_I64:
        case MMAY_MIR_REMAINDER_CHECKED_I64:
        case MMAY_MIR_BITWISE_AND_I64:
        case MMAY_MIR_BITWISE_XOR_I64:
        case MMAY_MIR_BITWISE_OR_I64:
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64:
        case MMAY_MIR_SHIFT_RIGHT_I64:
        case MMAY_MIR_ADD_F64:
        case MMAY_MIR_SUBTRACT_F64:
        case MMAY_MIR_MULTIPLY_F64:
        case MMAY_MIR_DIVIDE_F64:
        case MMAY_MIR_EQUAL_I64:
        case MMAY_MIR_EQUAL_F64:
        case MMAY_MIR_EQUAL_BOOL:
        case MMAY_MIR_EQUAL_STRING:
        case MMAY_MIR_LESS_I64:
        case MMAY_MIR_LESS_EQUAL_I64:
        case MMAY_MIR_LESS_F64:
        case MMAY_MIR_LESS_EQUAL_F64:
        case MMAY_MIR_RECORD_SET:
        case MMAY_MIR_RECORD_IDENTITY_SAME:
        case MMAY_MIR_ARRAY_GET:
        case MMAY_MIR_ARRAY_SET:
        case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
        case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
        case MMAY_MIR_ARRAY_REQUIRE_INDEX:
        case MMAY_MIR_ARRAY_ASSUME_INDEX:
        case MMAY_MIR_ARRAY_PUSH:
        case MMAY_MIR_ARRAY_TRUNCATE:
        case MMAY_MIR_ARRAY_RESERVE:
        case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
        case MMAY_MIR_COLLECTION_CONTAINS:
        case MMAY_MIR_COLLECTION_GET:
        case MMAY_MIR_COLLECTION_PUT:
        case MMAY_MIR_COLLECTION_ADD:
        case MMAY_MIR_COLLECTION_REMOVE:
        case MMAY_MIR_COLLECTION_REQUIRE_GENERATION:
        case MMAY_MIR_COLLECTION_ENTRY_KEY:
        case MMAY_MIR_COLLECTION_ENTRY_VALUE:
        case MMAY_MIR_INTEGER_POLICY:
        case MMAY_MIR_TEST_ASSERT:
            return true;
        case MMAY_MIR_CONSTANT_I64:
        case MMAY_MIR_CONSTANT_INTEGER:
        case MMAY_MIR_CONSTANT_F64:
        case MMAY_MIR_CONSTANT_BOOL:
        case MMAY_MIR_CONSTANT_STRING:
        case MMAY_MIR_COPY:
        case MMAY_MIR_NEGATE_CHECKED_I64:
        case MMAY_MIR_NEGATE_F64:
        case MMAY_MIR_NOT_BOOL:
        case MMAY_MIR_BITWISE_NOT_I64:
        case MMAY_MIR_RECORD_NEW:
        case MMAY_MIR_RECORD_GET:
        case MMAY_MIR_ARRAY_NEW:
        case MMAY_MIR_ARRAY_LENGTH:
        case MMAY_MIR_ARRAY_POP:
        case MMAY_MIR_ARRAY_CLEAR:
        case MMAY_MIR_ARRAY_GENERATION:
        case MMAY_MIR_COLLECTION_NEW:
        case MMAY_MIR_COLLECTION_LENGTH:
        case MMAY_MIR_COLLECTION_CLEAR:
        case MMAY_MIR_COLLECTION_GENERATION:
        case MMAY_MIR_ENUM_NEW:
        case MMAY_MIR_ENUM_IS_VARIANT:
        case MMAY_MIR_ENUM_REQUIRE_VARIANT:
        case MMAY_MIR_ENUM_GET_PAYLOAD:
        case MMAY_MIR_CALL:
        case MMAY_MIR_INTERFACE_NEW:
        case MMAY_MIR_CALL_INTERFACE:
        case MMAY_MIR_INTEGER_CONVERT:
        case MMAY_MIR_FORMAT_F32:
        case MMAY_MIR_FORMAT_F64:
        case MMAY_MIR_FORMAT_BOOL:
        case MMAY_MIR_FORMAT_INTEGER:
        case MMAY_MIR_IO_PRINTLN:
        case MMAY_MIR_IO_WRITE:
        case MMAY_MIR_IO_FLUSH:
        case MMAY_MIR_IO_READ_LINE:
        case MMAY_MIR_IO_READ_BYTES:
        case MMAY_MIR_IO_WRITE_BYTES:
        case MMAY_MIR_TEST_OUTPUT_BEGIN:
        case MMAY_MIR_TEST_OUTPUT_END:
        case MMAY_MIR_DEFER_REGISTER:
        case MMAY_MIR_DEFER_CALL:
            return false;
        case MMAY_MIR_STRING_CONCAT:
        case MMAY_MIR_PARSE_INTEGER:
        case MMAY_MIR_TEXT_BINARY:
        case MMAY_MIR_FS_BINARY:
            return true;
        case MMAY_MIR_TEXT_UNARY:
        case MMAY_MIR_PATH:
        case MMAY_MIR_FS_UNARY:
        case MMAY_MIR_ENV_READ:
            return false;
        case MMAY_MIR_PROCESS_CONTEXT:
            return false;
    }
    return false;
}
bool mmay_mir_instruction_uses_third(MarimayMirInstructionKind kind) {
    return kind == MMAY_MIR_ARRAY_SET
        || kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
        || kind == MMAY_MIR_COLLECTION_PUT;
}
bool mmay_mir_assigned_register(
    const unsigned char *assigned,
    size_t register_count,
    MarimayMirRegister register_id
) {
    return register_id != MMAY_MIR_NO_REGISTER
        && register_id < register_count && assigned[register_id] != 0;
}
void mmay_mir_transfer_assignments(
    const MarimayMirBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t index = 0;
    for (index = 0; index < block->instructions.length; index += 1) {
        const MarimayMirInstruction *instruction = mmay_vector_at_const(
            &block->instructions,
            index
        );
        if (instruction != NULL && instruction->target < register_count) {
            assigned[instruction->target] = 1;
        }
    }
}
const uint64_t mmay_mir_checked_index_top = UINT64_MAX;
const uint64_t mmay_mir_checked_index_none = UINT64_MAX - 1;
void mmay_mir_transfer_checked_index_instruction(
    const MarimayMirInstruction *instruction,
    uint64_t *checked,
    size_t register_count
) {
    size_t register_index = 0;
    if (instruction == NULL || checked == NULL
        || instruction->target >= register_count) {
        return;
    }
    if (instruction->kind == MMAY_MIR_ARRAY_POP
        || instruction->kind == MMAY_MIR_ARRAY_TRUNCATE
        || instruction->kind == MMAY_MIR_ARRAY_CLEAR) {
        for (register_index = 0; register_index < register_count;
            register_index += 1) {
            checked[register_index] = mmay_mir_checked_index_none;
        }
        return;
    }
    for (register_index = 0; register_index < register_count;
        register_index += 1) {
        if (checked[register_index] == instruction->target) {
            checked[register_index] = mmay_mir_checked_index_none;
        }
    }
    checked[instruction->target] = instruction->kind
            == MMAY_MIR_ARRAY_REQUIRE_INDEX
        ? instruction->second
        : mmay_mir_checked_index_none;
}
void mmay_mir_transfer_checked_indices(
    const MarimayMirBlock *block,
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
        mmay_mir_transfer_checked_index_instruction(
            mmay_vector_at_const(&block->instructions, instruction_index),
            checked,
            register_count
        );
    }
}
uint64_t mmay_mir_meet_checked_index(uint64_t left, uint64_t right) {
    if (left == mmay_mir_checked_index_top) {
        return right;
    }
    if (right == mmay_mir_checked_index_top || left == right) {
        return left;
    }
    return mmay_mir_checked_index_none;
}
bool mmay_mir_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (!mmay_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}
