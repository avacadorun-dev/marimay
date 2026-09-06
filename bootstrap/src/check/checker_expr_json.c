#include "checker_json_internal.h"
#include "checker_intrinsics_api.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"

static bool mmay_json_select(MarimayJsonExpansion *json,
    const MarimayAstNode *node, const MarimayIntrinsicSignature **intrinsic
) {
    MarimayChecker *checker = json->function->checker;
    const MarimayAstNode *callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0);
    const MarimayAstNode *member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0);
    const MarimaySyntaxToken *token = NULL, *member_token = NULL;
    const MarimayAstNode *receiver = NULL;
    MarimayCheckName binding_name;
    bool namespaced = member != NULL;
    size_t index = 0;
    if (namespaced) {
        if (!mmay_checker_private_member_parts(member, &receiver, &member_token)
            || receiver == NULL || receiver->kind != MMAY_SYNTAX_NAME_EXPRESSION) return false;
        callee = receiver;
    }
    token = mmay_checker_private_check_child_token(callee, MMAY_TOKEN_IDENTIFIER, 0);
    if (token == NULL || token->synthetic || checker->imports == NULL) return false;
    binding_name = (MarimayCheckName){checker->ast->snapshot, token->start, token->end};
    if (mmay_checker_private_function_find_local(json->function, binding_name) != NULL
        || mmay_checker_private_checker_find_top_level(checker, binding_name) != NULL) return false;
    for (index = 0; index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding = &checker->imports->bindings[index];
        const MarimayCheckPackageSurface *surface;
        const unsigned char *name;
        size_t length;
        if (binding->ast != checker->ast || binding->is_namespace != namespaced
            || binding->local_length != token->end - token->start
            || memcmp(binding->local, checker->source + token->start, binding->local_length) != 0
            || binding->surface_index >= checker->imports->surface_count) continue;
        surface = checker->imports->surfaces[binding->surface_index];
        if (namespaced && (member_token == NULL || member_token->synthetic)) return false;
        name = namespaced ? checker->source + member_token->start : binding->original;
        length = namespaced ? member_token->end - member_token->start : binding->original_length;
        *intrinsic = mmay_json_intrinsic_find(surface->path, surface->path_length, name, length);
        if (*intrinsic == NULL) return false;
        json->surface = surface;
        return true;
    }
    return false;
}

static bool mmay_json_type_argument(MarimayJsonExpansion *json,
    const MarimayAstNode *node, const MarimayIntrinsicSignature *intrinsic, MarimayTypeId *type
) {
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0);
    MarimayVector types;
    bool accepted = false;
    *type = MMAY_TYPE_INVALID;
    if (arguments == NULL) return intrinsic->min_type_arguments == 0 || mmay_json_error(json,
        "json.decode requires one explicit type argument");
    if (!mmay_vector_init(&types, json->function->checker->result->allocator,
            sizeof(MarimayTypeId))) return mmay_json_failed(json);
    if (!mmay_checker_private_parse_type_arguments(json->function->checker, arguments, &types)) goto cleanup;
    if (types.length < intrinsic->min_type_arguments || types.length > intrinsic->max_type_arguments
        || types.length == 0) {
        (void)mmay_json_error(json, "JSON convenience accepts exactly one type argument");
        goto cleanup;
    }
    *type = *(const MarimayTypeId *)mmay_vector_at_const(&types, 0);
    accepted = mmay_checker_private_substitute_function_type(json->function, *type, type);
cleanup:
    mmay_vector_dispose(&types);
    return accepted;
}

