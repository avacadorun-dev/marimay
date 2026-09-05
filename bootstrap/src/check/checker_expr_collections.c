#include "checker_expr_collections_api.h"

#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

typedef enum MarimayCollectionOperation {
    MMAY_COLLECTION_LENGTH,
    MMAY_COLLECTION_CONTAINS_KEY,
    MMAY_COLLECTION_GET,
    MMAY_COLLECTION_PUT,
    MMAY_COLLECTION_REMOVE,
    MMAY_COLLECTION_CLEAR,
    MMAY_COLLECTION_CONTAINS,
    MMAY_COLLECTION_ADD
} MarimayCollectionOperation;

static bool mmay_collection_type_contains_parameter(
    const MarimayTypeTable *types,
    MarimayTypeId type
) {
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(types, type);
    size_t index = 0;
    if (descriptor == NULL) return false;
    if (descriptor->kind == MMAY_TYPE_KIND_TYPE_PARAMETER) return true;
    for (index = 0; index < descriptor->argument_count; index += 1) {
        if (mmay_collection_type_contains_parameter(
                types, descriptor->arguments[index]
            )) return true;
    }
    return false;
}

static bool mmay_collection_generic_type_node(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    MarimayTypeId type = MMAY_TYPE_INVALID;
    const MarimayTypeDescriptor *descriptor = NULL;
    MarimayTypeId key = MMAY_TYPE_INVALID;
    if (!mmay_checker_private_check_exact_type(checker, node, &type)) {
        return !mmay_checker_has_failed(checker);
    }
    descriptor = mmay_type_descriptor(&checker->result->types, type);
    if (descriptor == NULL || (descriptor->kind != MMAY_TYPE_KIND_MAP
            && descriptor->kind != MMAY_TYPE_KIND_SET)
        || descriptor->argument_count == 0) return true;
    key = descriptor->arguments[0];
    if (!mmay_collection_type_contains_parameter(
            &checker->result->types, key
        )) return true;
    (void)mmay_checker_add_diagnostic(
        checker,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Map and Set keys cannot contain an unconstrained generic type parameter",
        node->start,
        node->end
    );
    return false;
}

bool mmay_checker_private_validate_generic_collection_keys(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    size_t index = 0;
    if (checker == NULL || node == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (node->kind == MMAY_SYNTAX_TYPE
        && !mmay_collection_generic_type_node(checker, node)) return false;
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && !mmay_checker_private_validate_generic_collection_keys(
                checker, element->as.node
            )) return false;
    }
    return !mmay_checker_has_failed(checker);
}

static const MarimayAstNode *mmay_collection_child(
    const MarimayAstNode *node,
    MarimaySyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found++ == ordinal) return element->as.node;
        }
    }
    return NULL;
}

static size_t mmay_collection_arguments(
    const MarimayAstNode *arguments,
    const MarimayAstNode **nodes,
    size_t capacity
) {
    size_t index = 0;
    size_t count = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (count < capacity) nodes[count] = element->as.node;
            count += 1;
        }
    }
    return count;
}

static bool mmay_collection_operation(
    const MarimayChecker *checker,
    const MarimaySyntaxToken *name,
    MarimayCollectionOperation *operation
) {
    static const char *const names[] = {
        "length", "containsKey", "get", "put", "remove", "clear",
        "contains", "add"
    };
    size_t index = 0;
    if (checker == NULL || name == NULL || name->synthetic
        || operation == NULL) return false;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        if (mmay_checker_private_check_text_equal(
                checker, name->start, name->end, names[index]
            )) {
            *operation = (MarimayCollectionOperation)index;
            return true;
        }
    }
    return false;
}

static bool mmay_collection_type_is_visiting(
    const MarimayVector *visiting,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; visiting != NULL && index < visiting->length; index += 1) {
        const MarimayTypeId *item = mmay_vector_at_const(visiting, index);
        if (item != NULL && *item == type) return true;
    }
    return false;
}

