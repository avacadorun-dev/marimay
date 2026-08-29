#include "checker_derive_api.h"

#include "checker_core_api.h"

bool mmay_checker_private_collect_eq_hash_derive(
    MarimayChecker *checker,
    const MarimayAstNode *declaration,
    bool *derives_eq_hash,
    bool *valid
) {
    size_t child_index = 0;
    bool saw_derive = false;
    bool saw_eq = false;
    bool saw_hash = false;
    if (checker == NULL || declaration == NULL || derives_eq_hash == NULL
        || valid == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *derives_eq_hash = false;
    for (child_index = 0; child_index < declaration->child_count;
        child_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            declaration, child_index
        );
        const MarimayAstNode *attribute = element != NULL
                && element->kind == MMAY_AST_ELEMENT_NODE
            ? element->as.node : NULL;
        const MarimayAstNode *arguments = NULL;
        size_t argument_index = 0;
        if (attribute == NULL || attribute->kind != MMAY_SYNTAX_ATTRIBUTE) {
            continue;
        }
        if (!mmay_checker_private_check_attribute_is(
                checker, attribute, "derive"
            ) || saw_derive) {
            mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
            return false;
        }
        saw_derive = true;
        arguments = mmay_checker_private_check_child_node(
            attribute, MMAY_SYNTAX_ARGUMENTS, 0
        );
        if (arguments == NULL) {
            *valid = false;
            continue;
        }
        for (argument_index = 0; argument_index < arguments->child_count;
            argument_index += 1) {
            const MarimayAstElement *argument = mmay_ast_child(
                arguments, argument_index
            );
            const MarimayAstNode *expression = argument != NULL
                    && argument->kind == MMAY_AST_ELEMENT_NODE
                ? argument->as.node : NULL;
            const MarimayAstNode *name = expression != NULL
                ? mmay_checker_private_check_child_node(
                    expression, MMAY_SYNTAX_NAME_EXPRESSION, 0
                ) : NULL;
            const MarimaySyntaxToken *token = name != NULL
                ? mmay_checker_private_check_child_token(
                    name, MMAY_TOKEN_IDENTIFIER, 0
                ) : NULL;
            if (token == NULL || token->synthetic) continue;
            if (mmay_checker_private_check_text_equal(
                    checker, token->start, token->end, "Eq"
                ) && !saw_eq) {
                saw_eq = true;
            } else if (mmay_checker_private_check_text_equal(
                    checker, token->start, token->end, "Hash"
                ) && !saw_hash) {
                saw_hash = true;
            } else {
                mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
                return false;
            }
        }
    }
    if (saw_derive && (!saw_eq || !saw_hash)) {
        *valid = false;
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
            "derive requires the exact Eq and Hash pair",
            declaration->start,
            declaration->end
        );
    }
    *derives_eq_hash = saw_derive && saw_eq && saw_hash;
    return !mmay_checker_has_failed(checker);
}
