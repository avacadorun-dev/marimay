#include "checker_tuple_match_api.h"

#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "checker_expr_tuples_api.h"
#include "checker_lookup_api.h"
#include "checker_match_binding_api.h"
#include "checker_match_lower_api.h"
#include "checker_match_pattern_api.h"
#include "checker_statement_api.h"

typedef struct MarimayCheckTupleMatchArm {
    const MarimayAstNode *pattern;
    const MarimayAstNode *guard;
    const MarimayAstNode *body;
    MarimayVector bindings;
    bool body_is_block;
} MarimayCheckTupleMatchArm;

static bool mmay_checker_tuple_collect_bindings(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayTypeId type,
    MarimayVector *bindings
) {
    const MarimayTypeDescriptor *descriptor = NULL;
    const MarimayAstNode *list = NULL;
    MarimayCheckName name;
    size_t index = 0;
    if (pattern == NULL) {
        return false;
    }
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        return mmay_checker_private_match_remainder_name(
                function,
                pattern,
                &has_binding,
                &name
            ) && (!has_binding
                || mmay_checker_private_enum_match_add_binding(
                    function,
                    bindings,
                    name,
                    type
                ));
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        return mmay_checker_private_enum_match_name(
                function,
                pattern,
                &name
            ) && mmay_checker_private_enum_match_add_binding(
                function,
                bindings,
                name,
                type
            );
    }
    descriptor = mmay_type_descriptor(
        &function->checker->result->types,
        type
    );
    list = mmay_checker_private_check_child_node(
        pattern,
        MMAY_SYNTAX_PATTERN_LIST,
        0
    );
    if (pattern->kind != MMAY_SYNTAX_TUPLE_PATTERN
        || descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_TUPLE
        || list == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "tuple patterns accept tuple, name, and wildcard subpatterns",
            pattern->start,
            pattern->end
        );
        return false;
    }
    for (;;) {
        const MarimayAstNode *child = mmay_checker_private_check_child_node_any(
            list,
            index
        );
        if (child == NULL) {
            break;
        }
        if (index >= descriptor->argument_count
            || !mmay_checker_tuple_collect_bindings(
                function,
                child,
                descriptor->arguments[index],
                bindings
            )) {
            return false;
        }
        index += 1;
    }
    if (index != descriptor->argument_count) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "tuple pattern arity does not match the scrutinee type",
            pattern->start,
            pattern->end
        );
        return false;
    }
    return true;
}

static bool mmay_checker_tuple_assign_bindings(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayTypeId type,
    MarimayMirRegister source
) {
    const MarimayTypeDescriptor *descriptor = NULL;
    const MarimayAstNode *list = NULL;
    MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
    MarimayCheckName name;
    size_t index = 0;
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        return mmay_checker_private_match_remainder_name(
                function,
                pattern,
                &has_binding,
                &name
            ) && (!has_binding
                || mmay_checker_private_assign_match_local(
                    function,
                    name,
                    type,
                    source,
                    pattern->start,
                    pattern->end
                ));
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        return mmay_checker_private_enum_match_name(
                function,
                pattern,
                &name
            ) && mmay_checker_private_assign_match_local(
                function,
                name,
                type,
                source,
                pattern->start,
                pattern->end
            );
    }
    descriptor = mmay_type_descriptor(
        &function->checker->result->types,
        type
    );
    list = mmay_checker_private_check_child_node(
        pattern,
        MMAY_SYNTAX_PATTERN_LIST,
        0
    );
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_TUPLE
        || list == NULL || !mmay_checker_private_function_tuple_layout(
            function,
            type,
            &layout
        )) {
        return false;
    }
    for (;;) {
        const MarimayAstNode *child = mmay_checker_private_check_child_node_any(
            list,
            index
        );
        MarimayExpressionValue projected;
        if (child == NULL) {
            break;
        }
        if (index >= descriptor->argument_count) {
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!mmay_checker_private_function_emit_record_access(
                function,
                MMAY_MIR_RECORD_GET,
                descriptor->arguments[index],
                source,
                MMAY_MIR_NO_REGISTER,
                layout,
                index,
                child->start,
                child->end,
                &projected
            ) || !mmay_checker_tuple_assign_bindings(
                function,
                child,
                descriptor->arguments[index],
                projected.register_id
            )) {
            return false;
        }
        index += 1;
    }
    return index == descriptor->argument_count;
}