static bool mmay_collection_key_eligible(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayVector *visiting
) {
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
        &function->checker->result->types, type
    );
    MarimayCheckRecordDeclaration *record = NULL;
    MarimayCheckEnumDeclaration *enum_value = NULL;
    const MarimayCheckedRecord *imported_record = NULL;
    const MarimayCheckedEnum *imported_enum = NULL;
    size_t index = 0;
    bool accepted = false;
    if (type == MMAY_TYPE_BOOL || (type >= MMAY_TYPE_I8
            && type <= MMAY_TYPE_U64) || type == MMAY_TYPE_RUNE
        || type == MMAY_TYPE_STRING) return true;
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL) {
        return false;
    }
    if (mmay_collection_type_is_visiting(visiting, type)) return true;
    if (!mmay_vector_push(visiting, &type)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    record = mmay_checker_private_checker_find_record_by_type(
        function->checker, type
    );
    enum_value = record == NULL
        ? mmay_checker_private_checker_find_enum_by_type(
            function->checker, type
        ) : NULL;
    imported_record = record == NULL && enum_value == NULL
        ? mmay_check_imported_record_by_type(function->checker, type) : NULL;
    imported_enum = record == NULL && enum_value == NULL
            && imported_record == NULL
        ? mmay_check_imported_enum_by_type(function->checker, type) : NULL;
    if (record != NULL && record->derives_eq_hash) {
        MarimayCheckRecordDeclaration concrete = *record;
        MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
        const MarimayMirRecord *mir_record = NULL;
        concrete.type = type;
        if (!mmay_checker_private_function_record_layout(
                function, &concrete, &layout
            )) goto cleanup;
        mir_record = mmay_vector_at_const(&function->mir.records, layout);
        accepted = mir_record != NULL;
        for (index = 0; accepted && index < mir_record->field_count; index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &function->mir.record_fields, mir_record->field_start + index
            );
            accepted = field != NULL && !field->is_mutable
                && mmay_collection_key_eligible(
                    function, field->type, visiting
                );
        }
    } else if (imported_record != NULL && imported_record->derives_eq_hash) {
        MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
        const MarimayMirRecord *mir_record = NULL;
        if (!mmay_checker_private_imported_record_layout(
                function, imported_record, type, &layout
            )) goto cleanup;
        mir_record = mmay_vector_at_const(&function->mir.records, layout);
        accepted = mir_record != NULL;
        for (index = 0; accepted && index < mir_record->field_count; index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &function->mir.record_fields, mir_record->field_start + index
            );
            accepted = field != NULL && !field->is_mutable
                && mmay_collection_key_eligible(
                    function, field->type, visiting
                );
        }
    } else if (enum_value != NULL && enum_value->derives_eq_hash) {
        MarimayCheckEnumDeclaration concrete = *enum_value;
        MarimayMirEnumId layout = MMAY_MIR_NO_ENUM;
        const MarimayMirEnum *mir_enum = NULL;
        concrete.type = type;
        if (!mmay_checker_private_function_enum_layout(
                function, &concrete, &layout
            )) goto cleanup;
        mir_enum = mmay_vector_at_const(&function->mir.enums, layout);
        accepted = mir_enum != NULL;
        for (index = 0; accepted && index < mir_enum->payload_count; index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &function->mir.enum_payloads, mir_enum->payload_start + index
            );
            accepted = payload != NULL && mmay_collection_key_eligible(
                function, payload->type, visiting
            );
        }
    } else if (imported_enum != NULL && imported_enum->derives_eq_hash) {
        MarimayMirEnumId layout = MMAY_MIR_NO_ENUM;
        const MarimayMirEnum *mir_enum = NULL;
        if (!mmay_checker_private_imported_enum_layout(
                function, imported_enum, type, &layout
            )) goto cleanup;
        mir_enum = mmay_vector_at_const(&function->mir.enums, layout);
        accepted = mir_enum != NULL;
        for (index = 0; accepted && index < mir_enum->payload_count; index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &function->mir.enum_payloads, mir_enum->payload_start + index
            );
            accepted = payload != NULL && mmay_collection_key_eligible(
                function, payload->type, visiting
            );
        }
    }

cleanup:
    visiting->length -= 1;
    return accepted;
}

