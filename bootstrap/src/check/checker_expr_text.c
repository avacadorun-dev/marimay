#include "checker_expr_text_api.h"

#include <string.h>

#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

typedef struct MarimayTextPrimitive {
    const char *name;
    MarimayMirTextOperation operation;
    size_t argument_count;
} MarimayTextPrimitive;

static bool mmay_text_checker_package(
    const MarimayChecker *checker, const char *name
) {
    size_t length = name != NULL ? strlen(name) : 0;
    return checker != NULL && name != NULL
        && checker->package_name_length == length
        && memcmp(checker->package_name, name, length) == 0;
}

static const MarimayTextPrimitive *mmay_text_checker_primitive(
    const MarimayChecker *checker,
    const MarimaySyntaxToken *token
) {
    static const MarimayTextPrimitive text[] = {
        {"mmayByteLength", MMAY_MIR_TEXT_BYTE_LENGTH, 1},
        {"mmayRuneCount", MMAY_MIR_TEXT_RUNE_COUNT, 1},
        {"mmayGraphemeCount", MMAY_MIR_TEXT_GRAPHEME_COUNT, 1},
        {"mmayIsRuneBoundary", MMAY_MIR_TEXT_IS_RUNE_BOUNDARY, 2},
        {"mmaySubstring", MMAY_MIR_TEXT_SUBSTRING, 3},
        {"mmayDropPrefix", MMAY_MIR_TEXT_DROP_PREFIX, 2},
        {"mmayTakePrefix", MMAY_MIR_TEXT_TAKE_PREFIX, 2},
        {"mmayContains", MMAY_MIR_TEXT_CONTAINS, 2},
        {"mmayStartsWith", MMAY_MIR_TEXT_STARTS_WITH, 2},
        {"mmayEndsWith", MMAY_MIR_TEXT_ENDS_WITH, 2},
        {"mmayTrimAscii", MMAY_MIR_TEXT_TRIM_ASCII, 1},
        {"mmaySplitCount", MMAY_MIR_TEXT_SPLIT_COUNT, 2},
        {"mmaySplit", MMAY_MIR_TEXT_SPLIT, 2},
        {"mmayJoin", MMAY_MIR_TEXT_JOIN, 2},
        {"mmayRunes", MMAY_MIR_TEXT_RUNES, 1},
        {"mmayGraphemes", MMAY_MIR_TEXT_GRAPHEMES, 1}
    };
    static const MarimayTextPrimitive bytes[] = {
        {"mmayEncodeUtf8", MMAY_MIR_TEXT_ENCODE_UTF8, 1},
        {"mmayInvalidUtf8Offset", MMAY_MIR_TEXT_INVALID_UTF8_OFFSET, 1},
        {"mmayDecodeUtf8", MMAY_MIR_TEXT_DECODE_UTF8, 1}
    };
    const MarimayTextPrimitive *items = NULL;
    size_t count = 0;
    size_t index = 0;
    if (checker == NULL || token == NULL || token->synthetic) return NULL;
    if (mmay_text_checker_package(checker, "std/text")) {
        items = text;
        count = sizeof(text) / sizeof(text[0]);
    } else if (mmay_text_checker_package(checker, "std/bytes")) {
        items = bytes;
        count = sizeof(bytes) / sizeof(bytes[0]);
    } else {
        return NULL;
    }
    for (index = 0; index < count; index += 1U) {
        if (mmay_checker_private_check_text_equal(
                checker, token->start, token->end, items[index].name
            )) return &items[index];
    }
    return NULL;
}

static size_t mmay_text_checker_arguments(
    const MarimayAstNode *arguments,
    const MarimayAstNode **nodes,
    size_t capacity
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1U) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (count < capacity) nodes[count] = element->as.node;
            count += 1U;
        }
    }
    return count;
}

static bool mmay_text_checker_array_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId result_type,
    const MarimayExpressionValue *arguments,
    size_t argument_count,
    MarimayMirArrayId *array
) {
    MarimayTypeId element = MMAY_TYPE_INVALID;
    size_t index = 0;
    *array = MMAY_MIR_NO_ARRAY;
    if (mmay_type_array_element(
            &function->checker->result->types, result_type, &element
        )) return mmay_array_layout(function, result_type, array);
    for (index = 0; index < argument_count; index += 1U) {
        if (mmay_type_array_element(
                &function->checker->result->types,
                arguments[index].type,
                &element
            )) return mmay_array_layout(function, arguments[index].type, array);
    }
    return true;
}