bool mmay_checker_compile_json(MarimayFunctionChecker *function,
    const MarimayAstNode *node, bool *resolved, MarimayExpressionValue *result
) {
    MarimayJsonExpansion json;
    const MarimayAstNode *arguments;
    const MarimayAstNode *nodes[2] = {NULL, NULL};
    const MarimayCheckedFunction *new_decoder, *finish_encoder;
    const MarimayTypeDescriptor *descriptor;
    const MarimayIntrinsicSignature *intrinsic = NULL;
    MarimayTypeId type, bytes_type;
    MarimayExpressionValue values[2], limits, decoded, unused, output;
    size_t count = 0, index = 0;
    bool decode = false, explicit_limits = false;
    memset(&json, 0, sizeof(json));
    memset(values, 0, sizeof(values));
    json.function = function;
    json.span = (MarimayMirSpan){node->start, node->end};
    *resolved = mmay_json_select(&json, node, &intrinsic);
    if (!*resolved) return true;
    decode = intrinsic->kind == MMAY_INTRINSIC_JSON_DECODE
        || intrinsic->kind == MMAY_INTRINSIC_JSON_DECODE_WITH_LIMITS;
    explicit_limits = intrinsic->argument_count == 2;
    if (function->constant_evaluation) return mmay_json_error(&json,
        "JSON operations are not available in constant expressions");
    if (!mmay_json_type_argument(&json, node, intrinsic, &type)) return false;
    arguments = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_ARGUMENTS, 0);
    for (index = 0; arguments != NULL && index < arguments->child_count; index += 1) {
        const MarimayAstElement *item = mmay_ast_child(arguments, index);
        if (item != NULL && item->kind == MMAY_AST_ELEMENT_NODE) {
            if (count < 2) nodes[count] = item->as.node;
            count += 1;
        }
    }
    if (count != intrinsic->argument_count) return mmay_json_error(&json,
        "JSON convenience argument count does not match its signature");
    new_decoder = mmay_json_function(&json, "newDecoder", MMAY_TYPE_INVALID);
    finish_encoder = mmay_json_function(&json, "finishEncoder", MMAY_TYPE_INVALID);
    if (new_decoder == NULL || finish_encoder == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    descriptor = mmay_type_descriptor(&function->checker->result->types, finish_encoder->mir.result_type);
    if (descriptor == NULL || descriptor->argument_count != 2) return mmay_json_failed(&json);
    bytes_type = descriptor->arguments[0];
    json.error_type = descriptor->arguments[1];
    for (index = 0; index < count; index += 1) {
        MarimayTypeId expected = index == 1
            ? *(const MarimayTypeId *)mmay_vector_at_const(&new_decoder->mir.register_types, 1)
            : decode ? bytes_type : type;
        if (!mmay_compile_expression(function, nodes[index], expected, &values[index])) return false;
        if (mmay_expression_is_never(function, &values[index])) {
            *result = values[index];
            return true;
        }
        if (expected != MMAY_TYPE_INVALID && expected != values[index].type) return mmay_json_error(&json,
            "JSON argument does not match the required type");
    }
    if (!decode) type = values[0].type;
    if (!mmay_json_eligible(&json, type)
        || !mmay_json_prelude_type(&json, true, decode ? type : bytes_type, &json.result_type)
        || !mmay_function_add_register(function, json.result_type, &json.result.register_id)
        || !mmay_json_block(&json, &json.done)) return false;
    json.result.type = json.result_type;
    json.result.valid = true;
    if (explicit_limits) limits = values[1];
    else if (!mmay_json_call(&json, "defaultLimits", NULL, 0, &limits)) return false;
    if (decode) {
        values[1] = limits;
        if (!mmay_json_try_call(&json, "newDecoder", values, 2, &json.cursor)
            || !mmay_json_read(&json, type, &decoded)
            || !mmay_json_try_call(&json, "finishDecoder", &json.cursor, 1, &unused)
            || !mmay_json_finish(&json, false, decoded)) return false;
    } else {
        if (!mmay_json_try_call(&json, "newEncoder", &limits, 1, &json.cursor)
            || !mmay_json_write(&json, values[0])
            || !mmay_json_try_call(&json, "finishEncoder", &json.cursor, 1, &output)
            || !mmay_json_finish(&json, false, output)) return false;
    }
    mmay_json_enter(&json, json.done);
    *result = json.result;
    return mmay_checker_private_function_ensure_nominal_layouts(function);
}