static bool mmay_collection_validate_key(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    size_t start,
    size_t end
) {
    MarimayVector visiting;
    bool accepted = false;
    memset(&visiting, 0, sizeof(visiting));
    if (!mmay_vector_init(
            &visiting,
            function->checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    accepted = mmay_collection_key_eligible(function, type, &visiting);
    mmay_vector_dispose(&visiting);
    if (!accepted && !mmay_checker_has_failed(function->checker)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "Map and Set keys require bool, integer, rune, string, or an immutable nominal type with @derive(Eq, Hash)",
            start,
            end
        );
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_checker_private_collection_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    size_t start,
    size_t end,
    MarimayMirCollectionId *layout,
    MarimayTypeId *key,
    MarimayTypeId *value,
    bool *is_set
) {
    MarimayTypeId local_key = MMAY_TYPE_INVALID;
    MarimayTypeId local_value = MMAY_TYPE_INVALID;
    bool local_set = false;
    if (function == NULL || layout == NULL) return false;
    if (mmay_type_map_parts(
            &function->checker->result->types,
            type,
            &local_key,
            &local_value
        )) {
        local_set = false;
    } else if (mmay_type_set_element(
            &function->checker->result->types, type, &local_key
        )) {
        local_set = true;
    } else {
        return false;
    }
    if (!mmay_collection_validate_key(function, local_key, start, end)) {
        return false;
    }
    if (!mmay_mir_add_collection(
            &function->mir,
            type,
            local_key,
            local_set ? MMAY_TYPE_INVALID : local_value,
            local_set,
            layout
        )) {
        if (!mmay_checker_has_failed(function->checker)
            && *layout == MMAY_MIR_NO_COLLECTION) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
        return false;
    }
    if (key != NULL) *key = local_key;
    if (value != NULL) *value = local_value;
    if (is_set != NULL) *is_set = local_set;
    return true;
}

static MarimayCheckEnumDeclaration *mmay_collection_prelude_enum(
    MarimayFunctionChecker *function,
    const char *name
) {
    MarimaySymbolId symbol;
    size_t index = 0;
    if (!mmay_symbol_id_from_bytes(
            "std", 3, "prelude", 7, name, strlen(name), 0, &symbol
        )) return NULL;
    for (index = 0; index < function->checker->prelude_enums.length;
        index += 1) {
        MarimayCheckEnumDeclaration *candidate = mmay_vector_at(
            &function->checker->prelude_enums, index
        );
        if (candidate != NULL
            && mmay_symbol_id_equal(candidate->symbol, symbol)) return candidate;
    }
    return NULL;
}

static bool mmay_collection_generic_type(
    MarimayFunctionChecker *function,
    const char *name,
    MarimayTypeId first,
    MarimayTypeId second,
    MarimayTypeId *result
) {
    MarimayCheckEnumDeclaration *base = mmay_collection_prelude_enum(
        function, name
    );
    MarimayVector arguments;
    bool accepted = false;
    bool initialized = false;
    memset(&arguments, 0, sizeof(arguments));
    if (base == NULL || !mmay_vector_init(
            &arguments,
            function->checker->result->allocator,
            sizeof(MarimayTypeId)
        )) goto cleanup;
    initialized = true;
    if (!mmay_vector_push(&arguments, &first)
        || (second != MMAY_TYPE_INVALID
            && !mmay_vector_push(&arguments, &second))
        || !mmay_checker_private_intern_generic_nominal(
            function->checker,
            MMAY_TYPE_KIND_NOMINAL,
            base->symbol,
            &arguments,
            result
        )) goto cleanup;
    accepted = true;
cleanup:
    if (initialized) mmay_vector_dispose(&arguments);
    if (!accepted && !mmay_checker_has_failed(function->checker)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
    }
    return accepted;
}

static bool mmay_collection_emit_raw(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId target_type,
    MarimayMirRegister first,
    MarimayMirRegister second,
    MarimayMirRegister third,
    MarimayMirCollectionId layout,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    if (!mmay_function_add_register(function, target_type, &instruction.target)) {
        return false;
    }
    instruction.kind = kind;
    instruction.first = first;
    instruction.second = second;
    instruction.third = third;
    instruction.collection = layout;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = target_type;
    result->register_id = instruction.target;
    result->valid = true;
    return true;
}

static bool mmay_collection_enum_layout(
    MarimayFunctionChecker *function,
    const char *base_name,
    MarimayTypeId concrete_type,
    MarimayMirEnumId *layout
) {
    MarimayCheckEnumDeclaration *base = mmay_collection_prelude_enum(
        function, base_name
    );
    MarimayCheckEnumDeclaration concrete;
    if (base == NULL) return false;
    concrete = *base;
    concrete.type = concrete_type;
    return mmay_checker_private_function_enum_layout(
        function, &concrete, layout
    );
}

static bool mmay_collection_emit_result_operation(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId target_type,
    MarimayTypeId error_type,
    MarimayMirRegister collection_register,
    MarimayMirRegister key_register,
    MarimayMirRegister value_register,
    MarimayMirCollectionId collection_layout,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    if (!mmay_collection_enum_layout(
            function,
            error_type == MMAY_TYPE_INVALID ? "Option" : "Result",
            target_type,
            &instruction.enum_id
        ) || (error_type != MMAY_TYPE_INVALID
            && !mmay_collection_enum_layout(
                function, "CollectionGrowthError", error_type,
                &instruction.option_enum_id
            )) || !mmay_function_add_register(
                function, target_type, &instruction.target
            )) return false;
    instruction.kind = kind;
    instruction.first = collection_register;
    instruction.second = key_register;
    instruction.third = value_register;
    instruction.collection = collection_layout;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = target_type;
    result->register_id = instruction.target;
    result->valid = true;
    return true;
}

bool mmay_checker_private_compile_collection_constructor(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = mmay_collection_child(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    const MarimayAstNode *arguments = mmay_collection_child(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimayAstNode *type_arguments = mmay_collection_child(
        node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
    );
    const MarimaySyntaxToken *name = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    MarimayCheckName checked_name;
    MarimayVector types;
    MarimayTypeId collection_type = MMAY_TYPE_INVALID;
    MarimayTypeId key = MMAY_TYPE_INVALID;
    MarimayTypeId value = MMAY_TYPE_INVALID;
    MarimayMirCollectionId layout = MMAY_MIR_NO_COLLECTION;
    const MarimayTypeDescriptor *expected_descriptor = NULL;
    bool is_map = false;
    bool accepted = false;
    const MarimayAstNode *ignored[1] = {NULL};
    memset(&types, 0, sizeof(types));
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (name == NULL || name->synthetic) return true;
    if (mmay_checker_private_check_text_equal(
            function->checker, name->start, name->end, "mapOf"
        )) {
        is_map = true;
    } else if (!mmay_checker_private_check_text_equal(
            function->checker, name->start, name->end, "setOf"
        )) {
        return true;
    }
    checked_name.snapshot = function->checker->ast->snapshot;
    checked_name.start = name->start;
    checked_name.end = name->end;
    if (mmay_checker_private_function_find_local(function, checked_name) != NULL
        || mmay_checker_private_checker_find_top_level(
            function->checker, checked_name
        ) != NULL) return true;
    *resolved = true;
    if (arguments == NULL || mmay_collection_arguments(
            arguments, ignored, 1
        ) != 0) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            is_map ? "mapOf requires no value arguments"
                : "setOf requires no value arguments",
            node->start,
            node->end
        );
        return false;
    }
    if (function->constant_evaluation) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "Map and Set values are not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    if (!mmay_vector_init(
            &types,
            function->checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (type_arguments != NULL) {
        if (!mmay_checker_private_parse_type_arguments(
                function->checker, type_arguments, &types
            )) goto cleanup;
    } else if (expected != MMAY_TYPE_INVALID) {
        size_t index = 0;
        expected_descriptor = mmay_type_descriptor(
            &function->checker->result->types, expected
        );
        if (expected_descriptor != NULL
            && expected_descriptor->kind == (is_map
                ? MMAY_TYPE_KIND_MAP : MMAY_TYPE_KIND_SET)) {
            for (index = 0; index < expected_descriptor->argument_count;
                index += 1) {
                MarimayTypeId item = expected_descriptor->arguments[index];
                if (!mmay_vector_push(&types, &item)) {
                    mmay_checker_fail(
                        function->checker, MMAY_CHECK_ALLOCATION_FAILED
                    );
                    goto cleanup;
                }
            }
        }
    }
    if (types.length != (is_map ? 2U : 1U)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            is_map
                ? "mapOf requires Map key and value type arguments"
                : "setOf requires one Set element type argument",
            node->start,
            node->end
        );
        goto cleanup;
    }
    key = *(const MarimayTypeId *)mmay_vector_at_const(&types, 0);
    if (is_map) value = *(const MarimayTypeId *)mmay_vector_at_const(&types, 1);
    if (!(is_map ? mmay_type_intern_map(
                &function->checker->result->types, key, value, &collection_type
            ) : mmay_type_intern_set(
                &function->checker->result->types, key, &collection_type
            ))) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!mmay_checker_private_collection_layout(
            function,
            collection_type,
            node->start,
            node->end,
            &layout,
            NULL,
            NULL,
            NULL
        ) || !mmay_collection_emit_raw(
            function,
            MMAY_MIR_COLLECTION_NEW,
            collection_type,
            MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER,
            layout,
            node->start,
            node->end,
            result
        )) goto cleanup;
    accepted = true;
cleanup:
    mmay_vector_dispose(&types);
    return accepted && !mmay_checker_has_failed(function->checker);
}