static bool mmay_text_checker_generation(
    MarimayFunctionChecker *function,
    const MarimaySyntaxToken *name,
    const MarimayAstNode *node,
    const MarimayCheckCallable *callable,
    const MarimayExpressionValue *values,
    size_t count,
    MarimayExpressionValue *result
) {
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    bool require = mmay_checker_private_check_text_equal(
        function->checker, name->start, name->end, "mmayRequireGeneration"
    );
    if ((!require && count != 1U) || (require && count != 2U)
        || !mmay_array_layout(function, values[0].type, &array)
        || !mmay_function_add_register(
            function, require ? values[0].type : callable->result_type, &target
        )) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = require
        ? MMAY_MIR_ARRAY_REQUIRE_GENERATION : MMAY_MIR_ARRAY_GENERATION;
    instruction.target = target;
    instruction.first = values[0].register_id;
    instruction.second = require
        ? values[1].register_id : MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = callable->result_type;
    result->register_id = require ? MMAY_MIR_NO_REGISTER : target;
    result->valid = true;
    return true;
}

bool mmay_checker_private_compile_text_primitive_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimaySyntaxToken *name = mmay_checker_private_check_child_token(
        callee, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimayAstNode *nodes[3] = {NULL, NULL, NULL};
    const MarimayTextPrimitive *primitive = NULL;
    MarimayCheckName check_name;
    MarimayCheckCallable callable;
    MarimayExpressionValue values[3];
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    size_t count = 0;
    size_t index = 0;
    bool generation = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    primitive = mmay_text_checker_primitive(function->checker, name);
    generation = mmay_text_checker_package(function->checker, "std/bytes")
        && name != NULL && !name->synthetic
        && (mmay_checker_private_check_text_equal(
                function->checker, name->start, name->end, "mmayGeneration"
            ) || mmay_checker_private_check_text_equal(
                function->checker, name->start, name->end,
                "mmayRequireGeneration"
            ));
    if (primitive == NULL && !generation) return true;
    *resolved = true;
    count = mmay_text_checker_arguments(arguments, nodes, 3);
    check_name = (MarimayCheckName){
        function->checker->ast->snapshot, name->start, name->end
    };
    memset(&callable, 0, sizeof(callable));
    if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || mmay_check_resolve_call(
            function->checker, check_name, false, MMAY_TYPE_INVALID, &callable
        ) != MMAY_CHECK_CALL_FOUND || !callable.is_external
        || callable.parameter_count != count
        || count != (generation
            ? (mmay_checker_private_check_text_equal(
                    function->checker, name->start, name->end,
                    "mmayRequireGeneration"
                ) ? 2U : 1U)
            : primitive->argument_count)) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "standard text primitive has an invalid argument list",
            node->start, node->end
        );
        return false;
    }
    if (expected != MMAY_TYPE_INVALID && expected != callable.result_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "standard text primitive result type does not match",
            node->start, node->end
        );
        return false;
    }
    memset(values, 0, sizeof(values));
    for (index = 0; index < count; index += 1U) {
        values[index].register_id = MMAY_MIR_NO_REGISTER;
        if (!mmay_compile_expression(
                function, nodes[index], callable.parameter_types[index],
                &values[index]
            )) return false;
        if (mmay_expression_is_never(function, &values[index])) {
            *result = values[index];
            return true;
        }
        if (values[index].type != callable.parameter_types[index]) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "standard text primitive argument type does not match",
                nodes[index]->start, nodes[index]->end
            );
            return false;
        }
    }
    if (generation) return mmay_text_checker_generation(
        function, name, node, &callable, values, count, result
    );
    if (!mmay_text_checker_array_layout(
            function, callable.result_type, values, count, &array
        ) || !mmay_function_add_register(
            function, callable.result_type, &target
        )) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = count == 3U ? MMAY_MIR_TEXT_TERNARY
        : count == 2U ? MMAY_MIR_TEXT_BINARY : MMAY_MIR_TEXT_UNARY;
    instruction.text_operation = primitive->operation;
    instruction.target = target;
    instruction.first = values[0].register_id;
    instruction.second = count >= 2U
        ? values[1].register_id : MMAY_MIR_NO_REGISTER;
    instruction.third = count == 3U
        ? values[2].register_id : MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) return false;
    result->type = callable.result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
