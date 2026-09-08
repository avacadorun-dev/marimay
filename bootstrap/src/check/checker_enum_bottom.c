#include "checker_enum_bottom_api.h"
#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_expr_string_api.h"
#include "checker_statement_api.h"

bool mmay_enum_variant_bottom(MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *declaration, MarimayTypeId type,
    const MarimayCheckEnumVariantDeclaration *variant, bool *bottom) {
    *bottom = false;
    for (size_t i = 0; i < variant->payload.length; i += 1) {
        const MarimayCheckEnumPayloadItemDeclaration *payload = mmay_vector_at_const(&variant->payload, i);
        MarimayTypeId concrete;
        if (function->enum_bottom_work >= function->checker->options.mir_options.max_verify_work) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
        }
        function->enum_bottom_work += 1;
        if (payload == NULL || !mmay_checker_private_substitute_nominal_member_type(
                function, type, &declaration->type_parameters, payload->type, &concrete)) return false;
        if (concrete == MMAY_TYPE_NEVER) { *bottom = true; return true; }
    }
    return true;
}

bool mmay_enum_bottom_coverage(MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *declaration, MarimayTypeId type,
    unsigned char *seen, bool *all_bottom) {
    *all_bottom = true;
    for (size_t i = 0; i < declaration->variants.length; i += 1) {
        bool bottom;
        if (!mmay_enum_variant_bottom(function, declaration, type,
                mmay_vector_at_const(&declaration->variants, i), &bottom)) return false;
        seen[i] = bottom ? 1 : 0;
        *all_bottom = *all_bottom && bottom;
    }
    return true;
}

bool mmay_enum_bottom_edge(MarimayFunctionChecker *function,
    size_t start, size_t end, MarimayExpressionValue *result) {
    static const unsigned char message[] = "unreachable enum variant with never payload";
    MarimayExpressionValue guard;
    if (!mmay_checker_private_emit_string_constant(function, message, sizeof(message) - 1,
            start, end, &guard)
        || !mmay_checker_private_function_set_panic(function, guard.register_id, start, end)) return false;
    mmay_checker_private_expression_set_never(function, result);
    return true;
}