static bool mmay_collection_check_arity(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arguments,
    bool dot,
    MarimayCollectionOperation operation,
    size_t count
) {
    size_t expected = dot ? 0 : 1;
    if (operation == MMAY_COLLECTION_CONTAINS_KEY
        || operation == MMAY_COLLECTION_GET
        || operation == MMAY_COLLECTION_REMOVE
        || operation == MMAY_COLLECTION_CONTAINS
        || operation == MMAY_COLLECTION_ADD) expected += 1;
    if (operation == MMAY_COLLECTION_PUT) expected += 2;
    if (count == expected) return true;
    (void)mmay_checker_add_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "Map or Set operation has an invalid argument count",
        arguments != NULL ? arguments->start : 0,
        arguments != NULL ? arguments->end : 0
    );
    return false;
}

static bool mmay_collection_compile_argument(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    if (!mmay_compile_expression(function, node, expected, result)) return false;
    if (mmay_expression_is_never(function, result)) return true;
    if (result->type != expected) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "Map and Set operation arguments require exact types",
            node->start,
            node->end
        );
        return false;
    }
    return true;
}

static bool mmay_collection_option_result(
    MarimayFunctionChecker *function,
    MarimayExpressionValue receiver,
    MarimayExpressionValue key_value,
    MarimayTypeId value_type,
    MarimayMirCollectionId layout,
    bool remove,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayTypeId option_type = MMAY_TYPE_INVALID;
    return mmay_collection_generic_type(
            function, "Option", value_type, MMAY_TYPE_INVALID, &option_type
        ) && mmay_collection_emit_result_operation(
            function,
            remove ? MMAY_MIR_COLLECTION_REMOVE_OPTION
                : MMAY_MIR_COLLECTION_GET_OPTION,
            option_type,
            MMAY_TYPE_INVALID,
            receiver.register_id,
            key_value.register_id,
            MMAY_MIR_NO_REGISTER,
            layout,
            start,
            end,
            result
        );
}

