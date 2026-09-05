#include "checker_enum_pattern_names_api.h"

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

/* Owns enum pattern qualification; matching/lowering do not reinterpret it. */
static bool mmay_enum_pattern_error(
    MarimayChecker *checker,
    const char *code,
    const char *message,
    MarimayCheckName name
) {
    (void)mmay_checker_add_diagnostic(
        checker, code, message, name.start, name.end
    );
    return false;
}

bool mmay_checker_enum_pattern_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern_name,
    MarimayTypeId enum_type,
    MarimayCheckName *variant_name
) {
    const MarimaySyntaxToken *tokens[3];
    MarimayCheckName names[3];
    MarimayCheckName no_namespace = {0};
    MarimayCheckName type_name;
    MarimayCheckEnumDeclaration *local = NULL;
    MarimayCheckTypeAliasDeclaration *alias = NULL;
    const MarimayCheckedEnum *imported = NULL;
    MarimayTypeId alias_type = MMAY_TYPE_INVALID;
    MarimayChecker *checker = function == NULL ? NULL : function->checker;
    size_t count = 0;
    bool is_namespace = false;
    bool import_resolved = false;
    bool exists_private = false;
    bool matches = false;
    if (checker == NULL || pattern_name == NULL || variant_name == NULL
        || pattern_name->kind != MMAY_SYNTAX_NAME_PATTERN
        || enum_type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (count = 0; count < 3; count += 1) {
        tokens[count] = mmay_checker_private_check_child_token(
            pattern_name, MMAY_TOKEN_IDENTIFIER, count
        );
        if (tokens[count] == NULL) {
            break;
        }
        if (tokens[count]->synthetic) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        names[count] = (MarimayCheckName){
            checker->ast->snapshot, tokens[count]->start, tokens[count]->end
        };
    }
    if (count == 0) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (mmay_checker_private_check_child_token(
            pattern_name, MMAY_TOKEN_IDENTIFIER, 3
        ) != NULL) {
        return mmay_enum_pattern_error(
            checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "enum pattern name must be Variant, Enum.Variant or namespace.Enum.Variant",
            (MarimayCheckName){checker->ast->snapshot,
                pattern_name->start, pattern_name->end}
        );
    }
    *variant_name = names[count - 1];
    if (count == 1) {
        return true;
    }
    is_namespace = mmay_check_import_namespace_exists(
        checker, checker->source + names[0].start,
        names[0].end - names[0].start
    );
    if (count == 2 && is_namespace) {
        return mmay_check_import_namespace_owns_enum(checker, names[0], enum_type)
            || mmay_enum_pattern_error(
                checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "qualified enum pattern namespace does not expose the matched enum type",
                names[0]
            );
    }
    if (count == 3 && !is_namespace) {
        return mmay_enum_pattern_error(
            checker, MMAY_DIAGNOSTIC_UNKNOWN_IMPORT,
            "qualified enum pattern requires an imported package namespace",
            names[0]
        );
    }
    type_name = names[count - 2];
    if (count == 2) {
        if (mmay_checker_private_function_find_local(function, type_name) != NULL) {
            return mmay_enum_pattern_error(
                checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "enum pattern qualifier is shadowed by a local binding", type_name
            );
        }
        local = mmay_checker_private_checker_find_enum(checker, type_name);
        if (local != NULL) {
            matches = mmay_checker_private_type_matches_nominal(
                checker, enum_type, local->symbol, local->type_parameters.length
            );
        } else {
            alias = mmay_checker_private_checker_find_type_alias(checker, type_name);
            if (alias != NULL) {
                if (!mmay_checker_private_evaluate_type_alias(
                        checker, alias, type_name.start, type_name.end
                    )) {
                    return false;
                }
                matches = alias->type == enum_type;
            }
        }
    }
    if (local == NULL && alias == NULL) {
        imported = mmay_check_imported_enum_by_name(
            checker, count == 3 ? names[0] : no_namespace, count == 3,
            type_name, &import_resolved, &exists_private, &alias_type
        );
        if (imported == NULL) {
            return mmay_enum_pattern_error(
                checker, exists_private ? MMAY_DIAGNOSTIC_PRIVATE_IMPORT
                    : import_resolved ? MMAY_DIAGNOSTIC_UNKNOWN_IMPORT
                    : MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
                exists_private ? "enum pattern qualifier is package-private"
                    : "unknown enum pattern qualifier", type_name
            );
        }
        matches = alias_type != MMAY_TYPE_INVALID ? alias_type == enum_type
            : mmay_checker_private_type_matches_nominal(
                checker, enum_type, imported->symbol, imported->type_parameters.length
            );
    }
    return matches || mmay_enum_pattern_error(
        checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "enum pattern qualifier does not denote the matched enum type", type_name
    );
}