bool mmay_checker_private_compile_tuple_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayExpressionValue *result
) {
    MarimayCheckTupleMatchArm *arms = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    while (mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        arm_count += 1;
    }
    if (arm_count == 0 || !mmay_size_multiply(
            arm_count,
            sizeof(*arms),
            &arm_bytes
        )) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
            "tuple match requires an unguarded structural or catch-all arm",
            node->start,
            node->end
        );
        goto cleanup;
    }
    arms = mmay_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const MarimayAstNode *arm_node = mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_index
        );
        const MarimayAstNode *wrapper = mmay_checker_private_check_child_node(
            arm_node,
            MMAY_SYNTAX_PATTERN,
            0
        );
        arms[arm_index].pattern = mmay_checker_private_check_child_node_any(
            wrapper,
            0
        );
        if (covered) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
                "tuple match arm is unreachable after an unguarded structural arm",
                arm_node->start,
                arm_node->end
            );
            goto cleanup;
        }
        if (!mmay_vector_init(
                &arms[arm_index].bindings,
                function->checker->result->allocator,
                sizeof(MarimayCheckEnumMatchBinding)
            ) || !mmay_checker_private_match_arm_body_parts(
                function,
                arm_node,
                &arms[arm_index].guard,
                &arms[arm_index].body,
                &arms[arm_index].body_is_block
            ) || !mmay_checker_tuple_collect_bindings(
                function,
                arms[arm_index].pattern,
                scrutinee.type,
                &arms[arm_index].bindings
            )) {
            goto cleanup;
        }
        covered = arms[arm_index].guard == NULL;
    }
    if (!covered) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
            "tuple match needs an unguarded final arm",
            node->start,
            node->end
        );
        goto cleanup;
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        MarimayCheckTupleMatchArm *arm = &arms[arm_index];
        MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId next_block = MMAY_MIR_NO_BLOCK;
        MarimayExpressionValue guard;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        bool guard_never = false;
        bool arm_compiled = false;
        memset(&guard, 0, sizeof(guard));
        function->scope_depth += 1;
        if (!mmay_checker_private_declare_match_bindings(
                function,
                &arm->bindings
            ) || !mmay_checker_tuple_assign_bindings(
                function,
                arm->pattern,
                scrutinee.type,
                scrutinee.register_id
            )) {
            goto arm_cleanup;
        }
        if (arm->guard != NULL) {
            if (!mmay_checker_private_function_add_block(
                    function,
                    &body_block
                ) || !mmay_checker_private_function_add_block(
                    function,
                    &next_block
                ) || !mmay_checker_private_compile_match_guard(
                    function,
                    arm->guard,
                    &guard,
                    &guard_never
                )) {
                goto arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto arm_cleanup;
            }
            if (!mmay_checker_private_function_set_branch(
                    function,
                    function->block,
                    guard.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                goto arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!mmay_checker_private_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto arm_cleanup;
        }
        arm_compiled = true;

arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (arm->guard != NULL) {
            function->block = next_block;
            function->terminated = false;
        }
    }
    if (target == MMAY_MIR_NO_REGISTER) {
        mmay_checker_private_expression_set_never(function, result);
    } else if (join == MMAY_MIR_NO_BLOCK
        || match_type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    } else {
        function->block = join;
        function->terminated = false;
        result->type = match_type;
        result->register_id = target;
        result->valid = true;
    }
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            if (arms[arm_index].bindings.element_size != 0) {
                mmay_vector_dispose(&arms[arm_index].bindings);
            }
        }
    }
    mmay_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}