static bool mmay_collection_update_result(
    MarimayFunctionChecker *function,
    MarimayExpressionValue receiver,
    MarimayExpressionValue key_value,
    const MarimayExpressionValue *map_value,
    MarimayMirCollectionId layout,
    bool is_set,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayCheckEnumDeclaration *error = mmay_collection_prelude_enum(
        function, "CollectionGrowthError"
    );
    MarimayTypeId result_type = MMAY_TYPE_INVALID;
    return error != NULL && mmay_collection_generic_type(
            function, "Result", MMAY_TYPE_BOOL, error->type, &result_type
        ) && mmay_collection_emit_result_operation(
            function,
            is_set ? MMAY_MIR_COLLECTION_ADD_RESULT
                : MMAY_MIR_COLLECTION_PUT_RESULT,
            result_type,
            error->type,
            receiver.register_id,
            key_value.register_id,
            is_set ? MMAY_MIR_NO_REGISTER : map_value->register_id,
            layout,
            start,
            end,
            result
        );
}

bool mmay_checker_private_compile_collection_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *receiver_node,
    const MarimayExpressionValue *receiver,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = mmay_collection_child(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    const MarimayAstNode *member = mmay_collection_child(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const MarimayAstNode *arguments = mmay_collection_child(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimaySyntaxToken *name = NULL;
    const MarimayAstNode *member_receiver = NULL;
    const MarimayAstNode *nodes[3] = {NULL, NULL, NULL};
    const MarimayAstNode *key_node = NULL;
    const MarimayAstNode *value_node = NULL;
    MarimayCollectionOperation operation;
    MarimayTypeId key_type = MMAY_TYPE_INVALID;
    MarimayTypeId value_type = MMAY_TYPE_INVALID;
    MarimayMirCollectionId layout = MMAY_MIR_NO_COLLECTION;
    MarimayExpressionValue key_value, map_value, raw;
    size_t count = 0;
    size_t offset = 0;
    bool dot = false;
    bool is_set = false;
    if (function == NULL || node == NULL || receiver == NULL
        || resolved == NULL || result == NULL) return false;
    *resolved = false;
    if (callee != NULL) {
        name = mmay_checker_private_check_child_token(
            callee, MMAY_TOKEN_IDENTIFIER, 0
        );
    } else if (member != NULL && mmay_checker_private_member_parts(
            member, &member_receiver, &name
        )) {
        dot = true;
    }
    if (!mmay_collection_operation(function->checker, name, &operation)) {
        return true;
    }
    if (member_receiver != NULL && member_receiver != receiver_node) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (mmay_collection_child(node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        *resolved = true;
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "Map and Set operations do not accept type arguments",
            node->start,
            node->end
        );
        return false;
    }
    count = mmay_collection_arguments(arguments, nodes, 3);
    if (!mmay_collection_check_arity(
            function, arguments, dot, operation, count
        )) {
        *resolved = true;
        return false;
    }
    if (!mmay_checker_private_collection_layout(
            function,
            receiver->type,
            receiver_node->start,
            receiver_node->end,
            &layout,
            &key_type,
            &value_type,
            &is_set
        )) return true;
    *resolved = true;
    if ((is_set && (operation == MMAY_COLLECTION_CONTAINS_KEY
            || operation == MMAY_COLLECTION_GET
            || operation == MMAY_COLLECTION_PUT))
        || (!is_set && (operation == MMAY_COLLECTION_CONTAINS
            || operation == MMAY_COLLECTION_ADD))) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            is_set ? "operation requires a Map receiver"
                : "operation requires a Set receiver",
            name->start,
            name->end
        );
        return false;
    }
    if (function->constant_evaluation) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "Map and Set operations are not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    offset = dot ? 0 : 1;
    key_node = count > offset ? nodes[offset] : NULL;
    value_node = count > offset + 1 ? nodes[offset + 1] : NULL;
    memset(&key_value, 0, sizeof(key_value));
    memset(&map_value, 0, sizeof(map_value));
    if (key_node != NULL && !mmay_collection_compile_argument(
            function, key_node, key_type, &key_value
        )) return false;
    if (key_node != NULL && mmay_expression_is_never(function, &key_value)) {
        *result = key_value;
        return true;
    }
    if (value_node != NULL && !mmay_collection_compile_argument(
            function, value_node, value_type, &map_value
        )) return false;
    if (value_node != NULL && mmay_expression_is_never(function, &map_value)) {
        *result = map_value;
        return true;
    }
    if (operation == MMAY_COLLECTION_LENGTH) {
        return mmay_collection_emit_raw(
            function, MMAY_MIR_COLLECTION_LENGTH, MMAY_TYPE_I64,
            receiver->register_id, MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER, layout, node->start, node->end, result
        );
    }
    if (operation == MMAY_COLLECTION_CLEAR) {
        if (!mmay_collection_emit_raw(
                function, MMAY_MIR_COLLECTION_CLEAR, receiver->type,
                receiver->register_id, MMAY_MIR_NO_REGISTER,
                MMAY_MIR_NO_REGISTER, layout, node->start, node->end, &raw
            )) return false;
        result->type = MMAY_TYPE_VOID;
        result->register_id = MMAY_MIR_NO_REGISTER;
        result->valid = true;
        return true;
    }
    if (operation == MMAY_COLLECTION_CONTAINS_KEY
        || operation == MMAY_COLLECTION_CONTAINS) {
        return mmay_collection_emit_raw(
            function, MMAY_MIR_COLLECTION_CONTAINS, MMAY_TYPE_BOOL,
            receiver->register_id, key_value.register_id,
            MMAY_MIR_NO_REGISTER, layout, node->start, node->end, result
        );
    }
    if (operation == MMAY_COLLECTION_GET) {
        return mmay_collection_option_result(
            function, *receiver, key_value, value_type, layout, false,
            node->start, node->end, result
        );
    }
    if (operation == MMAY_COLLECTION_REMOVE && !is_set) {
        return mmay_collection_option_result(
            function, *receiver, key_value, value_type, layout, true,
            node->start, node->end, result
        );
    }
    if (operation == MMAY_COLLECTION_REMOVE) {
        return mmay_collection_emit_raw(
            function, MMAY_MIR_COLLECTION_REMOVE, MMAY_TYPE_BOOL,
            receiver->register_id, key_value.register_id,
            MMAY_MIR_NO_REGISTER, layout, node->start, node->end, result
        );
    }
    return mmay_collection_update_result(
        function, *receiver, key_value,
        is_set ? NULL : &map_value, layout, is_set,
        node->start, node->end, result
    );
}
