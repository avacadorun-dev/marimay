#include "checker_collection_types_api.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"

static void mmay_checker_collection_type_intern_failed(
    MarimayChecker *checker
) {
    const size_t max_types = checker->options.type_options.max_types;
    const size_t dynamic_types = checker->result->types.descriptors.length;
    mmay_checker_fail(
        checker,
        max_types <= MMAY_TYPE_FIRST_DYNAMIC
                || dynamic_types >= max_types - MMAY_TYPE_FIRST_DYNAMIC
            ? MMAY_CHECK_LIMIT_REACHED
            : MMAY_CHECK_ALLOCATION_FAILED
    );
}

bool mmay_checker_private_resolve_collection_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result,
    bool *resolved
) {
    const MarimaySyntaxToken *name = NULL;
    const MarimayAstNode *type_arguments = NULL;
    MarimayVector arguments;
    bool is_map = false;
    bool accepted = false;
    const size_t *expected = NULL;
    static const size_t map_count = 2;
    static const size_t set_count = 1;
    memset(&arguments, 0, sizeof(arguments));
    if (checker == NULL || node == NULL || result == NULL || resolved == NULL
        || node->kind != MMAY_SYNTAX_TYPE) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    name = mmay_checker_private_check_child_token(
        node, MMAY_TOKEN_IDENTIFIER, 0
    );
    type_arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
    );
    if (name == NULL || name->synthetic || type_arguments == NULL) {
        return true;
    }
    if (mmay_checker_private_check_text_equal(
            checker, name->start, name->end, "Map"
        )) {
        is_map = true;
        expected = &map_count;
    } else if (mmay_checker_private_check_text_equal(
            checker, name->start, name->end, "Set"
        )) {
        expected = &set_count;
    } else {
        return true;
    }
    *resolved = true;
    if (!mmay_vector_init(
            &arguments, checker->result->allocator, sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (!mmay_checker_private_parse_type_arguments(
            checker, type_arguments, &arguments
        )) {
        goto cleanup;
    }
    if (arguments.length != *expected) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            is_map
                ? "Map requires exactly two type arguments"
                : "Set requires exactly one type argument",
            node->start,
            node->end
        );
        goto cleanup;
    }
    accepted = is_map
        ? mmay_type_intern_map(
            &checker->result->types,
            *(const MarimayTypeId *)mmay_vector_at_const(&arguments, 0),
            *(const MarimayTypeId *)mmay_vector_at_const(&arguments, 1),
            result
        )
        : mmay_type_intern_set(
            &checker->result->types,
            *(const MarimayTypeId *)mmay_vector_at_const(&arguments, 0),
            result
        );
    if (!accepted) {
        mmay_checker_collection_type_intern_failed(checker);
    }

cleanup:
    mmay_vector_dispose(&arguments);
    if (!accepted) {
        *result = MMAY_TYPE_INVALID;
    }
    return accepted && !mmay_checker_has_failed(checker);
}
