#include "ruda/check/checker.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"
#include "ruda/base/f64.h"
#include "ruda/base/string_literal.h"

typedef struct RudaCheckName {
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
} RudaCheckName;

typedef struct RudaCheckLocal {
    RudaCheckName name;
    RudaTypeId type;
    RudaMirRegister register_id;
    size_t scope_depth;
    bool mutable_binding;
    bool initialized;
} RudaCheckLocal;

typedef struct RudaCheckFunctionDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    const RudaAstNode *block;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaVector parameter_names;
    RudaVector parameter_types;
    RudaSymbolId symbol;
    RudaTypeId result_type;
    RudaTypeId signature_type;
    bool name_initialized;
    bool parameter_names_initialized;
    bool parameter_types_initialized;
    bool signature_valid;
    bool is_public;
    bool is_test;
    bool is_fuzz;
} RudaCheckFunctionDeclaration;

typedef enum RudaCheckConstantState {
    RUDA_CHECK_CONSTANT_UNEVALUATED,
    RUDA_CHECK_CONSTANT_EVALUATING,
    RUDA_CHECK_CONSTANT_EVALUATED,
    RUDA_CHECK_CONSTANT_INVALID
} RudaCheckConstantState;

typedef struct RudaCheckConstantDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    const RudaAstNode *initializer;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaMirValue value;
    RudaCheckConstantState state;
    bool name_initialized;
    bool declaration_valid;
    bool is_public;
} RudaCheckConstantDeclaration;

typedef enum RudaCheckTypeAliasState {
    RUDA_CHECK_TYPE_ALIAS_UNRESOLVED,
    RUDA_CHECK_TYPE_ALIAS_RESOLVING,
    RUDA_CHECK_TYPE_ALIAS_RESOLVED,
    RUDA_CHECK_TYPE_ALIAS_INVALID
} RudaCheckTypeAliasState;

typedef struct RudaCheckTypeAliasDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    const RudaAstNode *target;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaCheckTypeAliasState state;
    bool name_initialized;
    bool declaration_valid;
    bool is_public;
} RudaCheckTypeAliasDeclaration;

typedef struct RudaCheckRecordFieldDeclaration {
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaTypeId type;
    bool name_initialized;
    bool is_public;
    bool is_mutable;
} RudaCheckRecordFieldDeclaration;

typedef struct RudaCheckRecordDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector fields;
    bool name_initialized;
    bool fields_initialized;
    bool is_public;
    bool declaration_valid;
    bool fields_resolved;
} RudaCheckRecordDeclaration;

typedef struct RudaCheckEnumPayloadItemDeclaration {
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaTypeId type;
    bool name_initialized;
    bool has_name;
} RudaCheckEnumPayloadItemDeclaration;

typedef struct RudaCheckEnumVariantDeclaration {
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaCheckedEnumVariantKind kind;
    RudaVector payload;
    bool name_initialized;
    bool payload_initialized;
} RudaCheckEnumVariantDeclaration;

typedef struct RudaCheckEnumDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector variants;
    bool name_initialized;
    bool variants_initialized;
    bool is_public;
    bool declaration_valid;
    bool variants_resolved;
} RudaCheckEnumDeclaration;

typedef enum RudaCheckTopLevelKind {
    RUDA_CHECK_TOP_LEVEL_FUNCTION,
    RUDA_CHECK_TOP_LEVEL_CONSTANT,
    RUDA_CHECK_TOP_LEVEL_TYPE_ALIAS,
    RUDA_CHECK_TOP_LEVEL_RECORD,
    RUDA_CHECK_TOP_LEVEL_ENUM
} RudaCheckTopLevelKind;

typedef struct RudaCheckTopLevelEntry {
    RudaCheckTopLevelKind kind;
    size_t index;
} RudaCheckTopLevelEntry;

typedef struct RudaChecker {
    const RudaAstTree *ast;
    const char *package_name;
    const unsigned char *source;
    size_t source_length;
    RudaCheckOptions options;
    RudaCheckResult *result;
    RudaVector declarations;
    RudaVector constants;
    RudaVector type_aliases;
    RudaVector records;
    RudaVector enums;
    RudaMap top_level_index;
    size_t constant_depth;
    size_t type_alias_depth;
    size_t remaining_constant_steps;
    RudaCheckStatus failure;
} RudaChecker;

typedef struct RudaLoopContext {
    RudaMirBlockId break_target;
    RudaMirBlockId continue_target;
} RudaLoopContext;

typedef struct RudaMatchYieldContext {
    RudaMirBlockId *join;
    RudaTypeId *match_type;
    RudaMirRegister *target;
    bool yielded;
} RudaMatchYieldContext;

typedef struct RudaFunctionChecker {
    RudaChecker *checker;
    RudaVector locals;
    RudaVector loops;
    RudaMirFunction mir;
    RudaMirBlockId block;
    RudaTypeId result_type;
    size_t diagnostic_start;
    size_t scope_depth;
    RudaMatchYieldContext *match_yield;
    bool mir_initialized;
    bool terminated;
    bool constant_evaluation;
} RudaFunctionChecker;

typedef struct RudaExpressionValue {
    RudaTypeId type;
    RudaMirRegister register_id;
    bool valid;
} RudaExpressionValue;

typedef struct RudaCheckEnumMatchBinding {
    RudaCheckName name;
    RudaTypeId type;
} RudaCheckEnumMatchBinding;

typedef struct RudaCheckEnumMatchAlternative {
    const RudaAstNode *pattern;
    RudaCheckEnumVariantDeclaration *variant;
    RudaCheckName catch_binding;
    size_t variant_index;
    bool is_catch_all;
    bool has_catch_binding;
} RudaCheckEnumMatchAlternative;

typedef struct RudaCheckEnumMatchArm {
    const RudaAstNode *node;
    const RudaAstNode *pattern;
    const RudaAstNode *guard;
    const RudaAstNode *body;
    RudaVector alternatives;
    RudaVector bindings;
    bool body_is_block;
} RudaCheckEnumMatchArm;

typedef struct RudaCheckScalarMatchAlternative {
    const RudaAstNode *pattern;
    RudaCheckName catch_binding;
    RudaMirStringId string_constant;
    int64_t integer;
    bool literal;
    bool is_catch_all;
    bool has_catch_binding;
} RudaCheckScalarMatchAlternative;

typedef struct RudaCheckScalarMatchArm {
    const RudaAstNode *node;
    const RudaAstNode *pattern;
    const RudaAstNode *guard;
    const RudaAstNode *body;
    RudaVector alternatives;
    RudaVector bindings;
    bool body_is_block;
} RudaCheckScalarMatchArm;

typedef struct RudaCheckRecordMatchAlternative {
    const RudaAstNode *pattern;
    RudaCheckName catch_binding;
    bool is_record_pattern;
    bool has_catch_binding;
} RudaCheckRecordMatchAlternative;

typedef struct RudaCheckRecordMatchArm {
    const RudaAstNode *node;
    const RudaAstNode *pattern;
    const RudaAstNode *guard;
    const RudaAstNode *body;
    RudaVector alternatives;
    RudaVector bindings;
    bool body_is_block;
} RudaCheckRecordMatchArm;

typedef struct RudaInitializationSnapshot {
    RudaAllocator allocator;
    unsigned char *states;
    size_t count;
    size_t allocation_size;
} RudaInitializationSnapshot;

static bool ruda_evaluate_constant(
    RudaChecker *checker,
    RudaCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

static bool ruda_evaluate_type_alias(
    RudaChecker *checker,
    RudaCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

static void ruda_checker_fail(RudaChecker *checker, RudaCheckStatus status) {
    if (checker->failure == RUDA_CHECK_OK) {
        checker->failure = status;
    }
}

static void ruda_expression_set_never(
    RudaFunctionChecker *function,
    RudaExpressionValue *result
) {
    result->type = RUDA_TYPE_NEVER;
    result->register_id = RUDA_MIR_NO_REGISTER;
    result->valid = true;
    function->terminated = true;
}

static bool ruda_expression_is_never(
    const RudaFunctionChecker *function,
    const RudaExpressionValue *value
) {
    return function->terminated && value != NULL && value->valid
        && value->type == RUDA_TYPE_NEVER
        && value->register_id == RUDA_MIR_NO_REGISTER;
}

static bool ruda_checker_has_failed(const RudaChecker *checker) {
    return checker->failure != RUDA_CHECK_OK;
}

static bool ruda_checker_set_ast(
    RudaChecker *checker,
    const RudaAstTree *ast
) {
    if (checker == NULL || ast == NULL || !ast->initialized
        || ast->root == NULL || ast->root->kind != RUDA_SYNTAX_ROOT
        || ast->snapshot == NULL) {
        return false;
    }
    checker->ast = ast;
    checker->source = ruda_source_bytes(ast->snapshot);
    checker->source_length = ruda_source_length(ast->snapshot);
    return checker->source != NULL || checker->source_length == 0;
}

static bool ruda_check_span_valid(
    const RudaChecker *checker,
    size_t start,
    size_t end
) {
    return start <= end && end <= checker->source_length;
}

static bool ruda_check_text_equal(
    const RudaChecker *checker,
    size_t start,
    size_t end,
    const char *text
) {
    size_t length = text != NULL ? strlen(text) : 0;
    return text != NULL && ruda_check_span_valid(checker, start, end)
        && end - start == length
        && memcmp(checker->source + start, text, length) == 0;
}

static bool ruda_check_names_equal(
    const RudaChecker *checker,
    RudaCheckName left,
    RudaCheckName right
) {
    size_t left_length = left.end - left.start;
    size_t right_length = right.end - right.start;
    const unsigned char *left_source = left.snapshot != NULL
        ? ruda_source_bytes(left.snapshot)
        : NULL;
    const unsigned char *right_source = right.snapshot != NULL
        ? ruda_source_bytes(right.snapshot)
        : NULL;
    (void)checker;
    return left_source != NULL && right_source != NULL
        && left.start <= left.end
        && left.end <= ruda_source_length(left.snapshot)
        && right.start <= right.end
        && right.end <= ruda_source_length(right.snapshot)
        && left_length == right_length
        && memcmp(
            left_source + left.start,
            right_source + right.start,
            left_length
        ) == 0;
}

static bool ruda_check_name_bytes(
    RudaCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    const unsigned char *source = name.snapshot != NULL
        ? ruda_source_bytes(name.snapshot)
        : NULL;
    if (bytes == NULL || length == NULL || source == NULL
        || name.start > name.end
        || name.end > ruda_source_length(name.snapshot)) {
        return false;
    }
    *bytes = source + name.start;
    *length = name.end - name.start;
    return true;
}

static const RudaCheckTopLevelEntry *ruda_checker_find_top_level(
    const RudaChecker *checker,
    RudaCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    if (!ruda_check_name_bytes(name, &bytes, &length)) {
        return NULL;
    }
    return ruda_map_get_const(&checker->top_level_index, bytes, length);
}

static const RudaCheckFunctionDeclaration *ruda_checker_find_declaration(
    const RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_find_top_level(
        checker,
        name
    );
    const RudaCheckFunctionDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_FUNCTION) {
        declaration = ruda_vector_at_const(
            &checker->declarations,
            entry->index
        );
    }
    if (declaration != NULL && declaration->signature_valid
        && ruda_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

static RudaCheckConstantDeclaration *ruda_checker_find_constant(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_find_top_level(
        checker,
        name
    );
    RudaCheckConstantDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_CONSTANT) {
        declaration = ruda_vector_at(&checker->constants, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

static RudaCheckTypeAliasDeclaration *ruda_checker_find_type_alias(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_find_top_level(
        checker,
        name
    );
    RudaCheckTypeAliasDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_TYPE_ALIAS) {
        declaration = ruda_vector_at(&checker->type_aliases, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

static RudaCheckRecordDeclaration *ruda_checker_find_record(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_find_top_level(
        checker,
        name
    );
    RudaCheckRecordDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_RECORD) {
        declaration = ruda_vector_at(&checker->records, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

static RudaCheckRecordDeclaration *ruda_checker_find_record_by_type(
    RudaChecker *checker,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->records.length;
        index += 1) {
        RudaCheckRecordDeclaration *declaration = ruda_vector_at(
            &checker->records,
            index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->fields_resolved && declaration->type == type) {
            return declaration;
        }
    }
    return NULL;
}

static RudaCheckRecordFieldDeclaration *ruda_checker_find_record_field(
    RudaChecker *checker,
    RudaCheckRecordDeclaration *record,
    RudaCheckName name,
    size_t *field_index
) {
    size_t index = 0;
    for (index = 0; record != NULL && index < record->fields.length;
        index += 1) {
        RudaCheckRecordFieldDeclaration *field = ruda_vector_at(
            &record->fields,
            index
        );
        if (field != NULL && ruda_check_names_equal(
                checker,
                field->name,
                name
            )) {
            if (field_index != NULL) {
                *field_index = index;
            }
            return field;
        }
    }
    return NULL;
}

static RudaCheckEnumDeclaration *ruda_checker_find_enum(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_find_top_level(
        checker,
        name
    );
    RudaCheckEnumDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_ENUM) {
        declaration = ruda_vector_at(&checker->enums, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

static RudaCheckEnumDeclaration *ruda_checker_find_enum_by_type(
    RudaChecker *checker,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->enums.length;
        index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &checker->enums,
            index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->variants_resolved
            && declaration->type == type) {
            return declaration;
        }
    }
    return NULL;
}

static RudaCheckEnumVariantDeclaration *ruda_checker_find_enum_variant(
    RudaChecker *checker,
    RudaCheckEnumDeclaration *enum_declaration,
    RudaCheckName name,
    size_t *variant_index
) {
    size_t index = 0;
    for (index = 0; enum_declaration != NULL
        && index < enum_declaration->variants.length; index += 1) {
        RudaCheckEnumVariantDeclaration *variant = ruda_vector_at(
            &enum_declaration->variants,
            index
        );
        if (variant != NULL && ruda_check_names_equal(
                checker,
                variant->name,
                name
            )) {
            if (variant_index != NULL) {
                *variant_index = index;
            }
            return variant;
        }
    }
    return NULL;
}

static RudaCheckEnumPayloadItemDeclaration *ruda_checker_find_enum_payload(
    RudaChecker *checker,
    RudaCheckEnumVariantDeclaration *variant,
    RudaCheckName name,
    size_t *payload_index
) {
    size_t index = 0;
    for (index = 0; variant != NULL && index < variant->payload.length;
        index += 1) {
        RudaCheckEnumPayloadItemDeclaration *payload = ruda_vector_at(
            &variant->payload,
            index
        );
        if (payload != NULL && payload->has_name
            && ruda_check_names_equal(checker, payload->name, name)) {
            if (payload_index != NULL) {
                *payload_index = index;
            }
            return payload;
        }
    }
    return NULL;
}

static const RudaAstNode *ruda_check_child_node(
    const RudaAstNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

static const RudaAstNode *ruda_check_child_node_any(
    const RudaAstNode *node,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

static const RudaSyntaxToken *ruda_check_child_token(
    const RudaAstNode *node,
    RudaTokenKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.kind == kind) {
            if (found == ordinal) {
                return &element->as.token;
            }
            found += 1;
        }
    }
    return NULL;
}

static const RudaSyntaxToken *ruda_check_first_token(
    const RudaAstNode *node
) {
    size_t index = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN) {
            return &element->as.token;
        }
    }
    return NULL;
}

static const RudaSyntaxToken *ruda_check_attribute_name(
    const RudaAstNode *attribute
) {
    return attribute != NULL && attribute->kind == RUDA_SYNTAX_ATTRIBUTE
        ? ruda_check_child_token(attribute, RUDA_TOKEN_IDENTIFIER, 0)
        : NULL;
}

static bool ruda_check_attribute_is(
    const RudaChecker *checker,
    const RudaAstNode *attribute,
    const char *name
) {
    const RudaSyntaxToken *token = ruda_check_attribute_name(attribute);
    return token != NULL && !token->synthetic
        && ruda_check_text_equal(checker, token->start, token->end, name);
}

static bool ruda_check_source_is_test_file(const RudaChecker *checker) {
    static const char suffix[] = "_test.ruda";
    size_t path_length = checker != NULL && checker->ast != NULL
        ? ruda_source_path_length(checker->ast->snapshot)
        : 0;
    const char *path = checker != NULL && checker->ast != NULL
        ? ruda_source_path(checker->ast->snapshot)
        : NULL;
    return path != NULL && path_length >= sizeof(suffix) - 1
        && memcmp(
            path + path_length - (sizeof(suffix) - 1),
            suffix,
            sizeof(suffix) - 1
        ) == 0;
}

static bool ruda_checker_add_diagnostic(
    RudaChecker *checker,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaCheckDiagnostic diagnostic;
    if (ruda_checker_has_failed(checker)) {
        return false;
    }
    if (!ruda_check_span_valid(checker, start, end)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (checker->result->diagnostics.length
        >= checker->options.max_diagnostics) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.snapshot = checker->ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&checker->result->diagnostics, &diagnostic)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_checker_add_owned_diagnostic(
    RudaChecker *checker,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    size_t start,
    size_t end
) {
    RudaCheckDiagnostic diagnostic;
    if (ruda_checker_has_failed(checker)) {
        return false;
    }
    if (!ruda_check_span_valid(checker, start, end)
        || (message == NULL && message_length != 0)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (checker->result->diagnostics.length
        >= checker->options.max_diagnostics) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    if (!ruda_owned_bytes_copy(
            &diagnostic.owned_message,
            checker->result->allocator,
            message,
            message_length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = (const char *)diagnostic.owned_message.data;
    diagnostic.snapshot = checker->ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&checker->result->diagnostics, &diagnostic)) {
        ruda_owned_bytes_dispose(&diagnostic.owned_message);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_match_witness_append(
    RudaChecker *checker,
    RudaVector *buffer,
    const void *bytes,
    size_t length
) {
    if (!ruda_vector_append(buffer, bytes, length)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_match_witness_append_text(
    RudaChecker *checker,
    RudaVector *buffer,
    const char *text
) {
    return text != NULL && ruda_match_witness_append(
        checker,
        buffer,
        text,
        strlen(text)
    );
}

static bool ruda_match_witness_append_name(
    RudaChecker *checker,
    RudaVector *buffer,
    RudaCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t displayed = 0;
    if (!ruda_check_name_bytes(name, &bytes, &length)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    displayed = length < 64 ? length : 64;
    return ruda_match_witness_append(
        checker,
        buffer,
        bytes,
        displayed
    ) && (displayed == length || ruda_match_witness_append_text(
        checker,
        buffer,
        "..."
    ));
}

static bool ruda_match_witness_append_size(
    RudaChecker *checker,
    RudaVector *buffer,
    size_t value
) {
    char digits[sizeof(size_t) * 3 + 1];
    size_t cursor = sizeof(digits);
    do {
        size_t digit = value % 10;
        cursor -= 1;
        digits[cursor] = (char)('0' + digit);
        value /= 10;
    } while (value != 0 && cursor != 0);
    return ruda_match_witness_append(
        checker,
        buffer,
        digits + cursor,
        sizeof(digits) - cursor
    );
}

static bool ruda_match_witness_append_u64(
    RudaChecker *checker,
    RudaVector *buffer,
    uint64_t value
) {
    char digits[21];
    size_t cursor = sizeof(digits);
    do {
        uint64_t digit = value % UINT64_C(10);
        cursor -= 1;
        digits[cursor] = (char)('0' + digit);
        value /= UINT64_C(10);
    } while (value != 0 && cursor != 0);
    return ruda_match_witness_append(
        checker,
        buffer,
        digits + cursor,
        sizeof(digits) - cursor
    );
}

static bool ruda_render_enum_match_witness(
    RudaChecker *checker,
    RudaVector *buffer,
    const RudaCheckEnumVariantDeclaration *variant
) {
    size_t payload_index = 0;
    size_t displayed_payloads = 0;
    if (variant == NULL || !ruda_match_witness_append_name(
            checker,
            buffer,
            variant->name
        )) {
        if (variant == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    displayed_payloads = variant->payload.length < 4
        ? variant->payload.length
        : 4;
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        if (!ruda_match_witness_append_text(checker, buffer, "(")) {
            return false;
        }
        for (payload_index = 0; payload_index < displayed_payloads;
            payload_index += 1) {
            if ((payload_index != 0 && !ruda_match_witness_append_text(
                    checker,
                    buffer,
                    ", "
                )) || !ruda_match_witness_append_text(
                    checker,
                    buffer,
                    "_"
                )) {
                return false;
            }
        }
        if (displayed_payloads != variant->payload.length
            && !ruda_match_witness_append_text(
                checker,
                buffer,
                displayed_payloads == 0 ? "..." : ", ..."
            )) {
            return false;
        }
        return ruda_match_witness_append_text(checker, buffer, ")");
    }
    if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
        if (!ruda_match_witness_append_text(checker, buffer, " { ")) {
            return false;
        }
        for (payload_index = 0; payload_index < displayed_payloads;
            payload_index += 1) {
            const RudaCheckEnumPayloadItemDeclaration *payload =
                ruda_vector_at_const(&variant->payload, payload_index);
            if (payload == NULL || !payload->has_name
                || (payload_index != 0 && !ruda_match_witness_append_text(
                    checker,
                    buffer,
                    ", "
                )) || !ruda_match_witness_append_name(
                    checker,
                    buffer,
                    payload->name
                ) || !ruda_match_witness_append_text(
                    checker,
                    buffer,
                    ": _"
                )) {
                if (payload == NULL || !payload->has_name) {
                    ruda_checker_fail(
                        checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                return false;
            }
        }
        if (displayed_payloads != variant->payload.length
            && !ruda_match_witness_append_text(
                checker,
                buffer,
                displayed_payloads == 0 ? "..." : ", ..."
            )) {
            return false;
        }
        return ruda_match_witness_append_text(checker, buffer, " }");
    }
    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
    return false;
}

static bool ruda_add_enum_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaCheckEnumDeclaration *declaration,
    const unsigned char *seen_variants,
    const RudaCheckEnumMatchArm *arms,
    size_t arm_count
) {
    RudaVector buffer;
    size_t variant_index = 0;
    size_t missing_count = 0;
    size_t displayed_count = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || declaration == NULL
        || seen_variants == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        if (seen_variants[variant_index] == 0) {
            missing_count += 1;
        }
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || seen_variants[alternative->variant_index] == 0;
        }
    }
    if (missing_count == 0 || !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            missing_count == 1
                ? "enum match is missing witness: "
                : "enum match is missing witnesses: "
        )) {
        if (missing_count == 0) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INTERNAL_ERROR
            );
        }
        goto cleanup;
    }
    for (variant_index = 0; variant_index < declaration->variants.length
        && displayed_count < 4; variant_index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = NULL;
        if (seen_variants[variant_index] != 0) {
            continue;
        }
        variant = ruda_vector_at_const(
            &declaration->variants,
            variant_index
        );
        if ((displayed_count != 0 && !ruda_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || !ruda_render_enum_match_witness(
                function->checker,
                &buffer,
                variant
            )) {
            goto cleanup;
        }
        displayed_count += 1;
    }
    if (displayed_count != missing_count
        && (!ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "; +"
        ) || !ruda_match_witness_append_size(
            function->checker,
            &buffer,
            missing_count - displayed_count
        ) || !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            " more"
        ))) {
        goto cleanup;
    }
    if (guarded_witness && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        )) {
        goto cleanup;
    }
    accepted = ruda_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_add_bool_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const unsigned char seen[2],
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
) {
    RudaVector buffer;
    size_t missing_count = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    missing_count = (seen[0] == 0 ? 1 : 0)
        + (seen[1] == 0 ? 1 : 0);
    if (missing_count == 0) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || seen[alternative->literal ? 1 : 0] == 0;
        }
    }
    if (!ruda_match_witness_append_text(
            function->checker,
            &buffer,
            missing_count == 1
                ? "boolean match is missing witness: "
                : "boolean match is missing witnesses: "
        ) || (seen[0] == 0 && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "false"
        )) || (seen[0] == 0 && seen[1] == 0
            && !ruda_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || (seen[1] == 0 && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "true"
        )) || (guarded_witness && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_add_i64_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaMap *seen_literals,
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
) {
    RudaVector buffer;
    uint64_t candidate = 0;
    int64_t signed_candidate = 0;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen_literals == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (;;) {
        signed_candidate = (int64_t)candidate;
        if (ruda_map_get_const(
                seen_literals,
                &signed_candidate,
                sizeof(signed_candidate)
            ) == NULL) {
            break;
        }
        if (candidate == (uint64_t)INT64_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        candidate += 1;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || alternative->integer == signed_candidate;
        }
    }
    if (!ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "integer match is missing witness: "
        ) || !ruda_match_witness_append_u64(
            function->checker,
            &buffer,
            candidate
        ) || (guarded_witness && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static const RudaMirStringConstant *ruda_check_match_string_constant(
    const RudaFunctionChecker *function,
    RudaMirStringId string_id
) {
    return function != NULL && string_id != RUDA_MIR_NO_STRING
        ? ruda_vector_at_const(&function->mir.string_constants, string_id)
        : NULL;
}

static bool ruda_check_match_string_equal(
    const RudaFunctionChecker *function,
    RudaMirStringId left_id,
    RudaMirStringId right_id
) {
    const RudaMirStringConstant *left = ruda_check_match_string_constant(
        function,
        left_id
    );
    const RudaMirStringConstant *right = ruda_check_match_string_constant(
        function,
        right_id
    );
    return left != NULL && right != NULL
        && left->value.length == right->value.length
        && (left->value.length == 0 || memcmp(
            left->value.data,
            right->value.data,
            left->value.length
        ) == 0);
}

static bool ruda_add_string_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaMap *seen_literals,
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
) {
    RudaVector candidate;
    RudaVector buffer;
    size_t arm_index = 0;
    bool guarded_witness = false;
    bool candidate_initialized = false;
    bool buffer_initialized = false;
    bool accepted = false;
    static const char candidate_prefix[] = "ruda#";
    size_t candidate_index = 0;
    memset(&candidate, 0, sizeof(candidate));
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || seen_literals == NULL
        || (arms == NULL && arm_count != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &candidate,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    candidate_initialized = true;
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    buffer_initialized = true;
    for (candidate_index = 0;
        candidate_index <= seen_literals->length;
        candidate_index += 1) {
        candidate.length = 0;
        if (!ruda_match_witness_append_text(
                function->checker,
                &candidate,
                candidate_prefix
            ) || !ruda_match_witness_append_size(
                function->checker,
                &candidate,
                candidate_index
            )) {
            goto cleanup;
        }
        if (ruda_map_get_const(
                seen_literals,
                candidate.data,
                candidate.length
            ) == NULL) {
            break;
        }
    }
    if (candidate_index > seen_literals->length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        size_t alternative_index = 0;
        if (arms[arm_index].guard == NULL) {
            continue;
        }
        for (alternative_index = 0;
            alternative_index < arms[arm_index].alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arms[arm_index].alternatives,
                    alternative_index
                );
            const RudaMirStringConstant *literal = alternative != NULL
                ? ruda_check_match_string_constant(
                    function,
                    alternative->string_constant
                )
                : NULL;
            if (alternative == NULL
                || (!alternative->is_catch_all && literal == NULL)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            guarded_witness = guarded_witness
                || alternative->is_catch_all
                || (literal->value.length == candidate.length
                    && (candidate.length == 0 || memcmp(
                        literal->value.data,
                        candidate.data,
                        candidate.length
                    ) == 0));
        }
    }
    if (!ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "string match is missing witness: \""
        ) || !ruda_match_witness_append(
            function->checker,
            &buffer,
            candidate.data,
            candidate.length
        ) || !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "\""
        ) || (guarded_witness && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (buffer_initialized) {
        ruda_vector_dispose(&buffer);
    }
    if (candidate_initialized) {
        ruda_vector_dispose(&candidate);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_add_record_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaCheckRecordDeclaration *record,
    bool guarded_witness
) {
    RudaVector buffer;
    size_t field_index = 0;
    size_t displayed_fields = 0;
    bool initialized = false;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (node == NULL || record == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &buffer,
            function->checker->result->allocator,
            sizeof(unsigned char)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    displayed_fields = record->fields.length < 4
        ? record->fields.length
        : 4;
    if (!ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "record match is missing witness: "
        ) || !ruda_match_witness_append_name(
            function->checker,
            &buffer,
            record->name
        ) || !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            record->fields.length == 0 ? " {}" : " { "
        )) {
        goto cleanup;
    }
    for (field_index = 0; field_index < displayed_fields;
        field_index += 1) {
        const RudaCheckRecordFieldDeclaration *field = ruda_vector_at_const(
            &record->fields,
            field_index
        );
        if (field == NULL
            || (field_index != 0 && !ruda_match_witness_append_text(
                function->checker,
                &buffer,
                ", "
            )) || !ruda_match_witness_append_name(
                function->checker,
                &buffer,
                field->name
            ) || !ruda_match_witness_append_text(
                function->checker,
                &buffer,
                ": _"
            )) {
            if (field == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
    }
    if ((displayed_fields != record->fields.length
            && !ruda_match_witness_append_text(
                function->checker,
                &buffer,
                displayed_fields == 0 ? "..." : ", ..."
            )) || (record->fields.length != 0
            && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            " }"
        )) || (guarded_witness && !ruda_match_witness_append_text(
            function->checker,
            &buffer,
            "; guarded patterns do not count toward exhaustiveness"
        ))) {
        goto cleanup;
    }
    accepted = ruda_checker_add_owned_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        buffer.data,
        buffer.length,
        node->start,
        node->end
    );

cleanup:
    if (initialized) {
        ruda_vector_dispose(&buffer);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_check_exact_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result
) {
    const RudaSyntaxToken *name = NULL;
    RudaCheckName declared_name;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaCheckRecordDeclaration *record = NULL;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    if (node == NULL || result == NULL || node->kind != RUDA_SYNTAX_TYPE) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    name = ruda_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0);
    if (name == NULL || name->synthetic
        || !ruda_check_span_valid(checker, name->start, name->end)) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (node->child_count != 1) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (ruda_check_text_equal(checker, name->start, name->end, "bool")) {
        *result = RUDA_TYPE_BOOL;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "i8")) {
        *result = RUDA_TYPE_I8;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "i16")) {
        *result = RUDA_TYPE_I16;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "i32")) {
        *result = RUDA_TYPE_I32;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "i64")
        || ruda_check_text_equal(checker, name->start, name->end, "int")) {
        *result = RUDA_TYPE_I64;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "u8")
        || ruda_check_text_equal(checker, name->start, name->end, "byte")) {
        *result = RUDA_TYPE_U8;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "u16")) {
        *result = RUDA_TYPE_U16;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "u32")) {
        *result = RUDA_TYPE_U32;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "u64")
        || ruda_check_text_equal(checker, name->start, name->end, "uint")) {
        *result = RUDA_TYPE_U64;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "f32")) {
        *result = RUDA_TYPE_F32;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "f64")
        || ruda_check_text_equal(checker, name->start, name->end, "float")) {
        *result = RUDA_TYPE_F64;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "rune")) {
        *result = RUDA_TYPE_RUNE;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "string")) {
        *result = RUDA_TYPE_STRING;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "void")) {
        *result = RUDA_TYPE_VOID;
    } else if (ruda_check_text_equal(checker, name->start, name->end, "never")) {
        *result = RUDA_TYPE_NEVER;
    } else {
        declared_name.snapshot = checker->ast->snapshot;
        declared_name.start = name->start;
        declared_name.end = name->end;
        record = ruda_checker_find_record(checker, declared_name);
        if (record != NULL) {
            *result = record->type;
            return true;
        }
        enum_declaration = ruda_checker_find_enum(checker, declared_name);
        if (enum_declaration != NULL) {
            *result = enum_declaration->type;
            return true;
        }
        alias = ruda_checker_find_type_alias(checker, declared_name);
        if (alias != NULL) {
            if (!ruda_evaluate_type_alias(
                    checker,
                    alias,
                    name->start,
                    name->end
                )) {
                *result = RUDA_TYPE_INVALID;
                return false;
            }
            *result = alias->type;
            return true;
        }
        *result = RUDA_TYPE_INVALID;
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
            "unknown type name",
            name->start,
            name->end
        );
        return false;
    }
    return true;
}

static const RudaCheckLocal *ruda_function_find_local(
    const RudaFunctionChecker *function,
    RudaCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index - 1
        );
        if (local != NULL && ruda_check_names_equal(
                function->checker,
                local->name,
                name
            )) {
            return local;
        }
        index -= 1;
    }
    return NULL;
}

static const RudaCheckLocal *ruda_function_find_local_in_current_scope(
    const RudaFunctionChecker *function,
    RudaCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index - 1
        );
        if (local != NULL && local->scope_depth < function->scope_depth) {
            break;
        }
        if (local != NULL && local->scope_depth == function->scope_depth
            && ruda_check_names_equal(
                function->checker,
                local->name,
                name
            )) {
            return local;
        }
        index -= 1;
    }
    return NULL;
}

static RudaCheckLocal *ruda_function_find_local_mutable(
    RudaFunctionChecker *function,
    RudaCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        RudaCheckLocal *local = ruda_vector_at(&function->locals, index - 1);
        if (local != NULL && ruda_check_names_equal(
                function->checker,
                local->name,
                name
            )) {
            return local;
        }
        index -= 1;
    }
    return NULL;
}

static bool ruda_initialization_snapshot_init(
    RudaFunctionChecker *function,
    size_t count,
    RudaInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || count > function->locals.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->allocator = function->checker->result->allocator;
    snapshot->count = count;
    snapshot->allocation_size = count == 0 ? 1 : count;
    snapshot->states = ruda_allocate(
        &snapshot->allocator,
        snapshot->allocation_size
    );
    if (snapshot->states == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < count; index += 1) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index
        );
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            ruda_deallocate(
                &snapshot->allocator,
                snapshot->states,
                snapshot->allocation_size
            );
            memset(snapshot, 0, sizeof(*snapshot));
            return false;
        }
        snapshot->states[index] = local->initialized ? 1 : 0;
    }
    return true;
}

static bool ruda_initialization_snapshot_capture(
    RudaFunctionChecker *function,
    RudaInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || snapshot->states == NULL
        || snapshot->count > function->locals.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < snapshot->count; index += 1) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index
        );
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        snapshot->states[index] = local->initialized ? 1 : 0;
    }
    return true;
}

static bool ruda_initialization_snapshot_restore(
    RudaFunctionChecker *function,
    const RudaInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || snapshot->states == NULL
        || snapshot->count > function->locals.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < snapshot->count; index += 1) {
        RudaCheckLocal *local = ruda_vector_at(&function->locals, index);
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        local->initialized = snapshot->states[index] != 0;
    }
    return true;
}

static void ruda_initialization_snapshot_dispose(
    RudaInitializationSnapshot *snapshot
) {
    if (snapshot == NULL) {
        return;
    }
    ruda_deallocate(
        &snapshot->allocator,
        snapshot->states,
        snapshot->allocation_size
    );
    memset(snapshot, 0, sizeof(*snapshot));
}

static bool ruda_function_add_register(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaMirRegister *result
) {
    if (function->mir.register_types.length
        >= function->mir.options.max_registers) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_register(&function->mir, type, result)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_function_add_block(
    RudaFunctionChecker *function,
    RudaMirBlockId *result
) {
    if (function->mir.blocks.length >= function->mir.options.max_blocks) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_block(&function->mir, result)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_function_append_instruction(
    RudaFunctionChecker *function,
    const RudaMirInstruction *instruction
) {
    if (function->mir.instruction_count
        >= function->mir.options.max_instructions) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_append_instruction(
            &function->mir,
            function->block,
            instruction
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_function_emit_instruction(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId type,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (!ruda_function_add_register(function, type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.integer = integer;
    instruction.boolean = boolean;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_function_emit_f64_constant(
    RudaFunctionChecker *function,
    uint64_t bits,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (!ruda_function_add_register(function, RUDA_TYPE_F64, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.f64_bits = bits;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_F64;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_function_record_layout(
    RudaFunctionChecker *function,
    const RudaCheckRecordDeclaration *declaration,
    RudaMirRecordId *result
) {
    RudaMirRecordField *fields = NULL;
    size_t field_bytes = 0;
    size_t new_field_count = 0;
    size_t index = 0;
    bool layout_exists = false;
    if (function == NULL) {
        return false;
    }
    if (declaration == NULL || result == NULL
        || !declaration->declaration_valid || !declaration->fields_resolved
        || declaration->type == RUDA_TYPE_INVALID
        || !ruda_size_multiply(
            declaration->fields.length,
            sizeof(RudaMirRecordField),
            &field_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < function->mir.records.length; index += 1) {
        const RudaMirRecord *existing = ruda_vector_at_const(
            &function->mir.records,
            index
        );
        if (existing != NULL && existing->type == declaration->type) {
            layout_exists = true;
            break;
        }
    }
    if (!layout_exists
        && (function->mir.records.length
                >= function->mir.options.max_record_layouts
            || !ruda_size_add(
                function->mir.record_fields.length,
                declaration->fields.length,
                &new_field_count
            ) || new_field_count > function->mir.options.max_record_fields)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (field_bytes != 0) {
        fields = ruda_allocate(
            &function->checker->result->allocator,
            field_bytes
        );
        if (fields == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
    }
    for (index = 0; index < declaration->fields.length; index += 1) {
        const RudaCheckRecordFieldDeclaration *field = ruda_vector_at_const(
            &declaration->fields,
            index
        );
        if (field == NULL || fields == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            ruda_deallocate(
                &function->checker->result->allocator,
                fields,
                field_bytes
            );
            return false;
        }
        fields[index].type = field->type;
        fields[index].is_mutable = field->is_mutable;
    }
    if (!ruda_mir_add_record(
            &function->mir,
            declaration->type,
            fields,
            declaration->fields.length,
            result
        )) {
        ruda_checker_fail(
            function->checker,
            layout_exists
                ? RUDA_CHECK_INTERNAL_ERROR
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        ruda_deallocate(
            &function->checker->result->allocator,
            fields,
            field_bytes
        );
        return false;
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        fields,
        field_bytes
    );
    return true;
}

static bool ruda_function_enum_layout(
    RudaFunctionChecker *function,
    const RudaCheckEnumDeclaration *declaration,
    RudaMirEnumId *result
) {
    RudaMirEnumVariant *variants = NULL;
    RudaMirEnumPayload *payloads = NULL;
    size_t variant_bytes = 0;
    size_t payload_bytes = 0;
    size_t payload_count = 0;
    size_t new_variant_count = 0;
    size_t new_payload_count = 0;
    size_t variant_index = 0;
    size_t payload_cursor = 0;
    bool layout_exists = false;
    bool accepted = false;
    if (function == NULL) {
        return false;
    }
    if (declaration == NULL || result == NULL
        || !declaration->declaration_valid || !declaration->variants_resolved) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = ruda_vector_at_const(
            &declaration->variants,
            variant_index
        );
        if (variant == NULL || !variant->payload_initialized
            || !ruda_size_add(
                payload_count,
                variant->payload.length,
                &payload_count
            )) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
    }
    if (!ruda_size_multiply(
            declaration->variants.length,
            sizeof(RudaMirEnumVariant),
            &variant_bytes
        ) || !ruda_size_multiply(
            payload_count,
            sizeof(RudaMirEnumPayload),
            &payload_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    for (variant_index = 0; variant_index < function->mir.enums.length;
        variant_index += 1) {
        const RudaMirEnum *existing = ruda_vector_at_const(
            &function->mir.enums,
            variant_index
        );
        if (existing != NULL && existing->type == declaration->type) {
            layout_exists = true;
            break;
        }
    }
    if (!layout_exists
        && (function->mir.enums.length
                >= function->mir.options.max_enum_layouts
            || !ruda_size_add(
                function->mir.enum_variants.length,
                declaration->variants.length,
                &new_variant_count
            ) || new_variant_count > function->mir.options.max_enum_variants
            || !ruda_size_add(
                function->mir.enum_payloads.length,
                payload_count,
                &new_payload_count
            ) || new_payload_count > function->mir.options.max_enum_payloads)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (variant_bytes != 0) {
        variants = ruda_allocate(
            &function->checker->result->allocator,
            variant_bytes
        );
        if (variants == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (payload_bytes != 0) {
        payloads = ruda_allocate(
            &function->checker->result->allocator,
            payload_bytes
        );
        if (payloads == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = ruda_vector_at_const(
            &declaration->variants,
            variant_index
        );
        size_t payload_index = 0;
        if (variant == NULL || variants == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        switch (variant->kind) {
            case RUDA_CHECKED_ENUM_VARIANT_UNIT:
                variants[variant_index].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
                break;
            case RUDA_CHECKED_ENUM_VARIANT_POSITIONAL:
                variants[variant_index].kind =
                    RUDA_MIR_ENUM_VARIANT_POSITIONAL;
                break;
            case RUDA_CHECKED_ENUM_VARIANT_NAMED:
                variants[variant_index].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
                break;
            default:
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
        }
        variants[variant_index].payload_start = payload_cursor;
        variants[variant_index].payload_count = variant->payload.length;
        for (payload_index = 0; payload_index < variant->payload.length;
            payload_index += 1) {
            const RudaCheckEnumPayloadItemDeclaration *payload =
                ruda_vector_at_const(&variant->payload, payload_index);
            if (payload == NULL || payloads == NULL
                || payload->type == RUDA_TYPE_INVALID
                || (variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
                    && payload->has_name)
                || (variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED
                    && !payload->has_name)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            payloads[payload_cursor].type = payload->type;
            payload_cursor += 1;
        }
    }
    if (payload_cursor != payload_count || !ruda_mir_add_enum(
            &function->mir,
            declaration->type,
            variants,
            declaration->variants.length,
            payloads,
            payload_count,
            result
        )) {
        ruda_checker_fail(
            function->checker,
            layout_exists
                ? RUDA_CHECK_INTERNAL_ERROR
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        payloads,
        payload_bytes
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        variants,
        variant_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_function_emit_record_access(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId result_type,
    RudaMirRegister receiver,
    RudaMirRegister value,
    RudaMirRecordId record,
    size_t field,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if ((kind != RUDA_MIR_RECORD_GET && kind != RUDA_MIR_RECORD_SET)
        || result == NULL
        || !ruda_function_add_register(function, result_type, &target)) {
        if (result == NULL || (kind != RUDA_MIR_RECORD_GET
                && kind != RUDA_MIR_RECORD_SET)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = receiver;
    instruction.second = value;
    instruction.record = record;
    instruction.field = field;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_integer_magnitude(
    RudaChecker *checker,
    const RudaSyntaxToken *token,
    uint64_t limit,
    uint64_t *result
) {
    const unsigned char *bytes = NULL;
    size_t cursor = 0;
    size_t length = 0;
    unsigned int base = 10;
    uint64_t value = 0;
    if (token == NULL || result == NULL || token->kind != RUDA_TOKEN_INTEGER
        || token->synthetic
        || !ruda_check_span_valid(checker, token->start, token->end)) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    bytes = checker->source + token->start;
    length = token->end - token->start;
    if (length >= 2 && bytes[0] == '0') {
        if (bytes[1] == 'b' || bytes[1] == 'B') {
            base = 2;
            cursor = 2;
        } else if (bytes[1] == 'o' || bytes[1] == 'O') {
            base = 8;
            cursor = 2;
        } else if (bytes[1] == 'x' || bytes[1] == 'X') {
            base = 16;
            cursor = 2;
        }
    }
    for (; cursor < length; cursor += 1) {
        unsigned char byte = bytes[cursor];
        unsigned int digit = 0;
        if (byte == '_') {
            continue;
        }
        if (byte >= '0' && byte <= '9') {
            digit = (unsigned int)(byte - '0');
        } else if (byte >= 'a' && byte <= 'f') {
            digit = (unsigned int)(byte - 'a') + 10;
        } else if (byte >= 'A' && byte <= 'F') {
            digit = (unsigned int)(byte - 'A') + 10;
        } else {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (digit >= base || value > (limit - digit) / base) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
                "integer literal is outside the range of int",
                token->start,
                token->end
            );
            return false;
        }
        value = value * base + digit;
    }
    *result = value;
    return true;
}

static bool ruda_compile_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);

static bool ruda_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
);

static bool ruda_compile_enum_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);

static bool ruda_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);

static bool ruda_function_emit_copy(
    RudaFunctionChecker *function,
    RudaMirRegister target,
    RudaMirRegister source,
    size_t start,
    size_t end
);

static bool ruda_function_set_goto(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirBlockId target,
    size_t start,
    size_t end
);

static bool ruda_function_set_branch(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false,
    size_t start,
    size_t end
);

static bool ruda_function_set_panic(
    RudaFunctionChecker *function,
    RudaMirRegister message,
    size_t start,
    size_t end
);

static bool ruda_function_set_never_call(
    RudaFunctionChecker *function,
    RudaMirCallId call,
    size_t start,
    size_t end
);

static bool ruda_type_is_integer(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_U64;
}

static bool ruda_type_is_numeric(RudaTypeId type) {
    return ruda_type_is_integer(type)
        || type == RUDA_TYPE_F32 || type == RUDA_TYPE_F64;
}

static bool ruda_compile_literal(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaSyntaxToken *token = ruda_check_first_token(node);
    RudaChecker *checker = function->checker;
    if (token == NULL || token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (token->kind == RUDA_TOKEN_KW_TRUE
        || token->kind == RUDA_TOKEN_KW_FALSE) {
        return ruda_function_emit_instruction(
            function,
            RUDA_MIR_CONSTANT_BOOL,
            RUDA_TYPE_BOOL,
            RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER,
            0,
            token->kind == RUDA_TOKEN_KW_TRUE,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == RUDA_TOKEN_INTEGER) {
        uint64_t magnitude = 0;
        if (expected == RUDA_TYPE_STRING) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "integer literal does not match the required string type",
                token->start,
                token->end
            );
            return false;
        }
        if (expected != RUDA_TYPE_INVALID && expected != RUDA_TYPE_I64) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            return false;
        }
        if (!ruda_integer_magnitude(
                checker,
                token,
                (uint64_t)INT64_MAX,
                &magnitude
            )) {
            return false;
        }
        return ruda_function_emit_instruction(
            function,
            RUDA_MIR_CONSTANT_I64,
            RUDA_TYPE_I64,
            RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER,
            (int64_t)magnitude,
            false,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == RUDA_TOKEN_FLOAT) {
        uint64_t bits = 0;
        RudaF64ParseStatus parse_status;
        if (expected != RUDA_TYPE_INVALID && expected != RUDA_TYPE_F64) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "f64 literal does not match the required type",
                token->start,
                token->end
            );
            return false;
        }
        parse_status = ruda_f64_parse_decimal(
            checker->source + token->start,
            token->end - token->start,
            checker->result->allocator,
            checker->options.max_float_literal_digits,
            &bits
        );
        if (parse_status == RUDA_F64_PARSE_ALLOCATION_FAILED) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        if (parse_status == RUDA_F64_PARSE_LIMIT_REACHED) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        if (parse_status != RUDA_F64_PARSE_OK) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        return ruda_function_emit_f64_constant(
            function,
            bits,
            token->start,
            token->end,
            result
        );
    }
    if (token->kind == RUDA_TOKEN_STRING
        || token->kind == RUDA_TOKEN_RAW_STRING) {
        RudaOwnedBytes decoded;
        RudaStringLiteralStatus decode_status;
        RudaMirStringId string_constant = RUDA_MIR_NO_STRING;
        RudaMirRegister target = RUDA_MIR_NO_REGISTER;
        RudaMirInstruction instruction;
        size_t new_string_bytes = 0;
        if (expected != RUDA_TYPE_INVALID && expected != RUDA_TYPE_STRING) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "string literal does not match the required type",
                token->start,
                token->end
            );
            return false;
        }
        decode_status = ruda_string_literal_decode(
            checker->source + token->start,
            token->end - token->start,
            token->kind == RUDA_TOKEN_RAW_STRING,
            checker->result->allocator,
            &decoded
        );
        if (decode_status != RUDA_STRING_LITERAL_OK) {
            ruda_checker_fail(
                checker,
                decode_status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
                    ? RUDA_CHECK_ALLOCATION_FAILED
                    : RUDA_CHECK_INTERNAL_ERROR
            );
            return false;
        }
        if (function->mir.string_constants.length
                >= function->mir.options.max_string_constants
            || !ruda_size_add(
                function->mir.string_bytes,
                decoded.length,
                &new_string_bytes
            ) || new_string_bytes > function->mir.options.max_string_bytes) {
            ruda_owned_bytes_dispose(&decoded);
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        if (!ruda_function_add_register(function, RUDA_TYPE_STRING, &target)
            || !ruda_mir_add_string_constant(
                &function->mir,
                decoded.data,
                decoded.length,
                &string_constant
            )) {
            ruda_owned_bytes_dispose(&decoded);
            if (!ruda_checker_has_failed(checker)) {
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            }
            return false;
        }
        ruda_owned_bytes_dispose(&decoded);
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = RUDA_MIR_CONSTANT_STRING;
        instruction.target = target;
        instruction.first = RUDA_MIR_NO_REGISTER;
        instruction.second = RUDA_MIR_NO_REGISTER;
        instruction.string_constant = string_constant;
        instruction.span.start = token->start;
        instruction.span.end = token->end;
        if (!ruda_function_append_instruction(function, &instruction)) {
            return false;
        }
        result->type = RUDA_TYPE_STRING;
        result->register_id = target;
        result->valid = true;
        return true;
    }
    ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

static bool ruda_compile_name(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaSyntaxToken *token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckName name;
    const RudaCheckLocal *local = NULL;
    RudaCheckConstantDeclaration *constant = NULL;
    if (token == NULL || token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = token->start;
    name.end = token->end;
    local = ruda_function_find_local(function, name);
    if (local == NULL) {
        constant = ruda_checker_find_constant(function->checker, name);
        if (constant != NULL) {
            if (!ruda_evaluate_constant(
                    function->checker,
                    constant,
                    token->start,
                    token->end
                )) {
                return false;
            }
            if (constant->value.type == RUDA_TYPE_I64) {
                return ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_I64,
                    RUDA_TYPE_I64,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    constant->value.as.i64,
                    false,
                    token->start,
                    token->end,
                    result
                );
            }
            if (constant->value.type == RUDA_TYPE_BOOL) {
                return ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_BOOL,
                    RUDA_TYPE_BOOL,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    0,
                    constant->value.as.boolean,
                    token->start,
                    token->end,
                    result
                );
            }
            if (constant->value.type == RUDA_TYPE_F64) {
                return ruda_function_emit_f64_constant(
                    function,
                    constant->value.as.f64_bits,
                    token->start,
                    token->end,
                    result
                );
            }
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (ruda_checker_find_top_level(function->checker, name) != NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (ruda_check_text_equal(
                function->checker,
                token->start,
                token->end,
                "panic"
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_CALL_ARITY,
                "panic requires exactly one message argument",
                token->start,
                token->end
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            token->start,
            token->end
        );
        return false;
    }
    if (!local->initialized) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "local is used before it is initialized",
            token->start,
            token->end
        );
        return false;
    }
    result->type = local->type;
    result->register_id = local->register_id;
    result->valid = true;
    return true;
}

static const RudaAstNode *ruda_expression_only_node(
    const RudaAstNode *node
) {
    const RudaAstNode *result = NULL;
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (result != NULL) {
                return NULL;
            }
            result = element->as.node;
        }
    }
    return result;
}

static bool ruda_compile_unary(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaSyntaxToken *operator_token = ruda_check_first_token(node);
    const RudaAstNode *operand = ruda_expression_only_node(node);
    RudaExpressionValue value;
    RudaMirInstructionKind kind;
    RudaTypeId operand_type = RUDA_TYPE_INVALID;
    memset(&value, 0, sizeof(value));
    value.register_id = RUDA_MIR_NO_REGISTER;
    if (operator_token == NULL || operand == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (operator_token->kind == RUDA_TOKEN_MINUS
        && operand->kind == RUDA_SYNTAX_LITERAL_EXPRESSION) {
        const RudaSyntaxToken *literal = ruda_check_first_token(operand);
        uint64_t magnitude = 0;
        if (literal != NULL && literal->kind == RUDA_TOKEN_INTEGER
            && ruda_integer_magnitude(
                function->checker,
                literal,
                (uint64_t)INT64_MAX + UINT64_C(1),
                &magnitude
            )) {
            if (magnitude == (uint64_t)INT64_MAX + UINT64_C(1)) {
                return ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_I64,
                    RUDA_TYPE_I64,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    INT64_MIN,
                    false,
                    node->start,
                    node->end,
                    result
                );
            }
        } else if (literal != NULL && literal->kind == RUDA_TOKEN_FLOAT) {
            uint64_t bits = 0;
            RudaF64ParseStatus parse_status = ruda_f64_parse_decimal(
                function->checker->source + literal->start,
                literal->end - literal->start,
                function->checker->result->allocator,
                function->checker->options.max_float_literal_digits,
                &bits
            );
            if (parse_status == RUDA_F64_PARSE_OK) {
                return ruda_function_emit_f64_constant(
                    function,
                    ruda_f64_negate(bits),
                    node->start,
                    node->end,
                    result
                );
            }
            if (parse_status == RUDA_F64_PARSE_ALLOCATION_FAILED) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
            } else if (parse_status == RUDA_F64_PARSE_LIMIT_REACHED) {
                ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            } else {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        } else if (ruda_checker_has_failed(function->checker)
            || function->checker->result->diagnostics.length
                != function->diagnostic_start) {
            return false;
        }
    }
    if (operator_token->kind == RUDA_TOKEN_BANG) {
        operand_type = RUDA_TYPE_BOOL;
        kind = RUDA_MIR_NOT_BOOL;
    } else if (operator_token->kind == RUDA_TOKEN_MINUS) {
        operand_type = RUDA_TYPE_INVALID;
        kind = RUDA_MIR_NEGATE_CHECKED_I64;
    } else if (operator_token->kind == RUDA_TOKEN_TILDE) {
        operand_type = RUDA_TYPE_I64;
        kind = RUDA_MIR_BITWISE_NOT_I64;
    } else {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (!ruda_compile_expression(function, operand, operand_type, &value)) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        *result = value;
        return true;
    }
    if (operator_token->kind == RUDA_TOKEN_MINUS
        && (value.type == RUDA_TYPE_I64 || value.type == RUDA_TYPE_F64)) {
        operand_type = value.type;
        kind = value.type == RUDA_TYPE_F64
            ? RUDA_MIR_NEGATE_F64
            : RUDA_MIR_NEGATE_CHECKED_I64;
    }
    if (value.type != operand_type) {
        if ((operator_token->kind == RUDA_TOKEN_MINUS
                && (value.type == RUDA_TYPE_I8
                    || value.type == RUDA_TYPE_I16
                    || value.type == RUDA_TYPE_I32
                    || value.type == RUDA_TYPE_F32
                    || value.type == RUDA_TYPE_F64))
            || (operator_token->kind == RUDA_TOKEN_TILDE
                && ruda_type_is_integer(value.type))) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    return ruda_function_emit_instruction(
        function,
        kind,
        operand_type,
        value.register_id,
        RUDA_MIR_NO_REGISTER,
        0,
        false,
        node->start,
        node->end,
        result
    );
}

static bool ruda_binary_parts(
    const RudaAstNode *node,
    const RudaAstNode **left,
    const RudaSyntaxToken **operator_token,
    const RudaAstNode **right
) {
    size_t index = 0;
    *left = NULL;
    *operator_token = NULL;
    *right = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (*left == NULL) {
                *left = element->as.node;
            } else if (*right == NULL) {
                *right = element->as.node;
            } else {
                return false;
            }
        } else if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (*operator_token != NULL) {
                return false;
            }
            *operator_token = &element->as.token;
        }
    }
    return *left != NULL && *operator_token != NULL && *right != NULL;
}

static bool ruda_expression_is_integer_literal(const RudaAstNode *node) {
    const RudaAstNode *inner = NULL;
    const RudaSyntaxToken *token = NULL;
    if (node == NULL) {
        return false;
    }
    if (node->kind == RUDA_SYNTAX_EXPRESSION
        || node->kind == RUDA_SYNTAX_PAREN_GROUP) {
        inner = ruda_expression_only_node(node);
        return ruda_expression_is_integer_literal(inner);
    }
    if (node->kind != RUDA_SYNTAX_LITERAL_EXPRESSION) {
        return false;
    }
    token = ruda_check_first_token(node);
    return token != NULL && token->kind == RUDA_TOKEN_INTEGER;
}

static bool ruda_compile_short_circuit(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left_node,
    const RudaSyntaxToken *operator_token,
    const RudaAstNode *right_node,
    RudaExpressionValue *result
) {
    RudaExpressionValue left;
    RudaExpressionValue right;
    RudaExpressionValue shortcut;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId branch_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId right_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId shortcut_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId join_block = RUDA_MIR_NO_BLOCK;
    bool is_and = operator_token->kind == RUDA_TOKEN_AMPERSAND_AMPERSAND;
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(&shortcut, 0, sizeof(shortcut));
    if (!ruda_compile_expression(
            function,
            left_node,
            RUDA_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    if (left.type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "logical operator requires bool operands",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    branch_block = function->block;
    if (!ruda_function_add_register(function, RUDA_TYPE_BOOL, &target)
        || !ruda_function_add_block(function, &right_block)
        || !ruda_function_add_block(function, &shortcut_block)
        || !ruda_function_add_block(function, &join_block)
        || !ruda_function_set_branch(
            function,
            branch_block,
            left.register_id,
            is_and ? right_block : shortcut_block,
            is_and ? shortcut_block : right_block,
            operator_token->start,
            operator_token->end
        )) {
        return false;
    }

    function->block = shortcut_block;
    if (!ruda_function_emit_instruction(
            function,
            RUDA_MIR_CONSTANT_BOOL,
            RUDA_TYPE_BOOL,
            RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER,
            0,
            !is_and,
            operator_token->start,
            operator_token->end,
            &shortcut
        ) || !ruda_function_emit_copy(
            function,
            target,
            shortcut.register_id,
            operator_token->start,
            operator_token->end
        ) || !ruda_function_set_goto(
            function,
            shortcut_block,
            join_block,
            operator_token->start,
            operator_token->end
        )) {
        return false;
    }

    function->block = right_block;
    if (!ruda_compile_expression(
            function,
            right_node,
            RUDA_TYPE_INVALID,
            &right
        )) {
        return false;
    }
    if (!ruda_expression_is_never(function, &right)
        && right.type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "logical operator requires bool operands",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (!ruda_expression_is_never(function, &right)
        && (!ruda_function_emit_copy(
            function,
            target,
            right.register_id,
            right_node->start,
            right_node->end
        ) || !ruda_function_set_goto(
            function,
            function->block,
            join_block,
            right_node->start,
            right_node->end
        ))) {
        return false;
    }
    function->block = join_block;
    function->terminated = false;
    result->type = RUDA_TYPE_BOOL;
    result->register_id = target;
    result->valid = true;
    (void)node;
    return true;
}

static bool ruda_compile_binary(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaAstNode *left_node = NULL;
    const RudaAstNode *right_node = NULL;
    const RudaSyntaxToken *operator_token = NULL;
    RudaExpressionValue left;
    RudaExpressionValue right;
    RudaExpressionValue equality;
    RudaMirInstructionKind kind = RUDA_MIR_ADD_CHECKED_I64;
    RudaTypeId operand_type = RUDA_TYPE_I64;
    RudaTypeId result_type = RUDA_TYPE_I64;
    bool swap = false;
    bool negate = false;
    bool equality_operator = false;
    bool numeric_operator = false;
    bool comparison_operator = false;
    bool bitwise_operator = false;
    bool shift_operator = false;
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(&equality, 0, sizeof(equality));
    if (!ruda_binary_parts(node, &left_node, &operator_token, &right_node)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (operator_token->kind == RUDA_TOKEN_AMPERSAND_AMPERSAND
        || operator_token->kind == RUDA_TOKEN_PIPE_PIPE) {
        return ruda_compile_short_circuit(
            function,
            node,
            left_node,
            operator_token,
            right_node,
            result
        );
    }
    switch (operator_token->kind) {
        case RUDA_TOKEN_PLUS:
            kind = RUDA_MIR_ADD_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_MINUS:
            kind = RUDA_MIR_SUBTRACT_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_STAR:
            kind = RUDA_MIR_MULTIPLY_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_SLASH:
            kind = RUDA_MIR_DIVIDE_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_PERCENT:
            kind = RUDA_MIR_REMAINDER_CHECKED_I64;
            numeric_operator = true;
            break;
        case RUDA_TOKEN_AMPERSAND:
            kind = RUDA_MIR_BITWISE_AND_I64;
            bitwise_operator = true;
            break;
        case RUDA_TOKEN_CARET:
            kind = RUDA_MIR_BITWISE_XOR_I64;
            bitwise_operator = true;
            break;
        case RUDA_TOKEN_PIPE:
            kind = RUDA_MIR_BITWISE_OR_I64;
            bitwise_operator = true;
            break;
        case RUDA_TOKEN_SHIFT_LEFT:
            kind = RUDA_MIR_SHIFT_LEFT_CHECKED_I64;
            shift_operator = true;
            break;
        case RUDA_TOKEN_SHIFT_RIGHT:
            kind = RUDA_MIR_SHIFT_RIGHT_I64;
            shift_operator = true;
            break;
        case RUDA_TOKEN_LESS:
            kind = RUDA_MIR_LESS_I64;
            result_type = RUDA_TYPE_BOOL;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_LESS_EQUAL:
            kind = RUDA_MIR_LESS_EQUAL_I64;
            result_type = RUDA_TYPE_BOOL;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_GREATER:
            kind = RUDA_MIR_LESS_I64;
            result_type = RUDA_TYPE_BOOL;
            swap = true;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_GREATER_EQUAL:
            kind = RUDA_MIR_LESS_EQUAL_I64;
            result_type = RUDA_TYPE_BOOL;
            swap = true;
            comparison_operator = true;
            break;
        case RUDA_TOKEN_EQUAL_EQUAL:
            equality_operator = true;
            result_type = RUDA_TYPE_BOOL;
            break;
        case RUDA_TOKEN_BANG_EQUAL:
            equality_operator = true;
            result_type = RUDA_TYPE_BOOL;
            negate = true;
            break;
        default:
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
    }
    if (!ruda_compile_expression(
            function,
            left_node,
            RUDA_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    operand_type = left.type;
    if (!ruda_compile_expression(
            function,
            right_node,
            operand_type,
            &right
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &right)) {
        *result = right;
        return true;
    }
    if (left.type != right.type) {
        if ((ruda_expression_is_integer_literal(left_node)
                && ruda_type_is_integer(right.type))
            || (ruda_expression_is_integer_literal(right_node)
                && ruda_type_is_integer(left.type))) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (operand_type != RUDA_TYPE_I64
        && operand_type != RUDA_TYPE_F64
        && !(equality_operator && operand_type == RUDA_TYPE_BOOL)) {
        if ((numeric_operator || comparison_operator)
            && ruda_type_is_numeric(operand_type)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (bitwise_operator && ruda_type_is_integer(operand_type)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (shift_operator && ruda_type_is_integer(operand_type)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (equality_operator) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (operand_type == RUDA_TYPE_F64) {
        if (bitwise_operator || shift_operator
            || operator_token->kind == RUDA_TOKEN_PERCENT) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_INVALID_OPERATOR,
                "operator is not defined for f64 operands",
                operator_token->start,
                operator_token->end
            );
            return false;
        }
        if (numeric_operator) {
            result_type = RUDA_TYPE_F64;
        }
        if (equality_operator) {
            kind = RUDA_MIR_EQUAL_F64;
        } else if (comparison_operator) {
            kind = operator_token->kind == RUDA_TOKEN_LESS
                    || operator_token->kind == RUDA_TOKEN_GREATER
                ? RUDA_MIR_LESS_F64
                : RUDA_MIR_LESS_EQUAL_F64;
        } else if (operator_token->kind == RUDA_TOKEN_PLUS) {
            kind = RUDA_MIR_ADD_F64;
        } else if (operator_token->kind == RUDA_TOKEN_MINUS) {
            kind = RUDA_MIR_SUBTRACT_F64;
        } else if (operator_token->kind == RUDA_TOKEN_STAR) {
            kind = RUDA_MIR_MULTIPLY_F64;
        } else if (operator_token->kind == RUDA_TOKEN_SLASH) {
            kind = RUDA_MIR_DIVIDE_F64;
        }
    }
    if (equality_operator) {
        if (operand_type == RUDA_TYPE_BOOL) {
            kind = RUDA_MIR_EQUAL_BOOL;
        } else if (operand_type == RUDA_TYPE_I64) {
            kind = RUDA_MIR_EQUAL_I64;
        }
    }
    if (!ruda_function_emit_instruction(
            function,
            kind,
            result_type,
            swap ? right.register_id : left.register_id,
            swap ? left.register_id : right.register_id,
            0,
            false,
            node->start,
            node->end,
            negate ? &equality : result
        )) {
        return false;
    }
    if (!negate) {
        return true;
    }
    return ruda_function_emit_instruction(
        function,
        RUDA_MIR_NOT_BOOL,
        RUDA_TYPE_BOOL,
        equality.register_id,
        RUDA_MIR_NO_REGISTER,
        0,
        false,
        node->start,
        node->end,
        result
    );
}

static bool ruda_resolve_enum_member(
    RudaFunctionChecker *function,
    const RudaAstNode *member_node,
    RudaCheckEnumDeclaration **enum_declaration,
    RudaCheckEnumVariantDeclaration **variant_declaration,
    size_t *variant_index,
    bool *resolved
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *type_token = NULL;
    const RudaSyntaxToken *variant_token = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaCheckName type_name;
    RudaCheckName variant_name;
    if (function == NULL || enum_declaration == NULL
        || variant_declaration == NULL || variant_index == NULL
        || resolved == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *enum_declaration = NULL;
    *variant_declaration = NULL;
    *variant_index = 0;
    *resolved = false;
    if (!ruda_member_parts(member_node, &receiver_node, &variant_token)
        || receiver_node == NULL
        || receiver_node->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        return true;
    }
    type_token = ruda_check_child_token(
        receiver_node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    if (type_token == NULL || type_token->synthetic || variant_token == NULL
        || variant_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_name.snapshot = function->checker->ast->snapshot;
    type_name.start = type_token->start;
    type_name.end = type_token->end;
    if (ruda_function_find_local(function, type_name) != NULL) {
        return true;
    }
    *enum_declaration = ruda_checker_find_enum(
        function->checker,
        type_name
    );
    if (*enum_declaration == NULL) {
        alias = ruda_checker_find_type_alias(function->checker, type_name);
        if (alias != NULL && ruda_evaluate_type_alias(
                function->checker,
                alias,
                type_name.start,
                type_name.end
            )) {
            *enum_declaration = ruda_checker_find_enum_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (*enum_declaration == NULL) {
        return !ruda_checker_has_failed(function->checker);
    }
    *resolved = true;
    variant_name.snapshot = function->checker->ast->snapshot;
    variant_name.start = variant_token->start;
    variant_name.end = variant_token->end;
    *variant_declaration = ruda_checker_find_enum_variant(
        function->checker,
        *enum_declaration,
        variant_name,
        variant_index
    );
    if (*variant_declaration == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
            "enum type has no variant with this name",
            variant_name.start,
            variant_name.end
        );
        return false;
    }
    return true;
}

static bool ruda_function_emit_enum_construction(
    RudaFunctionChecker *function,
    const RudaCheckEnumDeclaration *enum_declaration,
    size_t variant_index,
    const RudaMirEnumArgument *arguments,
    size_t argument_count,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaMirEnumConstructionId construction =
        RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    size_t new_argument_count = 0;
    if (function == NULL || enum_declaration == NULL || result == NULL
        || (arguments == NULL && argument_count != 0)) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    if (!ruda_function_enum_layout(
            function,
            enum_declaration,
            &enum_id
        )) {
        return false;
    }
    if (function->mir.enum_constructions.length
            >= function->mir.options.max_enum_constructions
        || !ruda_size_add(
            function->mir.enum_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_enum_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_enum_construction(
            &function->mir,
            enum_id,
            variant_index,
            arguments,
            argument_count,
            &construction
        )) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    if (!ruda_function_add_register(
            function,
            enum_declaration->type,
            &target
        )) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = enum_id;
    instruction.enum_construction = construction;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = enum_declaration->type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_function_emit_enum_access(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId result_type,
    RudaMirRegister receiver,
    RudaMirEnumId enum_id,
    size_t variant,
    size_t payload,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if (result == NULL || (kind != RUDA_MIR_ENUM_IS_VARIANT
            && kind != RUDA_MIR_ENUM_REQUIRE_VARIANT
            && kind != RUDA_MIR_ENUM_GET_PAYLOAD)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!ruda_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = receiver;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = enum_id;
    instruction.variant = variant;
    instruction.field = payload;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_compile_enum_unit(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    size_t variant_index = 0;
    if (!ruda_resolve_enum_member(
            function,
            node,
            &enum_declaration,
            &variant,
            &variant_index,
            resolved
        )) {
        return false;
    }
    if (!*resolved) {
        return true;
    }
    if (variant == NULL
        || variant->kind != RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant requires its declared payload constructor syntax",
            node->start,
            node->end
        );
        return false;
    }
    return ruda_function_emit_enum_construction(
        function,
        enum_declaration,
        variant_index,
        NULL,
        0,
        node->start,
        node->end,
        result
    );
}

static bool ruda_compile_enum_positional(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const RudaAstNode *arguments_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_ARGUMENTS,
        0
    );
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    RudaMirEnumArgument *arguments = NULL;
    size_t variant_index = 0;
    size_t argument_count = 0;
    size_t argument_index = 0;
    size_t argument_bytes = 0;
    size_t child_index = 0;
    bool accepted = false;
    if (resolved == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    if (member_node == NULL) {
        return true;
    }
    if (!ruda_resolve_enum_member(
            function,
            member_node,
            &enum_declaration,
            &variant,
            &variant_index,
            resolved
        )) {
        goto cleanup;
    }
    if (!*resolved) {
        accepted = true;
        goto cleanup;
    }
    if (function->constant_evaluation) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (arguments_node == NULL || variant == NULL
        || variant->kind != RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant is not a positional payload constructor",
            node->start,
            node->end
        );
        goto cleanup;
    }
    for (child_index = 0; child_index < arguments_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            arguments_node,
            child_index
        );
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                || argument_count == SIZE_MAX) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
    }
    if (argument_count != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "positional enum payload count does not match its declaration",
            arguments_node->start,
            arguments_node->end
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            argument_count,
            sizeof(RudaMirEnumArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (child_index = 0; child_index < arguments_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            arguments_node,
            child_index
        );
        const RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaTypeId payload_type = RUDA_TYPE_INVALID;
        RudaExpressionValue value;
        if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE) {
            continue;
        }
        payload = ruda_vector_at_const(&variant->payload, argument_index);
        if (payload == NULL || arguments == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        payload_type = payload->type;
        memset(&value, 0, sizeof(value));
        if (!ruda_compile_expression(
                function,
                element->as.node,
                payload_type,
                &value
            )) {
            goto cleanup;
        }
        if (ruda_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (value.type != payload_type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "enum payload value type does not match its declaration",
                element->as.node->start,
                element->as.node->end
            );
            goto cleanup;
        }
        arguments[argument_index].register_id = value.register_id;
        arguments[argument_index].type = value.type;
        argument_index += 1;
    }
    if (argument_index != argument_count) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    accepted = ruda_function_emit_enum_construction(
        function,
        enum_declaration,
        variant_index,
        arguments,
        argument_count,
        node->start,
        node->end,
        result
    );

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_identity_same(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const RudaAstNode *arguments_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_ARGUMENTS,
        0
    );
    const RudaAstNode *receiver_node = NULL;
    const RudaAstNode *argument_nodes[2] = {NULL, NULL};
    const RudaSyntaxToken *identity_token = NULL;
    const RudaSyntaxToken *member_token = NULL;
    RudaCheckName identity_name;
    RudaExpressionValue left;
    RudaExpressionValue right;
    RudaCheckRecordDeclaration *record_declaration = NULL;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    size_t argument_count = 0;
    size_t index = 0;
    if (function == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    if (member_node == NULL || !ruda_member_parts(
            member_node,
            &receiver_node,
            &member_token
        ) || receiver_node == NULL
        || receiver_node->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        return true;
    }
    identity_token = ruda_check_child_token(
        receiver_node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    if (identity_token == NULL || identity_token->synthetic
        || member_token == NULL || member_token->synthetic
        || !ruda_check_text_equal(
            function->checker,
            identity_token->start,
            identity_token->end,
            "identity"
        ) || !ruda_check_text_equal(
            function->checker,
            member_token->start,
            member_token->end,
            "same"
        )) {
        return true;
    }
    identity_name.snapshot = function->checker->ast->snapshot;
    identity_name.start = identity_token->start;
    identity_name.end = identity_token->end;
    if (ruda_function_find_local(function, identity_name) != NULL
        || ruda_checker_find_top_level(
            function->checker,
            identity_name
        ) != NULL) {
        return true;
    }
    *resolved = true;
    if (ruda_check_child_node(node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "identity.same does not accept type arguments",
            node->start,
            node->end
        );
        return false;
    }
    if (function->constant_evaluation) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "record identity is not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    if (arguments_node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments_node, index);
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                || argument_count >= 2) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_CALL_ARITY,
                    "identity.same requires exactly two arguments",
                    arguments_node->start,
                    arguments_node->end
                );
                return false;
            }
            argument_nodes[argument_count] = element->as.node;
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
    }
    if (argument_count != 2) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "identity.same requires exactly two arguments",
            arguments_node->start,
            arguments_node->end
        );
        return false;
    }
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    if (!ruda_compile_expression(
            function,
            argument_nodes[0],
            RUDA_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    record_declaration = ruda_checker_find_record_by_type(
        function->checker,
        left.type
    );
    if (record_declaration == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "identity.same requires record arguments",
            argument_nodes[0]->start,
            argument_nodes[0]->end
        );
        return false;
    }
    if (!ruda_compile_expression(
            function,
            argument_nodes[1],
            RUDA_TYPE_INVALID,
            &right
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &right)) {
        *result = right;
        return true;
    }
    if (right.type != left.type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "identity.same arguments must have the same nominal record type",
            argument_nodes[1]->start,
            argument_nodes[1]->end
        );
        return false;
    }
    if (!ruda_function_record_layout(function, record_declaration, &record)
        || !ruda_function_add_register(function, RUDA_TYPE_BOOL, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_IDENTITY_SAME;
    instruction.target = target;
    instruction.first = left.register_id;
    instruction.second = right.register_id;
    instruction.record = record;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_BOOL;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_compile_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_NAME_EXPRESSION,
        0
    );
    const RudaAstNode *arguments_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_ARGUMENTS,
        0
    );
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        callee_node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaCheckFunctionDeclaration *declaration = NULL;
    RudaCheckName name;
    RudaMirCallArgument *arguments = NULL;
    size_t argument_count = 0;
    size_t argument_bytes = 0;
    size_t argument_index = 0;
    size_t new_call_argument_count = 0;
    size_t index = 0;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    bool accepted = false;
    bool enum_resolved = false;
    bool identity_resolved = false;
    if (name_token != NULL && !name_token->synthetic
        && ruda_check_text_equal(
            function->checker,
            name_token->start,
            name_token->end,
            "panic"
        ) && ruda_function_find_local(function, (RudaCheckName){
            function->checker->ast->snapshot,
            name_token->start,
            name_token->end
        }) == NULL && ruda_checker_find_top_level(
            function->checker,
            (RudaCheckName){
                function->checker->ast->snapshot,
                name_token->start,
                name_token->end
            }
        ) == NULL) {
        const RudaAstNode *message_node = NULL;
        RudaExpressionValue message;
        memset(&message, 0, sizeof(message));
        if (ruda_check_child_node(
                node,
                RUDA_SYNTAX_TYPE_ARGUMENTS,
                0
            ) != NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "panic does not accept type arguments",
                node->start,
                node->end
            );
            goto cleanup;
        }
        if (function->constant_evaluation) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
                "constant expression can panic",
                node->start,
                node->end
            );
            goto cleanup;
        }
        if (arguments_node == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        for (index = 0; index < arguments_node->child_count; index += 1) {
            const RudaAstElement *element = ruda_ast_child(
                arguments_node,
                index
            );
            if (element == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            if (element->kind == RUDA_AST_ELEMENT_NODE) {
                if (element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                    || message_node != NULL) {
                    (void)ruda_checker_add_diagnostic(
                        function->checker,
                        RUDA_DIAGNOSTIC_CALL_ARITY,
                        "panic requires exactly one message argument",
                        arguments_node->start,
                        arguments_node->end
                    );
                    goto cleanup;
                }
                message_node = element->as.node;
            } else if (element->as.token.synthetic) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
        }
        if (message_node == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_CALL_ARITY,
                "panic requires exactly one message argument",
                arguments_node->start,
                arguments_node->end
            );
            goto cleanup;
        }
        if (!ruda_compile_expression(
                function,
                message_node,
                RUDA_TYPE_STRING,
                &message
            )) {
            goto cleanup;
        }
        if (ruda_expression_is_never(function, &message)) {
            *result = message;
            accepted = true;
            goto cleanup;
        }
        if (message.type != RUDA_TYPE_STRING) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "panic message must have type string",
                message_node->start,
                message_node->end
            );
            goto cleanup;
        }
        if (!ruda_function_set_panic(
                function,
                message.register_id,
                node->start,
                node->end
            )) {
            goto cleanup;
        }
        ruda_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (!ruda_compile_identity_same(
            function,
            node,
            &identity_resolved,
            result
        )) {
        goto cleanup;
    }
    if (identity_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!ruda_compile_enum_positional(
            function,
            node,
            &enum_resolved,
            result
        )) {
        goto cleanup;
    }
    if (enum_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (function->constant_evaluation) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "function calls are not permitted in this constant subset",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (callee_node == NULL || arguments_node == NULL || name_token == NULL
        || name_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (ruda_check_child_node(node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (ruda_function_find_local(function, name) != NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    declaration = ruda_checker_find_declaration(function->checker, name);
    if (declaration == NULL) {
        if (ruda_checker_find_top_level(function->checker, name) != NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "called name is not a function",
                name.start,
                name.end
            );
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_NAME,
                "unknown function name",
                name.start,
                name.end
            );
        }
        goto cleanup;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments_node, index);
        if (element == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                || argument_count == SIZE_MAX) {
                ruda_checker_fail(
                    function->checker,
                    element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                        ? RUDA_CHECK_INCOMPLETE_LANGUAGE
                        : RUDA_CHECK_LIMIT_REACHED
                );
                goto cleanup;
            }
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
    }
    if (argument_count != declaration->parameter_types.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "call argument count does not match the function signature",
            arguments_node->start,
            arguments_node->end
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            argument_count,
            sizeof(RudaMirCallArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments_node, index);
        const RudaTypeId *parameter_type = NULL;
        RudaExpressionValue value;
        if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE) {
            continue;
        }
        parameter_type = ruda_vector_at_const(
            &declaration->parameter_types,
            argument_index
        );
        if (parameter_type == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INTERNAL_ERROR
            );
            goto cleanup;
        }
        memset(&value, 0, sizeof(value));
        if (!ruda_compile_expression(
                function,
                element->as.node,
                *parameter_type,
                &value
            )) {
            goto cleanup;
        }
        if (ruda_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (value.type != *parameter_type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "argument type does not match the function parameter type",
                element->as.node->start,
                element->as.node->end
            );
            goto cleanup;
        }
        if (arguments == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INTERNAL_ERROR
            );
            goto cleanup;
        }
        arguments[argument_index].register_id = value.register_id;
        arguments[argument_index].type = value.type;
        argument_index += 1;
    }
    if (argument_index != argument_count
        || function->mir.calls.length >= function->mir.options.max_calls
        || !ruda_size_add(
            function->mir.call_arguments.length,
            argument_count,
            &new_call_argument_count
        ) || new_call_argument_count
            > function->mir.options.max_call_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_mir_add_call(
            &function->mir,
            declaration->symbol,
            declaration->result_type,
            arguments,
            argument_count,
            &call
        )) {
        if (!ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
        }
        goto cleanup;
    }
    if (declaration->result_type == RUDA_TYPE_NEVER) {
        if (!ruda_function_set_never_call(
                function,
                call,
                node->start,
                node->end
            )) {
            goto cleanup;
        }
        ruda_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (!ruda_function_add_register(
            function,
            declaration->result_type,
            &target
        )) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CALL;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.call = call;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = declaration->result_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_enum_named(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const RudaAstNode *fields_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_RECORD_FIELDS,
        0
    );
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    RudaMirEnumArgument *arguments = NULL;
    unsigned char *seen = NULL;
    size_t variant_index = 0;
    size_t argument_bytes = 0;
    size_t seen_bytes = 0;
    size_t child_index = 0;
    size_t supplied_count = 0;
    bool accepted = false;
    if (resolved == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    if (member_node == NULL) {
        return true;
    }
    if (!ruda_resolve_enum_member(
            function,
            member_node,
            &enum_declaration,
            &variant,
            &variant_index,
            resolved
        )) {
        goto cleanup;
    }
    if (!*resolved) {
        accepted = true;
        goto cleanup;
    }
    if (function->constant_evaluation) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (fields_node == NULL || variant == NULL
        || variant->kind != RUDA_CHECKED_ENUM_VARIANT_NAMED) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant is not a named payload constructor",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            variant->payload.length,
            sizeof(RudaMirEnumArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    seen_bytes = variant->payload.length;
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        seen = ruda_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (arguments == NULL || seen == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen, 0, seen_bytes);
    }
    for (child_index = 0; child_index < fields_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            fields_node,
            child_index
        );
        const RudaAstNode *field_node = NULL;
        const RudaAstNode *expression = NULL;
        const RudaSyntaxToken *field_token = NULL;
        RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaTypeId payload_type = RUDA_TYPE_INVALID;
        RudaCheckName field_name;
        RudaExpressionValue value;
        size_t payload_index = 0;
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            continue;
        }
        field_node = element->as.node;
        if (field_node == NULL || field_node->kind != RUDA_SYNTAX_FIELD_VALUE) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_token = ruda_check_child_token(
            field_node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        payload = ruda_checker_find_enum_payload(
            function->checker,
            variant,
            field_name,
            &payload_index
        );
        if (payload == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "enum variant has no payload field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || arguments == NULL
            || payload_index >= variant->payload.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        payload_type = payload->type;
        if (seen[payload_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "enum payload field is supplied more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        expression = ruda_check_child_node(
            field_node,
            RUDA_SYNTAX_EXPRESSION,
            0
        );
        memset(&value, 0, sizeof(value));
        value.register_id = RUDA_MIR_NO_REGISTER;
        if (expression != NULL) {
            if (!ruda_compile_expression(
                    function,
                    expression,
                    payload_type,
                    &value
                )) {
                goto cleanup;
            }
            if (ruda_expression_is_never(function, &value)) {
                *result = value;
                accepted = true;
                goto cleanup;
            }
        } else {
            const RudaCheckLocal *local = ruda_function_find_local(
                function,
                field_name
            );
            if (local == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNKNOWN_NAME,
                    "enum payload shorthand requires a visible local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!local->initialized) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
                    "enum payload shorthand reads an uninitialized local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            value.type = local->type;
            value.register_id = local->register_id;
            value.valid = true;
        }
        if (value.type != payload_type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "enum payload value type does not match its declaration",
                expression != NULL ? expression->start : field_name.start,
                expression != NULL ? expression->end : field_name.end
            );
            goto cleanup;
        }
        arguments[payload_index].register_id = value.register_id;
        arguments[payload_index].type = value.type;
        seen[payload_index] = 1;
        supplied_count += 1;
    }
    if (supplied_count != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "named enum payload construction omits a required field",
            node->start,
            node->end
        );
        goto cleanup;
    }
    accepted = ruda_function_emit_enum_construction(
        function,
        enum_declaration,
        variant_index,
        arguments,
        variant->payload.length,
        node->start,
        node->end,
        result
    );

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_record_construction(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaAstNode *name_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_NAME_EXPRESSION,
        0
    );
    const RudaAstNode *fields_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_RECORD_FIELDS,
        0
    );
    const RudaSyntaxToken *type_token = ruda_check_child_token(
        name_node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckName type_name;
    RudaCheckRecordDeclaration *declaration = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaMirRecordArgument *arguments = NULL;
    unsigned char *seen = NULL;
    size_t argument_bytes = 0;
    size_t seen_bytes = 0;
    size_t new_argument_count = 0;
    size_t child_index = 0;
    size_t supplied_count = 0;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirRecordConstructionId construction =
        RUDA_MIR_NO_RECORD_CONSTRUCTION;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    bool accepted = false;
    if (function->constant_evaluation) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        goto cleanup;
    }
    if (name_node == NULL || fields_node == NULL || type_token == NULL
        || type_token->synthetic) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        goto cleanup;
    }
    type_name.snapshot = function->checker->ast->snapshot;
    type_name.start = type_token->start;
    type_name.end = type_token->end;
    declaration = ruda_checker_find_record(function->checker, type_name);
    if (declaration == NULL) {
        alias = ruda_checker_find_type_alias(function->checker, type_name);
        if (alias != NULL && ruda_evaluate_type_alias(
                function->checker,
                alias,
                type_name.start,
                type_name.end
            )) {
            declaration = ruda_checker_find_record_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (declaration == NULL) {
        if (ruda_checker_find_top_level(function->checker, type_name) != NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "constructed name is not a nominal record type",
                type_name.start,
                type_name.end
            );
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
                "unknown record type name",
                type_name.start,
                type_name.end
            );
        }
        goto cleanup;
    }
    if (!ruda_size_multiply(
            declaration->fields.length,
            sizeof(RudaMirRecordArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    seen_bytes = declaration->fields.length;
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(arguments, 0, argument_bytes);
    }
    if (seen_bytes != 0) {
        seen = ruda_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (seen == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen, 0, seen_bytes);
    }
    for (child_index = 0; child_index < fields_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            fields_node,
            child_index
        );
        const RudaAstNode *field_node = NULL;
        const RudaAstNode *expression = NULL;
        const RudaSyntaxToken *field_token = NULL;
        RudaCheckName field_name;
        RudaCheckRecordFieldDeclaration *field = NULL;
        RudaExpressionValue value;
        size_t field_index = 0;
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            continue;
        }
        field_node = element->as.node;
        if (field_node == NULL || field_node->kind != RUDA_SYNTAX_FIELD_VALUE) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_token = ruda_check_child_token(
            field_node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        field = ruda_checker_find_record_field(
            function->checker,
            declaration,
            field_name,
            &field_index
        );
        if (field == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "record type has no field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || arguments == NULL
            || field_index >= declaration->fields.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[field_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "record field is supplied more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        expression = ruda_check_child_node(
            field_node,
            RUDA_SYNTAX_EXPRESSION,
            0
        );
        memset(&value, 0, sizeof(value));
        value.register_id = RUDA_MIR_NO_REGISTER;
        if (expression != NULL) {
            if (!ruda_compile_expression(
                    function,
                    expression,
                    field->type,
                    &value
                )) {
                goto cleanup;
            }
            if (ruda_expression_is_never(function, &value)) {
                *result = value;
                accepted = true;
                goto cleanup;
            }
        } else {
            const RudaCheckLocal *local = ruda_function_find_local(
                function,
                field_name
            );
            if (local == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNKNOWN_NAME,
                    "record field shorthand requires a visible local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!local->initialized) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
                    "record field shorthand reads an uninitialized local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            value.type = local->type;
            value.register_id = local->register_id;
            value.valid = true;
        }
        if (value.type != field->type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "record field value type does not match its declaration",
                expression != NULL ? expression->start : field_name.start,
                expression != NULL ? expression->end : field_name.end
            );
            goto cleanup;
        }
        arguments[field_index].register_id = value.register_id;
        arguments[field_index].type = value.type;
        seen[field_index] = 1;
        supplied_count += 1;
    }
    if (supplied_count != declaration->fields.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "record construction omits a required field",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!ruda_function_record_layout(function, declaration, &record)) {
        goto cleanup;
    }
    if (function->mir.record_constructions.length
            >= function->mir.options.max_record_constructions
        || !ruda_size_add(
            function->mir.record_arguments.length,
            declaration->fields.length,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_record_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_mir_add_record_construction(
            &function->mir,
            record,
            arguments,
            declaration->fields.length,
            &construction
        )) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    if (!ruda_function_add_register(function, declaration->type, &target)) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_NEW;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.record = record;
    instruction.record_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = declaration->type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
) {
    size_t index = 0;
    *receiver = NULL;
    *member = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (*receiver != NULL) {
                return false;
            }
            *receiver = element->as.node;
        } else if (element->as.token.kind == RUDA_TOKEN_IDENTIFIER) {
            if (*member != NULL) {
                return false;
            }
            *member = &element->as.token;
        }
    }
    return *receiver != NULL && *member != NULL && !(*member)->synthetic;
}

static bool ruda_resolve_record_member(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *receiver,
    RudaCheckRecordDeclaration **record_declaration,
    RudaCheckRecordFieldDeclaration **field_declaration,
    RudaMirRecordId *record,
    size_t *field_index
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *field_token = NULL;
    RudaCheckName field_name;
    if (!ruda_member_parts(node, &receiver_node, &field_token)
        || !ruda_compile_expression(
            function,
            receiver_node,
            RUDA_TYPE_INVALID,
            receiver
        )) {
        if (!ruda_checker_has_failed(function->checker)
            && function->checker->result->diagnostics.length
                == function->diagnostic_start) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
        }
        return false;
    }
    if (ruda_expression_is_never(function, receiver)) {
        return true;
    }
    *record_declaration = ruda_checker_find_record_by_type(
        function->checker,
        receiver->type
    );
    if (*record_declaration == NULL) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    field_name.snapshot = function->checker->ast->snapshot;
    field_name.start = field_token->start;
    field_name.end = field_token->end;
    *field_declaration = ruda_checker_find_record_field(
        function->checker,
        *record_declaration,
        field_name,
        field_index
    );
    if (*field_declaration == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
            "record type has no field with this name",
            field_name.start,
            field_name.end
        );
        return false;
    }
    return ruda_function_record_layout(
        function,
        *record_declaration,
        record
    );
}

static bool ruda_compile_record_member(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    RudaExpressionValue receiver;
    RudaCheckRecordDeclaration *record_declaration = NULL;
    RudaCheckRecordFieldDeclaration *field_declaration = NULL;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    size_t field_index = 0;
    memset(&receiver, 0, sizeof(receiver));
    if (!ruda_resolve_record_member(
            function,
            node,
            &receiver,
            &record_declaration,
            &field_declaration,
            &record,
            &field_index
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    (void)record_declaration;
    return ruda_function_emit_record_access(
        function,
        RUDA_MIR_RECORD_GET,
        field_declaration->type,
        receiver.register_id,
        RUDA_MIR_NO_REGISTER,
        record,
        field_index,
        node->start,
        node->end,
        result
    );
}

static bool ruda_compile_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *inner = NULL;
    bool enum_resolved = false;
    if (result == NULL || node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->register_id = RUDA_MIR_NO_REGISTER;
    if (node->kind == RUDA_SYNTAX_EXPRESSION
        || node->kind == RUDA_SYNTAX_PAREN_GROUP) {
        inner = ruda_expression_only_node(node);
        if (inner == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        return ruda_compile_expression(function, inner, expected, result);
    }
    if (node->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        return ruda_compile_name(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_LITERAL_EXPRESSION) {
        return ruda_compile_literal(function, node, expected, result);
    }
    if (node->kind == RUDA_SYNTAX_UNARY_EXPRESSION) {
        return ruda_compile_unary(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_BINARY_EXPRESSION) {
        return ruda_compile_binary(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_CALL_EXPRESSION) {
        return ruda_compile_call(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_RECORD_EXPRESSION) {
        if (!ruda_compile_enum_named(
                function,
                node,
                &enum_resolved,
                result
            )) {
            return false;
        }
        if (enum_resolved) {
            return true;
        }
        return ruda_compile_record_construction(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_MEMBER_EXPRESSION) {
        if (!ruda_compile_enum_unit(
                function,
                node,
                &enum_resolved,
                result
            )) {
            return false;
        }
        if (enum_resolved) {
            return true;
        }
        return ruda_compile_record_member(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_MATCH_EXPRESSION) {
        return ruda_compile_enum_match(
            function,
            node,
            expected,
            result
        );
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

static bool ruda_function_add_local(
    RudaFunctionChecker *function,
    RudaCheckName name,
    RudaTypeId type,
    bool mutable_binding,
    bool initialized,
    RudaCheckLocal **result
) {
    RudaCheckLocal local;
    RudaMirRegister register_id = RUDA_MIR_NO_REGISTER;
    if (function->locals.length
        >= function->checker->options.max_locals_per_function) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_function_add_register(function, type, &register_id)) {
        return false;
    }
    local.name = name;
    local.type = type;
    local.register_id = register_id;
    local.scope_depth = function->scope_depth;
    local.mutable_binding = mutable_binding;
    local.initialized = initialized;
    if (!ruda_vector_push(&function->locals, &local)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (result != NULL) {
        *result = ruda_vector_at(&function->locals, function->locals.length - 1);
    }
    return true;
}

static bool ruda_function_emit_copy(
    RudaFunctionChecker *function,
    RudaMirRegister target,
    RudaMirRegister source,
    size_t start,
    size_t end
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_COPY;
    instruction.target = target;
    instruction.first = source;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.span.start = start;
    instruction.span.end = end;
    return ruda_function_append_instruction(function, &instruction);
}

static bool ruda_enum_match_name(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaCheckName *name
) {
    const RudaSyntaxToken *token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    if (node == NULL || node->kind != RUDA_SYNTAX_NAME_PATTERN
        || name == NULL || token == NULL || token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (ruda_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 1) != NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    name->snapshot = function->checker->ast->snapshot;
    name->start = token->start;
    name->end = token->end;
    return true;
}

static bool ruda_enum_match_add_binding(
    RudaFunctionChecker *function,
    RudaVector *bindings,
    RudaCheckName name,
    RudaTypeId type
) {
    size_t index = 0;
    RudaCheckEnumMatchBinding binding;
    if (bindings == NULL || name.snapshot == NULL
        || type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < bindings->length; index += 1) {
        const RudaCheckEnumMatchBinding *existing = ruda_vector_at_const(
            bindings,
            index
        );
        if (existing != NULL && ruda_check_names_equal(
                function->checker,
                existing->name,
                name
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "pattern binding name is repeated in the same alternative",
                name.start,
                name.end
            );
            return false;
        }
    }
    binding.name = name;
    binding.type = type;
    if (!ruda_vector_push(bindings, &binding)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static bool ruda_enum_match_collect_simple_binding(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId type,
    RudaVector *bindings
) {
    RudaCheckName name;
    if (node != NULL && node->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (node != NULL && node->kind == RUDA_SYNTAX_NAME_PATTERN) {
        return ruda_enum_match_name(function, node, &name)
            && ruda_enum_match_add_binding(
                function,
                bindings,
                name,
                type
            );
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

static bool ruda_enum_match_validate_positional(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    const RudaCheckEnumVariantDeclaration *variant,
    RudaVector *bindings
) {
    const RudaAstNode *positional = ruda_check_child_node(
        constructor,
        RUDA_SYNTAX_POSITIONAL_PATTERN,
        0
    );
    const RudaAstNode *list = ruda_check_child_node(
        positional,
        RUDA_SYNTAX_PATTERN_LIST,
        0
    );
    size_t count = 0;
    const RudaAstNode *binding = NULL;
    if (positional == NULL || list == NULL || variant == NULL
        || bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    while (ruda_check_child_node_any(list, count) != NULL) {
        count += 1;
    }
    if (count != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "positional enum pattern arity does not match its variant",
            positional->start,
            positional->end
        );
        return false;
    }
    for (count = 0; count < variant->payload.length; count += 1) {
        const RudaCheckEnumPayloadItemDeclaration *payload =
            ruda_vector_at_const(&variant->payload, count);
        binding = ruda_check_child_node_any(list, count);
        if (payload == NULL || !ruda_enum_match_collect_simple_binding(
                function,
                binding,
                payload->type,
                bindings
            )) {
            if (payload == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            return false;
        }
    }
    return true;
}

static bool ruda_enum_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckEnumVariantDeclaration *variant,
    RudaVector *bindings
) {
    const RudaAstNode *named = ruda_check_child_node(
        constructor,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    const RudaAstNode *fields = ruda_check_child_node(
        named,
        RUDA_SYNTAX_PATTERN_FIELDS,
        0
    );
    unsigned char *seen = NULL;
    size_t seen_bytes = variant != NULL ? variant->payload.length : 0;
    size_t field_ordinal = 0;
    size_t supplied = 0;
    bool accepted = false;
    if (named == NULL || fields == NULL || variant == NULL
        || bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (seen_bytes != 0) {
        seen = ruda_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (seen == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen, 0, seen_bytes);
    }
    for (;;) {
        const RudaAstNode *field = ruda_check_child_node(
            fields,
            RUDA_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const RudaSyntaxToken *field_token = NULL;
        const RudaAstNode *binding = NULL;
        RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaCheckName field_name;
        size_t payload_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = ruda_check_child_token(
            field,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        payload = ruda_checker_find_enum_payload(
            function->checker,
            variant,
            field_name,
            &payload_index
        );
        if (payload == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "enum variant has no payload field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || payload_index >= variant->payload.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[payload_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "enum payload pattern names a field more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        binding = ruda_check_child_node_any(field, 0);
        if (binding != NULL) {
            if (!ruda_enum_match_collect_simple_binding(
                    function,
                    binding,
                    payload->type,
                    bindings
                )) {
                goto cleanup;
            }
        } else if (!ruda_enum_match_add_binding(
                function,
                bindings,
                field_name,
                payload->type
            )) {
            goto cleanup;
        }
        seen[payload_index] = 1;
        supplied += 1;
    }
    if (supplied != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "named enum payload pattern omits a required field",
            named->start,
            named->end
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_record_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckRecordDeclaration *record,
    RudaVector *bindings
) {
    const RudaAstNode *named = ruda_check_child_node(
        constructor,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    const RudaAstNode *fields = ruda_check_child_node(
        named,
        RUDA_SYNTAX_PATTERN_FIELDS,
        0
    );
    unsigned char *seen = NULL;
    size_t seen_bytes = record != NULL ? record->fields.length : 0;
    size_t field_ordinal = 0;
    size_t supplied = 0;
    bool accepted = false;
    if (named == NULL || fields == NULL || record == NULL
        || bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (seen_bytes != 0) {
        seen = ruda_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (seen == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen, 0, seen_bytes);
    }
    for (;;) {
        const RudaAstNode *field = ruda_check_child_node(
            fields,
            RUDA_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const RudaSyntaxToken *field_token = NULL;
        const RudaAstNode *binding = NULL;
        RudaCheckRecordFieldDeclaration *field_declaration = NULL;
        RudaCheckName field_name;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = ruda_check_child_token(
            field,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        field_declaration = ruda_checker_find_record_field(
            function->checker,
            record,
            field_name,
            &field_index
        );
        if (field_declaration == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "record type has no field with this pattern name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || field_index >= record->fields.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[field_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "record pattern names a field more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        binding = ruda_check_child_node_any(field, 0);
        if (binding != NULL) {
            if (!ruda_enum_match_collect_simple_binding(
                    function,
                    binding,
                    field_declaration->type,
                    bindings
                )) {
                goto cleanup;
            }
        } else if (!ruda_enum_match_add_binding(
            function,
            bindings,
            field_name,
            field_declaration->type
        )) {
            goto cleanup;
        }
        seen[field_index] = 1;
        supplied += 1;
    }
    if (supplied != record->fields.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "record pattern omits a required field",
            named->start,
            named->end
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_match_collect_alternatives(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *patterns
) {
    RudaVector pending;
    bool initialized = false;
    bool accepted = false;
    memset(&pending, 0, sizeof(pending));
    if (pattern == NULL || patterns == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &pending,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    if (!ruda_vector_push(&pending, &pattern)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    while (pending.length != 0) {
        const RudaAstNode *const *slot = ruda_vector_at_const(
            &pending,
            pending.length - 1
        );
        const RudaAstNode *current = slot != NULL ? *slot : NULL;
        pending.length -= 1;
        if (current == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (current->kind == RUDA_SYNTAX_OR_PATTERN) {
            const RudaAstNode *left = ruda_check_child_node_any(current, 0);
            const RudaAstNode *right = ruda_check_child_node_any(current, 1);
            if (left == NULL || right == NULL
                || ruda_check_child_node_any(current, 2) != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            if (!ruda_vector_push(&pending, &right)
                || !ruda_vector_push(&pending, &left)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!ruda_vector_push(patterns, &current)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    accepted = true;

cleanup:
    if (initialized) {
        ruda_vector_dispose(&pending);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_analyze_enum_match_alternative(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckEnumMatchAlternative *alternative
) {
    const RudaAstNode *constructor_name = NULL;
    RudaCheckName name;
    size_t variant_index = 0;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    if (enum_declaration == NULL || pattern == NULL || bindings == NULL
        || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        alternative->is_catch_all = true;
    } else if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_enum_match_name(function, pattern, &name)) {
            return false;
        }
        variant = ruda_checker_find_enum_variant(
            function->checker,
            enum_declaration,
            name,
            &variant_index
        );
        if (variant == NULL) {
            alternative->is_catch_all = true;
            alternative->has_catch_binding = true;
            alternative->catch_binding = name;
            if (!ruda_enum_match_add_binding(
                    function,
                    bindings,
                    name,
                    enum_declaration->type
                )) {
                return false;
            }
        } else if (variant->kind != RUDA_CHECKED_ENUM_VARIANT_UNIT) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "enum pattern requires its declared payload shape",
                pattern->start,
                pattern->end
            );
            return false;
        }
    } else if (pattern->kind == RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        constructor_name = ruda_check_child_node(
            pattern,
            RUDA_SYNTAX_NAME_PATTERN,
            0
        );
        if (!ruda_enum_match_name(function, constructor_name, &name)) {
            return false;
        }
        variant = ruda_checker_find_enum_variant(
            function->checker,
            enum_declaration,
            name,
            &variant_index
        );
        if (variant == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
                "enum type has no variant with this pattern name",
                name.start,
                name.end
            );
            return false;
        }
        if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
            if (ruda_check_child_node(
                    pattern,
                    RUDA_SYNTAX_POSITIONAL_PATTERN,
                    0
                ) == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "enum pattern payload shape does not match its variant",
                    pattern->start,
                    pattern->end
                );
                return false;
            }
            if (!ruda_enum_match_validate_positional(
                    function,
                    pattern,
                    variant,
                    bindings
                )) {
                return false;
            }
        } else if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
            if (ruda_check_child_node(
                    pattern,
                    RUDA_SYNTAX_NAMED_PATTERN,
                    0
                ) == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "enum pattern payload shape does not match its variant",
                    pattern->start,
                    pattern->end
                );
                return false;
            }
            if (!ruda_enum_match_validate_named(
                    function,
                    pattern,
                    variant,
                    bindings
                )) {
                return false;
            }
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "unit enum variant pattern must not have a payload",
                pattern->start,
                pattern->end
            );
            return false;
        }
    } else {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (!alternative->is_catch_all) {
        alternative->variant = variant;
        alternative->variant_index = variant_index;
    }
    return true;
}

static bool ruda_enum_match_bindings_equal(
    const RudaFunctionChecker *function,
    const RudaVector *expected,
    const RudaVector *actual
) {
    size_t expected_index = 0;
    if (expected == NULL || actual == NULL
        || expected->length != actual->length) {
        return false;
    }
    for (expected_index = 0; expected_index < expected->length;
        expected_index += 1) {
        const RudaCheckEnumMatchBinding *left = ruda_vector_at_const(
            expected,
            expected_index
        );
        size_t actual_index = 0;
        bool found = false;
        if (left == NULL) {
            return false;
        }
        for (actual_index = 0; actual_index < actual->length;
            actual_index += 1) {
            const RudaCheckEnumMatchBinding *right = ruda_vector_at_const(
                actual,
                actual_index
            );
            if (right != NULL && ruda_check_names_equal(
                    function->checker,
                    left->name,
                    right->name
                )) {
                if (left->type != right->type) {
                    return false;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

static bool ruda_enum_match_alternative_redundant(
    const RudaCheckEnumMatchArm *arm,
    const RudaCheckEnumMatchAlternative *alternative
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const RudaCheckEnumMatchAlternative *earlier = ruda_vector_at_const(
            &arm->alternatives,
            index
        );
        if (earlier != NULL && (earlier->is_catch_all
                || (!alternative->is_catch_all
                    && earlier->variant_index
                        == alternative->variant_index))) {
            return true;
        }
    }
    return false;
}

static bool ruda_match_arm_body_parts(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    const RudaAstNode **guard,
    const RudaAstNode **body,
    bool *body_is_block
) {
    const RudaAstNode *block = NULL;
    bool guarded = false;
    if (arm_node == NULL || guard == NULL || body == NULL
        || body_is_block == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *guard = NULL;
    *body = NULL;
    *body_is_block = false;
    guarded = ruda_check_child_token(
        arm_node,
        RUDA_TOKEN_KW_IF,
        0
    ) != NULL;
    block = ruda_check_child_node(arm_node, RUDA_SYNTAX_BLOCK, 0);
    if (guarded) {
        *guard = ruda_check_child_node(
            arm_node,
            RUDA_SYNTAX_EXPRESSION,
            0
        );
    }
    if (block != NULL) {
        *body = block;
        *body_is_block = true;
    } else {
        *body = ruda_check_child_node(
            arm_node,
            RUDA_SYNTAX_EXPRESSION,
            guarded ? 1 : 0
        );
    }
    if (*body == NULL || (guarded && *guard == NULL)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    return true;
}

static bool ruda_analyze_enum_match_arm(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *arm_node,
    unsigned char *seen_variants,
    bool *covered,
    RudaCheckEnumMatchArm *arm
) {
    const RudaAstNode *pattern_wrapper = ruda_check_child_node(
        arm_node,
        RUDA_SYNTAX_PATTERN,
        0
    );
    const RudaAstNode *pattern = ruda_check_child_node_any(
        pattern_wrapper,
        0
    );
    RudaVector pattern_nodes;
    RudaVector alternative_bindings;
    size_t alternative_index = 0;
    bool pattern_nodes_initialized = false;
    bool alternative_bindings_initialized = false;
    bool accepted = false;
    memset(&pattern_nodes, 0, sizeof(pattern_nodes));
    memset(&alternative_bindings, 0, sizeof(alternative_bindings));
    if (arm == NULL || enum_declaration == NULL || arm_node == NULL
        || pattern_wrapper == NULL || pattern == NULL || covered == NULL
        || (seen_variants == NULL && enum_declaration->variants.length != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchAlternative)
        ) || !ruda_vector_init(
            &arm->bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        ) || !ruda_vector_init(
            &pattern_nodes,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        ) || !ruda_vector_init(
            &alternative_bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    pattern_nodes_initialized = true;
    alternative_bindings_initialized = true;
    arm->node = arm_node;
    arm->pattern = pattern;
    if (!ruda_match_arm_body_parts(
            function,
            arm_node,
            &arm->guard,
            &arm->body,
            &arm->body_is_block
        )) {
        goto cleanup;
    }
    if (!ruda_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (pattern_nodes.length == 0
            && !ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        goto cleanup;
    }
    for (alternative_index = 0;
        alternative_index < pattern_nodes.length;
        alternative_index += 1) {
        const RudaAstNode *const *alternative_pattern = ruda_vector_at_const(
            &pattern_nodes,
            alternative_index
        );
        RudaCheckEnumMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_analyze_enum_match_alternative(
                function,
                enum_declaration,
                *alternative_pattern,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (!alternative.is_catch_all
            && (seen_variants == NULL
                || alternative.variant_index
                    >= enum_declaration->variants.length)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const RudaCheckEnumMatchAlternative *subsumed =
                ruda_vector_at_const(&arm->alternatives, 0);
            if (subsumed == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is subsumed by a catch-all in the same OR pattern",
                subsumed->pattern->start,
                subsumed->pattern->end
            );
            goto cleanup;
        }
        if (*covered || (!alternative.is_catch_all
                && seen_variants[alternative.variant_index] != 0)
            || ruda_enum_match_alternative_redundant(
                arm,
                &alternative
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is unreachable after earlier patterns",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (alternative_index == 0) {
            if (!ruda_vector_append(
                    &arm->bindings,
                    alternative_bindings.data,
                    alternative_bindings.length
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!ruda_enum_match_bindings_equal(
                function,
                &arm->bindings,
                &alternative_bindings
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "OR pattern alternatives must bind the same names with identical types",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!ruda_vector_push(&arm->alternatives, &alternative)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (alternative->is_catch_all) {
                *covered = true;
            } else if (seen_variants != NULL
                && alternative->variant_index
                    < enum_declaration->variants.length) {
                seen_variants[alternative->variant_index] = 1;
            } else {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
        }
    }
    accepted = true;

cleanup:
    if (alternative_bindings_initialized) {
        ruda_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        ruda_vector_dispose(&pattern_nodes);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_declare_match_bindings(
    RudaFunctionChecker *function,
    const RudaVector *bindings
) {
    size_t index = 0;
    if (bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < bindings->length; index += 1) {
        const RudaCheckEnumMatchBinding *binding = ruda_vector_at_const(
            bindings,
            index
        );
        RudaCheckLocal *local = NULL;
        if (binding == NULL || ruda_function_find_local_in_current_scope(
                function,
                binding->name
            ) != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_function_add_local(
                function,
                binding->name,
                binding->type,
                false,
                true,
                &local
            ) || local == NULL) {
            return false;
        }
    }
    return true;
}

static bool ruda_assign_match_local(
    RudaFunctionChecker *function,
    RudaCheckName name,
    RudaTypeId type,
    RudaMirRegister source,
    size_t start,
    size_t end
) {
    const RudaCheckLocal *local = ruda_function_find_local_in_current_scope(
        function,
        name
    );
    if (local == NULL || local->type != type) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return ruda_function_emit_copy(
        function,
        local->register_id,
        source,
        start,
        end
    );
}

static bool ruda_bind_enum_match_payload(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister guarded,
    size_t payload_index,
    const RudaAstNode *binding,
    RudaCheckName shorthand
) {
    const RudaCheckEnumPayloadItemDeclaration *payload = ruda_vector_at_const(
        &alternative->variant->payload,
        payload_index
    );
    RudaExpressionValue projected;
    RudaCheckName name = shorthand;
    if (binding != NULL && binding->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (binding != NULL && !ruda_enum_match_name(
            function,
            binding,
            &name
        )) {
        return false;
    }
    if (payload == NULL || name.snapshot == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&projected, 0, sizeof(projected));
    if (!ruda_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_GET_PAYLOAD,
            payload->type,
            guarded,
            enum_id,
            alternative->variant_index,
            payload_index,
            binding != NULL ? binding->start : name.start,
            binding != NULL ? binding->end : name.end,
            &projected
        )) {
        return false;
    }
    return ruda_assign_match_local(
        function,
        name,
        payload->type,
        projected.register_id,
        binding != NULL ? binding->start : name.start,
        binding != NULL ? binding->end : name.end
    );
}

static bool ruda_compile_enum_match_bindings(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister receiver
) {
    if (alternative->is_catch_all) {
        const RudaTypeId *receiver_type = ruda_vector_at_const(
            &function->mir.register_types,
            receiver
        );
        if (receiver_type == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        return !alternative->has_catch_binding
            || ruda_assign_match_local(
                function,
                alternative->catch_binding,
                *receiver_type,
                receiver,
                alternative->pattern->start,
                alternative->pattern->end
            );
    }
    if (alternative->variant == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    if (alternative->variant->kind
        == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        const RudaAstNode *positional = ruda_check_child_node(
            alternative->pattern,
            RUDA_SYNTAX_POSITIONAL_PATTERN,
            0
        );
        const RudaAstNode *list = ruda_check_child_node(
            positional,
            RUDA_SYNTAX_PATTERN_LIST,
            0
        );
        size_t payload_index = 0;
        const RudaAstNode *binding = NULL;
        RudaCheckName no_shorthand;
        memset(&no_shorthand, 0, sizeof(no_shorthand));
        while ((binding = ruda_check_child_node_any(
                list,
                payload_index
            )) != NULL) {
            if (!ruda_bind_enum_match_payload(
                    function,
                    alternative,
                    enum_id,
                    receiver,
                    payload_index,
                    binding,
                    no_shorthand
                )) {
                return false;
            }
            payload_index += 1;
        }
        return payload_index == alternative->variant->payload.length;
    }
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
        const RudaAstNode *named = ruda_check_child_node(
            alternative->pattern,
            RUDA_SYNTAX_NAMED_PATTERN,
            0
        );
        const RudaAstNode *fields = ruda_check_child_node(
            named,
            RUDA_SYNTAX_PATTERN_FIELDS,
            0
        );
        size_t field_ordinal = 0;
        for (;;) {
            const RudaAstNode *field = ruda_check_child_node(
                fields,
                RUDA_SYNTAX_PATTERN_FIELD,
                field_ordinal
            );
            const RudaSyntaxToken *field_token = NULL;
            const RudaAstNode *binding = NULL;
            RudaCheckName field_name;
            size_t payload_index = 0;
            if (field == NULL) {
                break;
            }
            field_ordinal += 1;
            field_token = ruda_check_child_token(
                field,
                RUDA_TOKEN_IDENTIFIER,
                0
            );
            if (field_token == NULL || field_token->synthetic) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            field_name.snapshot = function->checker->ast->snapshot;
            field_name.start = field_token->start;
            field_name.end = field_token->end;
            if (ruda_checker_find_enum_payload(
                    function->checker,
                    alternative->variant,
                    field_name,
                    &payload_index
                ) == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            binding = ruda_check_child_node_any(field, 0);
            if (!ruda_bind_enum_match_payload(
                    function,
                    alternative,
                    enum_id,
                    receiver,
                    payload_index,
                    binding,
                    field_name
                )) {
                return false;
            }
        }
        return field_ordinal == alternative->variant->payload.length;
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
    return false;
}

static bool ruda_compile_record_match_bindings(
    RudaFunctionChecker *function,
    const RudaCheckRecordMatchAlternative *alternative,
    RudaCheckRecordDeclaration *record_declaration,
    RudaMirRecordId record,
    RudaMirRegister receiver
) {
    const RudaAstNode *named = NULL;
    const RudaAstNode *fields = NULL;
    size_t field_ordinal = 0;
    if (alternative == NULL || record_declaration == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!alternative->is_record_pattern) {
        return !alternative->has_catch_binding
            || ruda_assign_match_local(
                function,
                alternative->catch_binding,
                record_declaration->type,
                receiver,
                alternative->pattern->start,
                alternative->pattern->end
            );
    }
    named = ruda_check_child_node(
        alternative->pattern,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    fields = ruda_check_child_node(
        named,
        RUDA_SYNTAX_PATTERN_FIELDS,
        0
    );
    if (named == NULL || fields == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (;;) {
        const RudaAstNode *field = ruda_check_child_node(
            fields,
            RUDA_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const RudaSyntaxToken *field_token = NULL;
        const RudaAstNode *binding = NULL;
        RudaCheckRecordFieldDeclaration *field_declaration = NULL;
        RudaCheckName field_name;
        RudaCheckName binding_name;
        RudaExpressionValue projected;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = ruda_check_child_token(
            field,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        binding = ruda_check_child_node_any(field, 0);
        if (binding != NULL && binding->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
            continue;
        }
        binding_name = field_name;
        if (binding != NULL
            && !ruda_enum_match_name(function, binding, &binding_name)) {
            return false;
        }
        field_declaration = ruda_checker_find_record_field(
            function->checker,
            record_declaration,
            field_name,
            &field_index
        );
        if (field_declaration == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!ruda_function_emit_record_access(
                function,
                RUDA_MIR_RECORD_GET,
                field_declaration->type,
                receiver,
                RUDA_MIR_NO_REGISTER,
                record,
                field_index,
                binding != NULL ? binding->start : field_name.start,
                binding != NULL ? binding->end : field_name.end,
                &projected
            ) || !ruda_assign_match_local(
                function,
                binding_name,
                field_declaration->type,
                projected.register_id,
                binding != NULL ? binding->start : field_name.start,
                binding != NULL ? binding->end : field_name.end
            )) {
            return false;
        }
    }
    return field_ordinal == record_declaration->fields.length;
}

static bool ruda_analyze_bool_match_alternative(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckScalarMatchAlternative *alternative
) {
    const RudaSyntaxToken *literal = NULL;
    RudaCheckName name;
    if (pattern == NULL || bindings == NULL || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        alternative->is_catch_all = true;
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_enum_match_name(function, pattern, &name)
            || !ruda_enum_match_add_binding(
                function,
                bindings,
                name,
                RUDA_TYPE_BOOL
            )) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = true;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_LITERAL_PATTERN) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    literal = ruda_check_first_token(pattern);
    if (literal == NULL || literal->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (literal->kind != RUDA_TOKEN_KW_TRUE
        && literal->kind != RUDA_TOKEN_KW_FALSE) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    alternative->literal = literal->kind == RUDA_TOKEN_KW_TRUE;
    return true;
}

static bool ruda_bool_match_alternative_redundant(
    const RudaCheckScalarMatchArm *arm,
    const RudaCheckScalarMatchAlternative *alternative
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const RudaCheckScalarMatchAlternative *earlier = ruda_vector_at_const(
            &arm->alternatives,
            index
        );
        if (earlier != NULL && (earlier->is_catch_all
                || (!alternative->is_catch_all
                    && earlier->literal == alternative->literal))) {
            return true;
        }
    }
    return false;
}

static bool ruda_analyze_bool_match_arm(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    unsigned char seen[2],
    bool *covered,
    RudaCheckScalarMatchArm *arm
) {
    const RudaAstNode *pattern_wrapper = ruda_check_child_node(
        arm_node,
        RUDA_SYNTAX_PATTERN,
        0
    );
    const RudaAstNode *pattern = ruda_check_child_node_any(
        pattern_wrapper,
        0
    );
    RudaVector pattern_nodes;
    RudaVector alternative_bindings;
    size_t alternative_index = 0;
    bool pattern_nodes_initialized = false;
    bool alternative_bindings_initialized = false;
    bool accepted = false;
    memset(&pattern_nodes, 0, sizeof(pattern_nodes));
    memset(&alternative_bindings, 0, sizeof(alternative_bindings));
    if (arm == NULL || arm_node == NULL || pattern_wrapper == NULL
        || pattern == NULL || seen == NULL || covered == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckScalarMatchAlternative)
        ) || !ruda_vector_init(
            &arm->bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        ) || !ruda_vector_init(
            &pattern_nodes,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        ) || !ruda_vector_init(
            &alternative_bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    pattern_nodes_initialized = true;
    alternative_bindings_initialized = true;
    arm->node = arm_node;
    arm->pattern = pattern;
    if (!ruda_match_arm_body_parts(
            function,
            arm_node,
            &arm->guard,
            &arm->body,
            &arm->body_is_block
        ) || !ruda_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        goto cleanup;
    }
    for (alternative_index = 0;
        alternative_index < pattern_nodes.length;
        alternative_index += 1) {
        const RudaAstNode *const *alternative_pattern = ruda_vector_at_const(
            &pattern_nodes,
            alternative_index
        );
        RudaCheckScalarMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_analyze_bool_match_alternative(
                function,
                *alternative_pattern,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const RudaCheckScalarMatchAlternative *subsumed =
                ruda_vector_at_const(&arm->alternatives, 0);
            if (subsumed == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is subsumed by a catch-all in the same OR pattern",
                subsumed->pattern->start,
                subsumed->pattern->end
            );
            goto cleanup;
        }
        if (*covered || (!alternative.is_catch_all
                && seen[alternative.literal ? 1 : 0] != 0)
            || ruda_bool_match_alternative_redundant(
                arm,
                &alternative
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is unreachable after earlier patterns",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (alternative_index == 0) {
            if (!ruda_vector_append(
                    &arm->bindings,
                    alternative_bindings.data,
                    alternative_bindings.length
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!ruda_enum_match_bindings_equal(
                function,
                &arm->bindings,
                &alternative_bindings
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "OR pattern alternatives must bind the same names with identical types",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!ruda_vector_push(&arm->alternatives, &alternative)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (alternative->is_catch_all) {
                *covered = true;
            } else {
                seen[alternative->literal ? 1 : 0] = 1;
            }
        }
    }
    accepted = true;

cleanup:
    if (alternative_bindings_initialized) {
        ruda_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        ruda_vector_dispose(&pattern_nodes);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_analyze_scalar_match_alternative(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaTypeId scalar_type,
    RudaCheckScalarMatchAlternative *alternative
) {
    const RudaSyntaxToken *literal = NULL;
    RudaCheckName name;
    uint64_t magnitude = 0;
    RudaOwnedBytes decoded;
    RudaStringLiteralStatus decode_status;
    size_t new_string_bytes = 0;
    memset(&decoded, 0, sizeof(decoded));
    if (pattern == NULL || bindings == NULL || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->string_constant = RUDA_MIR_NO_STRING;
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        alternative->is_catch_all = true;
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_enum_match_name(function, pattern, &name)
            || !ruda_enum_match_add_binding(
                function,
                bindings,
                name,
                scalar_type
            )) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = true;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_LITERAL_PATTERN) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    literal = ruda_check_first_token(pattern);
    if (literal == NULL || literal->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64 && literal->kind != RUDA_TOKEN_INTEGER) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "literal pattern type must match the integer scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64 && !ruda_integer_magnitude(
            function->checker,
            literal,
            (uint64_t)INT64_MAX,
            &magnitude
        )) {
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64) {
        alternative->integer = (int64_t)magnitude;
        return true;
    }
    if (scalar_type != RUDA_TYPE_STRING
        || (literal->kind != RUDA_TOKEN_STRING
            && literal->kind != RUDA_TOKEN_RAW_STRING)) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "literal pattern type must match the string scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    decode_status = ruda_string_literal_decode(
        function->checker->source + literal->start,
        literal->end - literal->start,
        literal->kind == RUDA_TOKEN_RAW_STRING,
        function->checker->result->allocator,
        &decoded
    );
    if (decode_status != RUDA_STRING_LITERAL_OK) {
        ruda_checker_fail(
            function->checker,
            decode_status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
                ? RUDA_CHECK_ALLOCATION_FAILED
                : RUDA_CHECK_INTERNAL_ERROR
        );
        return false;
    }
    if (function->mir.string_constants.length
            >= function->mir.options.max_string_constants
        || !ruda_size_add(
            function->mir.string_bytes,
            decoded.length,
            &new_string_bytes
        ) || new_string_bytes > function->mir.options.max_string_bytes) {
        ruda_owned_bytes_dispose(&decoded);
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_string_constant(
            &function->mir,
            decoded.data,
            decoded.length,
            &alternative->string_constant
        )) {
        ruda_owned_bytes_dispose(&decoded);
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    ruda_owned_bytes_dispose(&decoded);
    return true;
}

static bool ruda_scalar_match_key(
    const RudaFunctionChecker *function,
    const RudaCheckScalarMatchAlternative *alternative,
    RudaTypeId scalar_type,
    const void **key,
    size_t *key_length
) {
    const RudaMirStringConstant *string_constant = NULL;
    if (alternative == NULL || alternative->is_catch_all
        || key == NULL || key_length == NULL) {
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64) {
        *key = &alternative->integer;
        *key_length = sizeof(alternative->integer);
        return true;
    }
    string_constant = scalar_type == RUDA_TYPE_STRING
        ? ruda_check_match_string_constant(
            function,
            alternative->string_constant
        )
        : NULL;
    if (string_constant == NULL) {
        return false;
    }
    *key = string_constant->value.data;
    *key_length = string_constant->value.length;
    return true;
}

static bool ruda_scalar_match_alternative_redundant(
    const RudaFunctionChecker *function,
    const RudaCheckScalarMatchArm *arm,
    const RudaCheckScalarMatchAlternative *alternative,
    RudaTypeId scalar_type
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const RudaCheckScalarMatchAlternative *earlier = ruda_vector_at_const(
            &arm->alternatives,
            index
        );
        bool equal = scalar_type == RUDA_TYPE_I64
            ? earlier != NULL
                && earlier->integer == alternative->integer
            : earlier != NULL && ruda_check_match_string_equal(
                function,
                earlier->string_constant,
                alternative->string_constant
            );
        if (earlier != NULL && (earlier->is_catch_all
                || (!alternative->is_catch_all && equal))) {
            return true;
        }
    }
    return false;
}

static bool ruda_analyze_scalar_match_arm(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    RudaMap *seen_literals,
    bool *covered,
    RudaTypeId scalar_type,
    RudaCheckScalarMatchArm *arm
) {
    const RudaAstNode *pattern_wrapper = ruda_check_child_node(
        arm_node,
        RUDA_SYNTAX_PATTERN,
        0
    );
    const RudaAstNode *pattern = ruda_check_child_node_any(
        pattern_wrapper,
        0
    );
    RudaVector pattern_nodes;
    RudaVector alternative_bindings;
    size_t alternative_index = 0;
    bool pattern_nodes_initialized = false;
    bool alternative_bindings_initialized = false;
    bool accepted = false;
    memset(&pattern_nodes, 0, sizeof(pattern_nodes));
    memset(&alternative_bindings, 0, sizeof(alternative_bindings));
    if (arm == NULL || arm_node == NULL || pattern_wrapper == NULL
        || pattern == NULL || seen_literals == NULL || covered == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckScalarMatchAlternative)
        ) || !ruda_vector_init(
            &arm->bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        ) || !ruda_vector_init(
            &pattern_nodes,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        ) || !ruda_vector_init(
            &alternative_bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    pattern_nodes_initialized = true;
    alternative_bindings_initialized = true;
    arm->node = arm_node;
    arm->pattern = pattern;
    if (!ruda_match_arm_body_parts(
            function,
            arm_node,
            &arm->guard,
            &arm->body,
            &arm->body_is_block
        ) || !ruda_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        goto cleanup;
    }
    for (alternative_index = 0;
        alternative_index < pattern_nodes.length;
        alternative_index += 1) {
        const RudaAstNode *const *alternative_pattern = ruda_vector_at_const(
            &pattern_nodes,
            alternative_index
        );
        RudaCheckScalarMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_analyze_scalar_match_alternative(
                function,
                *alternative_pattern,
                &alternative_bindings,
                scalar_type,
                &alternative
            )) {
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const RudaCheckScalarMatchAlternative *subsumed =
                ruda_vector_at_const(&arm->alternatives, 0);
            if (subsumed == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is subsumed by a catch-all in the same OR pattern",
                subsumed->pattern->start,
                subsumed->pattern->end
            );
            goto cleanup;
        }
        {
            const void *key = NULL;
            size_t key_length = 0;
            bool has_key = alternative.is_catch_all
                || ruda_scalar_match_key(
                    function,
                    &alternative,
                    scalar_type,
                    &key,
                    &key_length
                );
            if (!has_key) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (*covered || (!alternative.is_catch_all
                    && ruda_map_get_const(
                        seen_literals,
                        key,
                        key_length
                    ) != NULL)
                || ruda_scalar_match_alternative_redundant(
                    function,
                    arm,
                    &alternative,
                    scalar_type
                )) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                    "match alternative is unreachable after earlier patterns",
                    alternative.pattern->start,
                    alternative.pattern->end
                );
                goto cleanup;
            }
        }
        if (alternative_index == 0) {
            if (!ruda_vector_append(
                    &arm->bindings,
                    alternative_bindings.data,
                    alternative_bindings.length
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!ruda_enum_match_bindings_equal(
                function,
                &arm->bindings,
                &alternative_bindings
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "OR pattern alternatives must bind the same names with identical types",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!ruda_vector_push(&arm->alternatives, &alternative)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            unsigned char present = 1;
            const void *key = NULL;
            size_t key_length = 0;
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (alternative->is_catch_all) {
                *covered = true;
            } else if (!ruda_scalar_match_key(
                    function,
                    alternative,
                    scalar_type,
                    &key,
                    &key_length
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            } else if (!ruda_map_put(
                    seen_literals,
                    key,
                    key_length,
                    &present
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        }
    }
    accepted = true;

cleanup:
    if (alternative_bindings_initialized) {
        ruda_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        ruda_vector_dispose(&pattern_nodes);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_resolve_record_match_pattern(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaCheckRecordDeclaration *scrutinee_record,
    RudaVector *bindings,
    RudaCheckRecordMatchAlternative *alternative
) {
    const RudaAstNode *constructor_name = NULL;
    const RudaAstNode *named = NULL;
    RudaCheckRecordDeclaration *pattern_record = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaCheckName name;
    if (pattern == NULL || scrutinee_record == NULL || bindings == NULL
        || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_enum_match_name(function, pattern, &name)
            || !ruda_enum_match_add_binding(
                function,
                bindings,
                name,
                scrutinee_record->type
            )) {
            return false;
        }
        alternative->has_catch_binding = true;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "pattern does not match the nominal record scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    constructor_name = ruda_check_child_node(
        pattern,
        RUDA_SYNTAX_NAME_PATTERN,
        0
    );
    named = ruda_check_child_node(
        pattern,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    if (!ruda_enum_match_name(function, constructor_name, &name)) {
        return false;
    }
    pattern_record = ruda_checker_find_record(function->checker, name);
    if (pattern_record == NULL) {
        alias = ruda_checker_find_type_alias(function->checker, name);
        if (alias != NULL && ruda_evaluate_type_alias(
                function->checker,
                alias,
                name.start,
                name.end
            )) {
            pattern_record = ruda_checker_find_record_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (pattern_record == NULL) {
        if (ruda_checker_find_top_level(function->checker, name) != NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "record pattern name does not denote a record type",
                name.start,
                name.end
            );
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
                "unknown record pattern type",
                name.start,
                name.end
            );
        }
        return false;
    }
    if (pattern_record->type != scrutinee_record->type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "record pattern type differs from the scrutinee type",
            name.start,
            name.end
        );
        return false;
    }
    if (named == NULL || ruda_check_child_node(
            pattern,
            RUDA_SYNTAX_POSITIONAL_PATTERN,
            0
        ) != NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "record pattern requires its named field shape",
            pattern->start,
            pattern->end
        );
        return false;
    }
    if (!ruda_record_match_validate_named(
            function,
            pattern,
            pattern_record,
            bindings
        )) {
        return false;
    }
    alternative->is_record_pattern = true;
    return true;
}

static bool ruda_analyze_record_match_arm(
    RudaFunctionChecker *function,
    RudaCheckRecordDeclaration *record_declaration,
    const RudaAstNode *arm_node,
    bool *covered,
    RudaCheckRecordMatchArm *arm
) {
    const RudaAstNode *pattern_wrapper = ruda_check_child_node(
        arm_node,
        RUDA_SYNTAX_PATTERN,
        0
    );
    const RudaAstNode *pattern = ruda_check_child_node_any(
        pattern_wrapper,
        0
    );
    RudaVector pattern_nodes;
    RudaVector alternative_bindings;
    size_t alternative_index = 0;
    bool pattern_nodes_initialized = false;
    bool alternative_bindings_initialized = false;
    bool accepted = false;
    memset(&pattern_nodes, 0, sizeof(pattern_nodes));
    memset(&alternative_bindings, 0, sizeof(alternative_bindings));
    if (record_declaration == NULL || arm_node == NULL || covered == NULL
        || arm == NULL || pattern_wrapper == NULL || pattern == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckRecordMatchAlternative)
        ) || !ruda_vector_init(
            &arm->bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        ) || !ruda_vector_init(
            &pattern_nodes,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        ) || !ruda_vector_init(
            &alternative_bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    pattern_nodes_initialized = true;
    alternative_bindings_initialized = true;
    arm->node = arm_node;
    arm->pattern = pattern;
    if (!ruda_match_arm_body_parts(
            function,
            arm_node,
            &arm->guard,
            &arm->body,
            &arm->body_is_block
        ) || !ruda_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        goto cleanup;
    }
    for (alternative_index = 0;
        alternative_index < pattern_nodes.length;
        alternative_index += 1) {
        const RudaAstNode *const *alternative_pattern = ruda_vector_at_const(
            &pattern_nodes,
            alternative_index
        );
        RudaCheckRecordMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_resolve_record_match_pattern(
                function,
                *alternative_pattern,
                record_declaration,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (*covered || arm->alternatives.length != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "record pattern is unreachable after an irrefutable pattern",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!ruda_vector_append(
                &arm->bindings,
                alternative_bindings.data,
                alternative_bindings.length
            ) || !ruda_vector_push(&arm->alternatives, &alternative)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        *covered = true;
    }
    accepted = true;

cleanup:
    if (alternative_bindings_initialized) {
        ruda_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        ruda_vector_dispose(&pattern_nodes);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_yield(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaSyntaxToken *keyword = ruda_check_first_token(statement);
    const RudaAstNode *expression = ruda_check_child_node(
        statement,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaMatchYieldContext *context = function->match_yield;
    RudaExpressionValue value;
    if (keyword == NULL || keyword->kind != RUDA_TOKEN_KW_YIELD
        || expression == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (context == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_YIELD_OUTSIDE_MATCH_ARM,
            "yield is only valid inside a block match arm",
            keyword->start,
            keyword->end
        );
        return false;
    }
    if (context->match_type == NULL || context->target == NULL
        || context->join == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function,
            expression,
            *context->match_type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type == RUDA_TYPE_VOID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (*context->match_type == RUDA_TYPE_INVALID) {
        *context->match_type = value.type;
    }
    if (value.type != *context->match_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "yielded match value type differs from the other arms",
            expression->start,
            expression->end
        );
        return false;
    }
    if (*context->join == RUDA_MIR_NO_BLOCK
        && !ruda_function_add_block(function, context->join)) {
        return false;
    }
    if (*context->target == RUDA_MIR_NO_REGISTER
        && !ruda_function_add_register(
            function,
            *context->match_type,
            context->target
        )) {
        return false;
    }
    if (*context->target == RUDA_MIR_NO_REGISTER
        || !ruda_function_emit_copy(
            function,
            *context->target,
            value.register_id,
            expression->start,
            expression->end
        ) || !ruda_function_set_goto(
            function,
            function->block,
            *context->join,
            statement->start,
            statement->end
        )) {
        return false;
    }
    context->yielded = true;
    function->terminated = true;
    return true;
}

static bool ruda_compile_match_arm_body(
    RudaFunctionChecker *function,
    const RudaAstNode *body,
    bool body_is_block,
    RudaMirBlockId *join,
    RudaTypeId *match_type,
    RudaMirRegister *target
) {
    RudaExpressionValue value;
    RudaMatchYieldContext context;
    RudaMatchYieldContext *previous_context = NULL;
    bool compiled = false;
    if (body == NULL || join == NULL || match_type == NULL
        || target == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (body_is_block) {
        memset(&context, 0, sizeof(context));
        context.join = join;
        context.match_type = match_type;
        context.target = target;
        previous_context = function->match_yield;
        function->match_yield = &context;
        compiled = ruda_compile_block(function, body);
        function->match_yield = previous_context;
        if (!compiled) {
            return false;
        }
        if (!function->terminated) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_MISSING_MATCH_YIELD,
                "reachable block match arm path does not yield a value",
                body->start,
                body->end
            );
            return false;
        }
        return true;
    }
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function,
            body,
            *match_type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type == RUDA_TYPE_VOID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (*match_type == RUDA_TYPE_INVALID) {
        *match_type = value.type;
    }
    if (value.type != *match_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "match arm result type differs from the other arms",
            body->start,
            body->end
        );
        return false;
    }
    if (*join == RUDA_MIR_NO_BLOCK
        && !ruda_function_add_block(function, join)) {
        return false;
    }
    if (*target == RUDA_MIR_NO_REGISTER
        && !ruda_function_add_register(function, *match_type, target)) {
        return false;
    }
    return *target != RUDA_MIR_NO_REGISTER
        && ruda_function_emit_copy(
            function,
            *target,
            value.register_id,
            body->start,
            body->end
        ) && ruda_function_set_goto(
            function,
            function->block,
            *join,
            body->end,
            body->end
        );
}

static bool ruda_compile_match_guard(
    RudaFunctionChecker *function,
    const RudaAstNode *guard,
    RudaExpressionValue *result,
    bool *never
) {
    if (guard == NULL || result == NULL || never == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *never = false;
    if (!ruda_compile_expression(
            function,
            guard,
            RUDA_TYPE_INVALID,
            result
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, result)) {
        *never = true;
        return true;
    }
    if (result->type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "match guard must have type bool",
            guard->start,
            guard->end
        );
        return false;
    }
    return true;
}

static bool ruda_compile_bool_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaExpressionValue *result
) {
    RudaCheckScalarMatchArm *arms = NULL;
    unsigned char seen[2] = {0, 0};
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId test_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    if (node == NULL || result == NULL || !scrutinee.valid
        || scrutinee.type != RUDA_TYPE_BOOL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    while (ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        (void)ruda_add_bool_match_witness_diagnostic(
            function,
            node,
            seen,
            NULL,
            0
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckScalarMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_analyze_bool_match_arm(
                function,
                arm_node,
                seen,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (!covered && (seen[0] == 0 || seen[1] == 0)) {
        (void)ruda_add_bool_match_witness_diagnostic(
            function,
            node,
            seen,
            arms,
            arm_count
        );
        goto cleanup;
    }
    test_block = function->block;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        RudaCheckScalarMatchArm *arm = &arms[arm_index];
        RudaMirBlockId selected_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        size_t alternative_index = 0;
        bool last = arm_index + 1 == arm_count;
        bool has_catch_all = false;
        bool needs_next = false;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            has_catch_all = has_catch_all || alternative->is_catch_all;
        }
        needs_next = (!last && !has_catch_all) || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !ruda_function_add_block(function, &selected_block)
            || (needs_next
                && !ruda_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto bool_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_declare_match_bindings(function, &arm->bindings)) {
            goto bool_arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            RudaMirBlockId alternative_block = RUDA_MIR_NO_BLOCK;
            RudaMirBlockId alternative_next = RUDA_MIR_NO_BLOCK;
            RudaExpressionValue literal_value;
            RudaExpressionValue matched;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&literal_value, 0, sizeof(literal_value));
            memset(&matched, 0, sizeof(matched));
            if (alternative == NULL
                || !ruda_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto bool_arm_cleanup;
            }
            if (!alternative_last) {
                if (!ruda_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto bool_arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!ruda_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto bool_arm_cleanup;
                }
            } else if (alternative_next == RUDA_MIR_NO_BLOCK
                || !ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_BOOL,
                    RUDA_TYPE_BOOL,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    0,
                    alternative->literal,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &literal_value
                ) || !ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_EQUAL_BOOL,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    literal_value.register_id,
                    0,
                    false,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !ruda_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto bool_arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->has_catch_binding
                && !ruda_assign_match_local(
                    function,
                    alternative->catch_binding,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                goto bool_arm_cleanup;
            }
            if (!ruda_function_set_goto(
                    function,
                    function->block,
                    selected_block,
                    alternative->pattern->end,
                    alternative->pattern->end
                )) {
                goto bool_arm_cleanup;
            }
            if (!alternative_last) {
                function->block = alternative_next;
                function->terminated = false;
            }
        }
        function->block = selected_block;
        function->terminated = false;
        if (arm->guard != NULL) {
            if (!ruda_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto bool_arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto bool_arm_cleanup;
            }
            if (next_block == RUDA_MIR_NO_BLOCK
                || !ruda_function_add_block(function, &body_block)
                || !ruda_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto bool_arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!ruda_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto bool_arm_cleanup;
        }
        arm_compiled = true;

bool_arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (next_block != RUDA_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_function_emit_string_constant(
    RudaFunctionChecker *function,
    RudaMirStringId string_constant,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (ruda_check_match_string_constant(function, string_constant) == NULL
        || result == NULL
        || !ruda_function_add_register(function, RUDA_TYPE_STRING, &target)) {
        if (result == NULL
            || ruda_check_match_string_constant(
                function,
                string_constant
            ) == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INTERNAL_ERROR
            );
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_STRING;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.string_constant = string_constant;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_compile_scalar_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaTypeId scalar_type,
    RudaExpressionValue *result
) {
    static const uint64_t seen_key0 = UINT64_C(0x727564612d693634);
    static const uint64_t seen_key1 = UINT64_C(0x2d6d617463682d31);
    RudaCheckScalarMatchArm *arms = NULL;
    RudaMap seen_literals;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId test_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool seen_initialized = false;
    bool covered = false;
    bool accepted = false;
    memset(&seen_literals, 0, sizeof(seen_literals));
    if (node == NULL || result == NULL || !scrutinee.valid
        || scrutinee.type != scalar_type
        || (scalar_type != RUDA_TYPE_I64
            && scalar_type != RUDA_TYPE_STRING)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_map_init(
            &seen_literals,
            function->checker->result->allocator,
            sizeof(unsigned char),
            seen_key0,
            seen_key1
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    seen_initialized = true;
    while (ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        if (scalar_type == RUDA_TYPE_I64) {
            (void)ruda_add_i64_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                NULL,
                0
            );
        } else {
            (void)ruda_add_string_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                NULL,
                0
            );
        }
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckScalarMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_analyze_scalar_match_arm(
                function,
                arm_node,
                &seen_literals,
                &covered,
                scalar_type,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (!covered) {
        if (scalar_type == RUDA_TYPE_I64) {
            (void)ruda_add_i64_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                arms,
                arm_count
            );
        } else {
            (void)ruda_add_string_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                arms,
                arm_count
            );
        }
        goto cleanup;
    }
    test_block = function->block;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        RudaCheckScalarMatchArm *arm = &arms[arm_index];
        RudaMirBlockId selected_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        size_t alternative_index = 0;
        bool last = arm_index + 1 == arm_count;
        bool has_catch_all = false;
        bool needs_next = false;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            has_catch_all = has_catch_all || alternative->is_catch_all;
        }
        needs_next = (!last && !has_catch_all) || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !ruda_function_add_block(function, &selected_block)
            || (needs_next
                && !ruda_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto i64_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_declare_match_bindings(function, &arm->bindings)) {
            goto i64_arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            RudaMirBlockId alternative_block = RUDA_MIR_NO_BLOCK;
            RudaMirBlockId alternative_next = RUDA_MIR_NO_BLOCK;
            RudaExpressionValue literal_value;
            RudaExpressionValue matched;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&literal_value, 0, sizeof(literal_value));
            memset(&matched, 0, sizeof(matched));
            if (alternative == NULL
                || !ruda_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            if (!alternative_last) {
                if (!ruda_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto i64_arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!ruda_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto i64_arm_cleanup;
                }
            } else if (alternative_next == RUDA_MIR_NO_BLOCK
                || (scalar_type == RUDA_TYPE_I64
                    ? !ruda_function_emit_instruction(
                        function,
                        RUDA_MIR_CONSTANT_I64,
                        RUDA_TYPE_I64,
                        RUDA_MIR_NO_REGISTER,
                        RUDA_MIR_NO_REGISTER,
                        alternative->integer,
                        false,
                        alternative->pattern->start,
                        alternative->pattern->end,
                        &literal_value
                    )
                    : !ruda_function_emit_string_constant(
                        function,
                        alternative->string_constant,
                        alternative->pattern->start,
                        alternative->pattern->end,
                        &literal_value
                    )) || !ruda_function_emit_instruction(
                    function,
                    scalar_type == RUDA_TYPE_I64
                        ? RUDA_MIR_EQUAL_I64
                        : RUDA_MIR_EQUAL_STRING,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    literal_value.register_id,
                    0,
                    false,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !ruda_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->has_catch_binding
                && !ruda_assign_match_local(
                    function,
                    alternative->catch_binding,
                    scalar_type,
                    scrutinee.register_id,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                goto i64_arm_cleanup;
            }
            if (!ruda_function_set_goto(
                    function,
                    function->block,
                    selected_block,
                    alternative->pattern->end,
                    alternative->pattern->end
                )) {
                goto i64_arm_cleanup;
            }
            if (!alternative_last) {
                function->block = alternative_next;
                function->terminated = false;
            }
        }
        function->block = selected_block;
        function->terminated = false;
        if (arm->guard != NULL) {
            if (!ruda_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto i64_arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto i64_arm_cleanup;
            }
            if (next_block == RUDA_MIR_NO_BLOCK
                || !ruda_function_add_block(function, &body_block)
                || !ruda_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!ruda_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto i64_arm_cleanup;
        }
        arm_compiled = true;

i64_arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (next_block != RUDA_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    if (seen_initialized) {
        ruda_map_dispose(&seen_literals);
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_record_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaCheckRecordDeclaration *record_declaration,
    RudaExpressionValue *result
) {
    RudaCheckRecordMatchArm *arms = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    if (node == NULL || result == NULL || record_declaration == NULL
        || !scrutinee.valid
        || scrutinee.type != record_declaration->type) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    while (ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        (void)ruda_add_record_match_witness_diagnostic(
            function,
            node,
            record_declaration,
            false
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckRecordMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_analyze_record_match_arm(
                function,
                record_declaration,
                arm_node,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (!covered) {
        (void)ruda_add_record_match_witness_diagnostic(
            function,
            node,
            record_declaration,
            true
        );
        goto cleanup;
    }
    if (!ruda_function_record_layout(
            function,
            record_declaration,
            &record
        )) {
        goto cleanup;
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        RudaCheckRecordMatchArm *arm = &arms[arm_index];
        const RudaCheckRecordMatchAlternative *alternative =
            ruda_vector_at_const(&arm->alternatives, 0);
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
        function->terminated = false;
        if (alternative == NULL || arm->alternatives.length != 1
            || (arm->guard != NULL
                && !ruda_function_add_block(function, &next_block))) {
            if (alternative == NULL || arm->alternatives.length != 1) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto record_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_declare_match_bindings(function, &arm->bindings)
            || !ruda_compile_record_match_bindings(
                function,
                alternative,
                record_declaration,
                record,
                scrutinee.register_id
            )) {
            goto record_arm_cleanup;
        }
        if (arm->guard != NULL) {
            if (!ruda_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto record_arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto record_arm_cleanup;
            }
            if (!ruda_function_add_block(function, &body_block)
                || !ruda_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                goto record_arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!ruda_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto record_arm_cleanup;
        }
        arm_compiled = true;

record_arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (arm->guard != NULL) {
            function->block = next_block;
            function->terminated = false;
            next_block = RUDA_MIR_NO_BLOCK;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_enum_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *scrutinee_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaExpressionValue scrutinee;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumMatchArm *arms = NULL;
    unsigned char *seen_variants = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    size_t seen_count = 0;
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId test_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    memset(&scrutinee, 0, sizeof(scrutinee));
    if (function->constant_evaluation) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (scrutinee_node == NULL || result == NULL
        || !ruda_compile_expression(
            function,
            scrutinee_node,
            RUDA_TYPE_INVALID,
            &scrutinee
        )) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &scrutinee)) {
        *result = scrutinee;
        accepted = true;
        goto cleanup;
    }
    if (scrutinee.type == RUDA_TYPE_BOOL) {
        return ruda_compile_bool_match(
            function,
            node,
            expected,
            scrutinee,
            result
        );
    }
    if (scrutinee.type == RUDA_TYPE_I64) {
        return ruda_compile_scalar_match(
            function,
            node,
            expected,
            scrutinee,
            RUDA_TYPE_I64,
            result
        );
    }
    if (scrutinee.type == RUDA_TYPE_STRING) {
        return ruda_compile_scalar_match(
            function,
            node,
            expected,
            scrutinee,
            RUDA_TYPE_STRING,
            result
        );
    }
    {
        RudaCheckRecordDeclaration *record_declaration =
            ruda_checker_find_record_by_type(
                function->checker,
                scrutinee.type
            );
        if (record_declaration != NULL) {
            return ruda_compile_record_match(
                function,
                node,
                expected,
                scrutinee,
                record_declaration,
                result
            );
        }
    }
    enum_declaration = ruda_checker_find_enum_by_type(
        function->checker,
        scrutinee.type
    );
    if (enum_declaration == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    while (ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        if (enum_declaration->variants.length == 0) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            goto cleanup;
        }
        seen_variants = ruda_allocate(
            &function->checker->result->allocator,
            enum_declaration->variants.length
        );
        if (seen_variants == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen_variants, 0, enum_declaration->variants.length);
        (void)ruda_add_enum_match_witness_diagnostic(
            function,
            node,
            enum_declaration,
            seen_variants,
            NULL,
            0
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckEnumMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    if (enum_declaration->variants.length != 0) {
        seen_variants = ruda_allocate(
            &function->checker->result->allocator,
            enum_declaration->variants.length
        );
    }
    if (enum_declaration->variants.length != 0
        && seen_variants == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (seen_variants != NULL) {
        memset(seen_variants, 0, enum_declaration->variants.length);
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_analyze_enum_match_arm(
                function,
                enum_declaration,
                arm_node,
                seen_variants,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (enum_declaration->variants.length != 0 && seen_variants == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    for (arm_index = 0; arm_index < enum_declaration->variants.length;
        arm_index += 1) {
        if (seen_variants[arm_index] != 0) {
            seen_count += 1;
        }
    }
    if (!covered && seen_count != enum_declaration->variants.length) {
        (void)ruda_add_enum_match_witness_diagnostic(
            function,
            node,
            enum_declaration,
            seen_variants,
            arms,
            arm_count
        );
        goto cleanup;
    }
    if (!ruda_function_enum_layout(
            function,
            enum_declaration,
            &enum_id
        )) {
        goto cleanup;
    }
    test_block = function->block;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        RudaCheckEnumMatchArm *arm = &arms[arm_index];
        RudaMirBlockId selected_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        size_t alternative_index = 0;
        bool last = arm_index + 1 == arm_count;
        bool has_catch_all = false;
        bool needs_next = false;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            has_catch_all = has_catch_all || alternative->is_catch_all;
        }
        needs_next = (!last && !has_catch_all) || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !ruda_function_add_block(function, &selected_block)
            || (needs_next
                && !ruda_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_declare_match_bindings(function, &arm->bindings)) {
            goto arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            RudaMirBlockId alternative_block = RUDA_MIR_NO_BLOCK;
            RudaMirBlockId alternative_next = RUDA_MIR_NO_BLOCK;
            RudaExpressionValue matched;
            RudaExpressionValue guarded;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&matched, 0, sizeof(matched));
            memset(&guarded, 0, sizeof(guarded));
            if (alternative == NULL
                || !ruda_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            if (!alternative_last) {
                if (!ruda_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!ruda_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto arm_cleanup;
                }
            } else if (alternative_next == RUDA_MIR_NO_BLOCK
                || !ruda_function_emit_enum_access(
                    function,
                    RUDA_MIR_ENUM_IS_VARIANT,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    enum_id,
                    alternative->variant_index,
                    0,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !ruda_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->is_catch_all) {
                guarded = scrutinee;
            } else if (!ruda_function_emit_enum_access(
                    function,
                    RUDA_MIR_ENUM_REQUIRE_VARIANT,
                    scrutinee.type,
                    scrutinee.register_id,
                    enum_id,
                    alternative->variant_index,
                    0,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &guarded
                )) {
                goto arm_cleanup;
            }
            if (!ruda_compile_enum_match_bindings(
                    function,
                    alternative,
                    enum_id,
                    guarded.register_id
                ) || !ruda_function_set_goto(
                    function,
                    function->block,
                    selected_block,
                    alternative->pattern->end,
                    alternative->pattern->end
                )) {
                goto arm_cleanup;
            }
            if (!alternative_last) {
                function->block = alternative_next;
                function->terminated = false;
            }
        }
        function->block = selected_block;
        function->terminated = false;
        if (arm->guard != NULL) {
            if (!ruda_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto arm_cleanup;
            }
            if (next_block == RUDA_MIR_NO_BLOCK
                || !ruda_function_add_block(function, &body_block)
                || !ruda_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!ruda_compile_match_arm_body(
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
        if (next_block != RUDA_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        seen_variants,
        enum_declaration != NULL ? enum_declaration->variants.length : 0
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_binding(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaSyntaxToken *discard = ruda_check_child_token(
        node,
        RUDA_TOKEN_UNDERSCORE,
        0
    );
    const RudaAstNode *type_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    const RudaAstNode *expression = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaCheckName name;
    RudaTypeId declared_type = RUDA_TYPE_INVALID;
    RudaTypeId local_type = RUDA_TYPE_INVALID;
    RudaExpressionValue value;
    RudaCheckLocal *local = NULL;
    size_t diagnostics_before = function->checker->result->diagnostics.length;
    bool has_value = false;
    memset(&value, 0, sizeof(value));
    if (discard != NULL) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (ruda_function_find_local_in_current_scope(function, name) != NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            name.start,
            name.end
        );
        return false;
    }
    if (type_node != NULL
        && !ruda_check_exact_type(
            function->checker,
            type_node,
            &declared_type
        ) && ruda_checker_has_failed(function->checker)) {
        return false;
    }
    if (expression != NULL) {
        has_value = ruda_compile_expression(
            function,
            expression,
            declared_type,
            &value
        );
        if (!has_value && ruda_checker_has_failed(function->checker)) {
            return false;
        }
        if (has_value && ruda_expression_is_never(function, &value)) {
            return true;
        }
    }
    if (declared_type == RUDA_TYPE_INVALID && expression == NULL) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    local_type = declared_type != RUDA_TYPE_INVALID
        ? declared_type
        : value.type;
    if (has_value && declared_type != RUDA_TYPE_INVALID
        && value.type != declared_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "initializer type does not match the binding type",
            expression->start,
            expression->end
        );
        has_value = false;
    }
    if (local_type == RUDA_TYPE_INVALID) {
        return false;
    }
    if (!ruda_function_add_local(
            function,
            name,
            local_type,
            node->kind == RUDA_SYNTAX_LET_DECL,
            expression != NULL,
            &local
        )) {
        return false;
    }
    if (has_value && local != NULL) {
        return ruda_function_emit_copy(
            function,
            local->register_id,
            value.register_id,
            expression->start,
            expression->end
        );
    }
    return function->checker->result->diagnostics.length > diagnostics_before
        || expression == NULL;
}

static bool ruda_compound_assignment_kind(
    RudaTypeId type,
    RudaTokenKind operator_kind,
    RudaMirInstructionKind *kind
) {
    if (kind == NULL) {
        return false;
    }
    if (type == RUDA_TYPE_I64) {
        switch (operator_kind) {
            case RUDA_TOKEN_PLUS_EQUAL:
                *kind = RUDA_MIR_ADD_CHECKED_I64;
                return true;
            case RUDA_TOKEN_MINUS_EQUAL:
                *kind = RUDA_MIR_SUBTRACT_CHECKED_I64;
                return true;
            case RUDA_TOKEN_STAR_EQUAL:
                *kind = RUDA_MIR_MULTIPLY_CHECKED_I64;
                return true;
            case RUDA_TOKEN_SLASH_EQUAL:
                *kind = RUDA_MIR_DIVIDE_CHECKED_I64;
                return true;
            case RUDA_TOKEN_PERCENT_EQUAL:
                *kind = RUDA_MIR_REMAINDER_CHECKED_I64;
                return true;
            case RUDA_TOKEN_AMPERSAND_EQUAL:
                *kind = RUDA_MIR_BITWISE_AND_I64;
                return true;
            case RUDA_TOKEN_CARET_EQUAL:
                *kind = RUDA_MIR_BITWISE_XOR_I64;
                return true;
            case RUDA_TOKEN_PIPE_EQUAL:
                *kind = RUDA_MIR_BITWISE_OR_I64;
                return true;
            case RUDA_TOKEN_SHIFT_LEFT_EQUAL:
                *kind = RUDA_MIR_SHIFT_LEFT_CHECKED_I64;
                return true;
            case RUDA_TOKEN_SHIFT_RIGHT_EQUAL:
                *kind = RUDA_MIR_SHIFT_RIGHT_I64;
                return true;
            default:
                return false;
        }
    }
    if (type == RUDA_TYPE_F64) {
        switch (operator_kind) {
            case RUDA_TOKEN_PLUS_EQUAL:
                *kind = RUDA_MIR_ADD_F64;
                return true;
            case RUDA_TOKEN_MINUS_EQUAL:
                *kind = RUDA_MIR_SUBTRACT_F64;
                return true;
            case RUDA_TOKEN_STAR_EQUAL:
                *kind = RUDA_MIR_MULTIPLY_F64;
                return true;
            case RUDA_TOKEN_SLASH_EQUAL:
                *kind = RUDA_MIR_DIVIDE_F64;
                return true;
            default:
                return false;
        }
    }
    return false;
}

static bool ruda_compile_record_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left,
    const RudaAstNode *right,
    const RudaSyntaxToken *operator_token,
    RudaMirInstructionKind compound_kind,
    bool compound
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *field_token = NULL;
    RudaExpressionValue receiver;
    RudaExpressionValue previous;
    RudaExpressionValue value;
    RudaExpressionValue combined;
    RudaExpressionValue stored;
    RudaCheckRecordDeclaration *record_declaration = NULL;
    RudaCheckRecordFieldDeclaration *field_declaration = NULL;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    size_t field_index = 0;
    memset(&receiver, 0, sizeof(receiver));
    memset(&previous, 0, sizeof(previous));
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    memset(&stored, 0, sizeof(stored));
    if (!ruda_member_parts(left, &receiver_node, &field_token)
        || !ruda_resolve_record_member(
            function,
            left,
            &receiver,
            &record_declaration,
            &field_declaration,
            &record,
            &field_index
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)) {
        return true;
    }
    (void)receiver_node;
    (void)record_declaration;
    if (!field_declaration->is_mutable) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
            "cannot assign to an immutable record field",
            field_token->start,
            field_token->end
        );
        return false;
    }
    if (compound && !ruda_compound_assignment_kind(
            field_declaration->type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the record field type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (compound && !ruda_function_emit_record_access(
            function,
            RUDA_MIR_RECORD_GET,
            field_declaration->type,
            receiver.register_id,
            RUDA_MIR_NO_REGISTER,
            record,
            field_index,
            left->start,
            left->end,
            &previous
        )) {
        return false;
    }
    if (!ruda_compile_expression(
            function,
            right,
            field_declaration->type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != field_declaration->type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the record field type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !ruda_function_emit_instruction(
            function,
            compound_kind,
            field_declaration->type,
            previous.register_id,
            value.register_id,
            0,
            false,
            node->start,
            node->end,
            &combined
        )) {
        return false;
    }
    return ruda_function_emit_record_access(
        function,
        RUDA_MIR_RECORD_SET,
        field_declaration->type,
        receiver.register_id,
        compound ? combined.register_id : value.register_id,
        record,
        field_index,
        node->start,
        node->end,
        &stored
    );
}

static bool ruda_compile_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *left = NULL;
    const RudaAstNode *right = NULL;
    const RudaSyntaxToken *operator_token = NULL;
    const RudaSyntaxToken *name_token = NULL;
    RudaCheckName name;
    RudaCheckLocal *local = NULL;
    RudaExpressionValue value;
    RudaExpressionValue combined;
    RudaMirInstructionKind compound_kind = RUDA_MIR_COPY;
    bool compound = false;
    if (!ruda_binary_parts(node, &left, &operator_token, &right)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    switch (operator_token->kind) {
        case RUDA_TOKEN_EQUAL:
            break;
        case RUDA_TOKEN_PLUS_EQUAL:
            compound_kind = RUDA_MIR_ADD_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_MINUS_EQUAL:
            compound_kind = RUDA_MIR_SUBTRACT_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_STAR_EQUAL:
            compound_kind = RUDA_MIR_MULTIPLY_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_SLASH_EQUAL:
            compound_kind = RUDA_MIR_DIVIDE_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_PERCENT_EQUAL:
            compound_kind = RUDA_MIR_REMAINDER_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_AMPERSAND_EQUAL:
            compound_kind = RUDA_MIR_BITWISE_AND_I64;
            compound = true;
            break;
        case RUDA_TOKEN_CARET_EQUAL:
            compound_kind = RUDA_MIR_BITWISE_XOR_I64;
            compound = true;
            break;
        case RUDA_TOKEN_PIPE_EQUAL:
            compound_kind = RUDA_MIR_BITWISE_OR_I64;
            compound = true;
            break;
        case RUDA_TOKEN_SHIFT_LEFT_EQUAL:
            compound_kind = RUDA_MIR_SHIFT_LEFT_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_SHIFT_RIGHT_EQUAL:
            compound_kind = RUDA_MIR_SHIFT_RIGHT_I64;
            compound = true;
            break;
        default:
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
    }
    if (left->kind == RUDA_SYNTAX_MEMBER_EXPRESSION) {
        return ruda_compile_record_assignment(
            function,
            node,
            left,
            right,
            operator_token,
            compound_kind,
            compound
        );
    }
    if (left->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    name_token = ruda_check_child_token(left, RUDA_TOKEN_IDENTIFIER, 0);
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    local = ruda_function_find_local_mutable(function, name);
    if (local == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            name.start,
            name.end
        );
        return false;
    }
    if (!local->mutable_binding) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
            "cannot assign to a const binding",
            name.start,
            name.end
        );
        return false;
    }
    if (compound && !local->initialized) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "local is used before it is initialized",
            name.start,
            name.end
        );
        return false;
    }
    if (compound && !ruda_compound_assignment_kind(
            local->type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    if (!ruda_compile_expression(function, right, local->type, &value)) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != local->type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the binding type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !ruda_function_emit_instruction(
            function,
            compound_kind,
            local->type,
            local->register_id,
            value.register_id,
            0,
            false,
            node->start,
            node->end,
            &combined
        )) {
        return false;
    }
    if (!ruda_function_emit_copy(
            function,
            local->register_id,
            compound ? combined.register_id : value.register_id,
            right->start,
            right->end
        )) {
        return false;
    }
    local->initialized = true;
    return true;
}

static bool ruda_compile_expression_statement(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *expression = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *inner = expression != NULL
        ? ruda_expression_only_node(expression)
        : NULL;
    RudaExpressionValue value;
    if (inner == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (inner->kind == RUDA_SYNTAX_CALL_EXPRESSION) {
        memset(&value, 0, sizeof(value));
        return ruda_compile_expression(
            function,
            inner,
            RUDA_TYPE_INVALID,
            &value
        );
    }
    if (inner->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        const RudaSyntaxToken *token = ruda_check_child_token(
            inner,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        memset(&value, 0, sizeof(value));
        if (token != NULL && ruda_check_text_equal(
                function->checker,
                token->start,
                token->end,
                "panic"
            )) {
            return ruda_compile_expression(
                function,
                inner,
                RUDA_TYPE_INVALID,
                &value
            );
        }
    }
    if (inner->kind != RUDA_SYNTAX_ASSIGNMENT_EXPRESSION) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    return ruda_compile_assignment(function, inner);
}

static bool ruda_function_set_goto(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirBlockId target,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_GOTO;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(&function->mir, block, terminator)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return true;
}

static bool ruda_function_set_branch(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(&function->mir, block, terminator)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return true;
}

static bool ruda_function_set_return(
    RudaFunctionChecker *function,
    RudaMirRegister value,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_RETURN;
    terminator.value = value;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

static bool ruda_function_set_panic(
    RudaFunctionChecker *function,
    RudaMirRegister message,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_PANIC;
    terminator.value = message;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

static bool ruda_function_set_never_call(
    RudaFunctionChecker *function,
    RudaMirCallId call,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_CALL_NEVER;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.call = call;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

static bool ruda_compile_return(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *expression = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaExpressionValue value;
    if (expression == NULL) {
        if (function->result_type != RUDA_TYPE_VOID) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "non-void function must return a value",
                node->start,
                node->end
            );
            return false;
        }
        return ruda_function_set_return(
            function,
            RUDA_MIR_NO_REGISTER,
            node->start,
            node->end
        );
    }
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function,
            expression,
            function->result_type == RUDA_TYPE_VOID
                    || function->result_type == RUDA_TYPE_NEVER
                ? RUDA_TYPE_INVALID
                : function->result_type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (function->result_type == RUDA_TYPE_VOID) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "void function cannot return a value",
            expression->start,
            expression->end
        );
        return false;
    }
    if (value.type != function->result_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "returned value type does not match the function result type",
            expression->start,
            expression->end
        );
        return false;
    }
    return ruda_function_set_return(
        function,
        value.register_id,
        node->start,
        node->end
    );
}

static bool ruda_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);

static bool ruda_compile_condition(
    RudaFunctionChecker *function,
    const RudaAstNode *expression,
    RudaExpressionValue *result
) {
    if (expression == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!ruda_compile_expression(
            function,
            expression,
            RUDA_TYPE_INVALID,
            result
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, result)) {
        return true;
    }
    if (result->type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "condition must have type bool",
            expression->start,
            expression->end
        );
        return false;
    }
    return true;
}

static bool ruda_compile_scoped_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
) {
    size_t local_count = function->locals.length;
    size_t previous_depth = function->scope_depth;
    bool compiled = false;
    if (block == NULL || previous_depth == SIZE_MAX) {
        ruda_checker_fail(
            function->checker,
            block == NULL
                ? RUDA_CHECK_INVALID_ARGUMENT
                : RUDA_CHECK_LIMIT_REACHED
        );
        return false;
    }
    function->scope_depth = previous_depth + 1;
    compiled = ruda_compile_block(function, block);
    function->locals.length = local_count;
    function->scope_depth = previous_depth;
    return compiled;
}

static bool ruda_merge_initialization_states(
    RudaFunctionChecker *function,
    const RudaInitializationSnapshot *when_true,
    bool true_continues,
    const RudaInitializationSnapshot *when_false,
    bool false_continues
) {
    size_t index = 0;
    if (when_true == NULL || when_false == NULL
        || when_true->states == NULL || when_false->states == NULL
        || when_true->count != when_false->count
        || when_true->count > function->locals.length
        || (!true_continues && !false_continues)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < when_true->count; index += 1) {
        RudaCheckLocal *local = ruda_vector_at(&function->locals, index);
        bool initialized = false;
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (true_continues && false_continues) {
            initialized = when_true->states[index] != 0
                && when_false->states[index] != 0;
        } else if (true_continues) {
            initialized = when_true->states[index] != 0;
        } else {
            initialized = when_false->states[index] != 0;
        }
        local->initialized = initialized;
    }
    return true;
}

static bool ruda_compile_if(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *condition = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *when_true_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        0
    );
    const RudaAstNode *when_false_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        1
    );
    const RudaAstNode *else_if_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_IF_STATEMENT,
        0
    );
    RudaInitializationSnapshot initial;
    RudaInitializationSnapshot true_state;
    RudaInitializationSnapshot false_state;
    RudaExpressionValue condition_value;
    RudaMirBlockId branch_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId true_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId false_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId true_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId false_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    size_t local_count = function->locals.length;
    bool initial_initialized = false;
    bool true_state_initialized = false;
    bool false_state_initialized = false;
    bool true_continues = false;
    bool false_continues = false;
    bool accepted = false;
    memset(&initial, 0, sizeof(initial));
    memset(&true_state, 0, sizeof(true_state));
    memset(&false_state, 0, sizeof(false_state));
    memset(&condition_value, 0, sizeof(condition_value));
    if (condition == NULL || when_true_node == NULL
        || (when_false_node != NULL && else_if_node != NULL)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    initial_initialized = ruda_initialization_snapshot_init(
        function,
        local_count,
        &initial
    );
    true_state_initialized = initial_initialized
        && ruda_initialization_snapshot_init(
            function,
            local_count,
            &true_state
        );
    false_state_initialized = true_state_initialized
        && ruda_initialization_snapshot_init(
            function,
            local_count,
            &false_state
        );
    if (!initial_initialized || !true_state_initialized
        || !false_state_initialized
        || !ruda_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    branch_block = function->block;
    if (!ruda_function_add_block(function, &true_block)
        || !ruda_function_add_block(function, &false_block)
        || !ruda_function_set_branch(
            function,
            branch_block,
            condition_value.register_id,
            true_block,
            false_block,
            condition->start,
            condition->end
        )) {
        goto cleanup;
    }

    function->block = true_block;
    function->terminated = false;
    if (!ruda_compile_scoped_block(function, when_true_node)) {
        goto cleanup;
    }
    true_continues = !function->terminated;
    true_end = function->block;
    if (!ruda_initialization_snapshot_capture(function, &true_state)
        || !ruda_initialization_snapshot_restore(function, &initial)) {
        goto cleanup;
    }

    function->block = false_block;
    function->terminated = false;
    if (when_false_node != NULL) {
        if (!ruda_compile_scoped_block(function, when_false_node)) {
            goto cleanup;
        }
    } else if (else_if_node != NULL) {
        if (!ruda_compile_if(function, else_if_node)) {
            goto cleanup;
        }
    }
    false_continues = !function->terminated;
    false_end = function->block;
    if (!ruda_initialization_snapshot_capture(function, &false_state)) {
        goto cleanup;
    }

    if (!true_continues && !false_continues) {
        function->terminated = true;
        accepted = true;
        goto cleanup;
    }
    if (!ruda_merge_initialization_states(
            function,
            &true_state,
            true_continues,
            &false_state,
            false_continues
        )) {
        goto cleanup;
    }
    if (true_continues && false_continues) {
        if (!ruda_function_add_block(function, &join)
            || !ruda_function_set_goto(
                function,
                true_end,
                join,
                node->end,
                node->end
            ) || !ruda_function_set_goto(
                function,
                false_end,
                join,
                node->end,
                node->end
            )) {
            goto cleanup;
        }
        function->block = join;
    } else {
        function->block = true_continues ? true_end : false_end;
    }
    function->terminated = false;
    accepted = true;

cleanup:
    if (false_state_initialized) {
        ruda_initialization_snapshot_dispose(&false_state);
    }
    if (true_state_initialized) {
        ruda_initialization_snapshot_dispose(&true_state);
    }
    if (initial_initialized) {
        ruda_initialization_snapshot_dispose(&initial);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_while(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *condition = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *body = ruda_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        0
    );
    RudaInitializationSnapshot initial;
    RudaExpressionValue condition_value;
    RudaMirBlockId entry = function->block;
    RudaMirBlockId condition_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId condition_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId exit_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_end = RUDA_MIR_NO_BLOCK;
    RudaLoopContext loop;
    bool initial_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&initial, 0, sizeof(initial));
    memset(&condition_value, 0, sizeof(condition_value));
    if (condition == NULL || body == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    initial_initialized = ruda_initialization_snapshot_init(
        function,
        function->locals.length,
        &initial
    );
    if (!initial_initialized
        || !ruda_function_add_block(function, &condition_block)
        || !ruda_function_set_goto(
            function,
            entry,
            condition_block,
            node->start,
            node->start
        )) {
        goto cleanup;
    }
    function->block = condition_block;
    function->terminated = false;
    if (!ruda_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    condition_end = function->block;
    if (!ruda_function_add_block(function, &body_block)
        || !ruda_function_add_block(function, &exit_block)
        || !ruda_function_set_branch(
            function,
            condition_end,
            condition_value.register_id,
            body_block,
            exit_block,
            condition->start,
            condition->end
        )) {
        goto cleanup;
    }
    loop.break_target = exit_block;
    loop.continue_target = condition_block;
    if (function->loops.length >= function->mir.options.max_blocks) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_vector_push(&function->loops, &loop)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    loop_pushed = true;
    function->block = body_block;
    function->terminated = false;
    if (!ruda_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !ruda_function_set_goto(
            function,
            body_end,
            condition_block,
            body->end,
            body->end
        )) {
        goto cleanup;
    }
    if (!ruda_initialization_snapshot_restore(function, &initial)) {
        goto cleanup;
    }
    function->block = exit_block;
    function->terminated = false;
    accepted = true;

cleanup:
    if (loop_pushed) {
        function->loops.length -= 1;
    }
    if (initial_initialized) {
        ruda_initialization_snapshot_dispose(&initial);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_c_for(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *header = ruda_check_child_node(
        node,
        RUDA_SYNTAX_FOR_HEADER,
        0
    );
    const RudaAstNode *body = ruda_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        0
    );
    const RudaSyntaxToken *let_token = ruda_check_child_token(
        header,
        RUDA_TOKEN_KW_LET,
        0
    );
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        header,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *initializer = ruda_check_child_node(
        header,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *condition = ruda_check_child_node(
        header,
        RUDA_SYNTAX_EXPRESSION,
        1
    );
    const RudaAstNode *step = ruda_check_child_node(
        header,
        RUDA_SYNTAX_EXPRESSION,
        2
    );
    const RudaAstNode *step_inner = NULL;
    RudaCheckName name;
    RudaCheckLocal *local = NULL;
    RudaExpressionValue initial_value;
    RudaExpressionValue condition_value;
    RudaInitializationSnapshot initial_state;
    RudaLoopContext loop;
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId condition_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId condition_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId step_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId exit_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_end = RUDA_MIR_NO_BLOCK;
    size_t outer_local_count = function->locals.length;
    size_t previous_depth = function->scope_depth;
    bool scope_entered = false;
    bool initial_state_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&initial_value, 0, sizeof(initial_value));
    memset(&condition_value, 0, sizeof(condition_value));
    memset(&initial_state, 0, sizeof(initial_state));
    if (header == NULL || body == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (let_token == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (name_token == NULL || name_token->synthetic || initializer == NULL
        || condition == NULL || step == NULL || previous_depth == SIZE_MAX) {
        ruda_checker_fail(
            function->checker,
            previous_depth == SIZE_MAX
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_INVALID_ARGUMENT
        );
        goto cleanup;
    }
    function->scope_depth = previous_depth + 1;
    scope_entered = true;
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!ruda_compile_expression(
            function,
            initializer,
            RUDA_TYPE_INVALID,
            &initial_value
        )) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &initial_value)) {
        accepted = true;
        goto cleanup;
    }
    if (!ruda_function_add_local(
            function,
            name,
            initial_value.type,
            true,
            true,
            &local
        ) || local == NULL
        || !ruda_function_emit_copy(
            function,
            local->register_id,
            initial_value.register_id,
            initializer->start,
            initializer->end
        )) {
        goto cleanup;
    }
    entry = function->block;
    initial_state_initialized = ruda_initialization_snapshot_init(
        function,
        outer_local_count,
        &initial_state
    );
    if (!initial_state_initialized
        || !ruda_function_add_block(function, &condition_block)
        || !ruda_function_set_goto(
            function,
            entry,
            condition_block,
            node->start,
            node->start
        )) {
        goto cleanup;
    }
    function->block = condition_block;
    function->terminated = false;
    if (!ruda_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    condition_end = function->block;
    if (!ruda_function_add_block(function, &body_block)
        || !ruda_function_add_block(function, &step_block)
        || !ruda_function_add_block(function, &exit_block)
        || !ruda_function_set_branch(
            function,
            condition_end,
            condition_value.register_id,
            body_block,
            exit_block,
            condition->start,
            condition->end
        )) {
        goto cleanup;
    }
    loop.break_target = exit_block;
    loop.continue_target = step_block;
    if (function->loops.length >= function->mir.options.max_blocks) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_vector_push(&function->loops, &loop)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    loop_pushed = true;
    function->block = body_block;
    function->terminated = false;
    if (!ruda_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !ruda_function_set_goto(
            function,
            body_end,
            step_block,
            body->end,
            body->end
        )) {
        goto cleanup;
    }
    step_inner = ruda_expression_only_node(step);
    function->block = step_block;
    function->terminated = false;
    if (step_inner == NULL
        || step_inner->kind != RUDA_SYNTAX_ASSIGNMENT_EXPRESSION) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (!ruda_compile_assignment(function, step_inner)) {
        goto cleanup;
    }
    if ((!function->terminated && !ruda_function_set_goto(
            function,
            function->block,
            condition_block,
            step->end,
            step->end
        )) || !ruda_initialization_snapshot_restore(
            function,
            &initial_state
        )) {
        goto cleanup;
    }
    function->locals.length = outer_local_count;
    function->scope_depth = previous_depth;
    scope_entered = false;
    function->block = exit_block;
    function->terminated = false;
    accepted = true;

cleanup:
    if (loop_pushed) {
        function->loops.length -= 1;
    }
    if (initial_state_initialized) {
        ruda_initialization_snapshot_dispose(&initial_state);
    }
    if (scope_entered) {
        function->locals.length = outer_local_count;
        function->scope_depth = previous_depth;
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

static bool ruda_compile_loop_control(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaSyntaxToken *keyword = ruda_check_first_token(statement);
    const RudaLoopContext *loop = NULL;
    RudaMirBlockId target = RUDA_MIR_NO_BLOCK;
    if (keyword == NULL || (keyword->kind != RUDA_TOKEN_KW_BREAK
            && keyword->kind != RUDA_TOKEN_KW_CONTINUE)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (function->loops.length == 0) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
            "loop control statement is outside a loop",
            keyword->start,
            keyword->end
        );
        return false;
    }
    loop = ruda_vector_at_const(
        &function->loops,
        function->loops.length - 1
    );
    if (loop == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    target = keyword->kind == RUDA_TOKEN_KW_BREAK
        ? loop->break_target
        : loop->continue_target;
    if (!ruda_function_set_goto(
            function,
            function->block,
            target,
            statement->start,
            statement->end
        )) {
        return false;
    }
    function->terminated = true;
    return true;
}

static bool ruda_compile_statement(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaAstNode *block = NULL;
    if (statement == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (statement->kind == RUDA_SYNTAX_CONST_DECL
        || statement->kind == RUDA_SYNTAX_LET_DECL) {
        return ruda_compile_binding(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_RETURN_STATEMENT) {
        return ruda_compile_return(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_YIELD_STATEMENT) {
        return ruda_compile_yield(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_EXPRESSION_STATEMENT) {
        return ruda_compile_expression_statement(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_IF_STATEMENT) {
        return ruda_compile_if(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_WHILE_STATEMENT) {
        return ruda_compile_while(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_FOR_STATEMENT) {
        return ruda_compile_c_for(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_BREAK_STATEMENT
        || statement->kind == RUDA_SYNTAX_CONTINUE_STATEMENT) {
        return ruda_compile_loop_control(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_BLOCK_STATEMENT) {
        block = ruda_check_child_node(statement, RUDA_SYNTAX_BLOCK, 0);
        return ruda_compile_scoped_block(function, block);
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

static bool ruda_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
) {
    size_t index = 0;
    if (block == NULL || block->kind != RUDA_SYNTAX_BLOCK) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < block->child_count
        && !ruda_checker_has_failed(function->checker); index += 1) {
        const RudaAstElement *element = ruda_ast_child(block, index);
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic
                || (element->as.token.kind != RUDA_TOKEN_LEFT_BRACE
                    && element->as.token.kind != RUDA_TOKEN_RIGHT_BRACE)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                return false;
            }
            continue;
        }
        if (function->terminated) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (!ruda_compile_statement(function, element->as.node)) {
            return false;
        }
    }
    return !ruda_checker_has_failed(function->checker);
}

static void ruda_function_declaration_dispose(
    RudaCheckFunctionDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->parameter_types_initialized) {
        ruda_vector_dispose(&declaration->parameter_types);
    }
    if (declaration->parameter_names_initialized) {
        ruda_vector_dispose(&declaration->parameter_names);
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

static void ruda_constant_declaration_dispose(
    RudaCheckConstantDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

static void ruda_type_alias_declaration_dispose(
    RudaCheckTypeAliasDeclaration *declaration
) {
    if (declaration == NULL) {
        return;
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

static void ruda_record_field_declaration_dispose(
    RudaCheckRecordFieldDeclaration *field
) {
    if (field == NULL) {
        return;
    }
    if (field->name_initialized) {
        ruda_owned_bytes_dispose(&field->owned_name);
    }
    memset(field, 0, sizeof(*field));
}

static void ruda_record_declaration_dispose(
    RudaCheckRecordDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->fields_initialized) {
        for (index = 0; index < declaration->fields.length; index += 1) {
            RudaCheckRecordFieldDeclaration *field = ruda_vector_at(
                &declaration->fields,
                index
            );
            ruda_record_field_declaration_dispose(field);
        }
        ruda_vector_dispose(&declaration->fields);
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

static void ruda_enum_payload_item_declaration_dispose(
    RudaCheckEnumPayloadItemDeclaration *item
) {
    if (item == NULL) {
        return;
    }
    if (item->name_initialized) {
        ruda_owned_bytes_dispose(&item->owned_name);
    }
    memset(item, 0, sizeof(*item));
}

static void ruda_enum_variant_declaration_dispose(
    RudaCheckEnumVariantDeclaration *variant
) {
    size_t index = 0;
    if (variant == NULL) {
        return;
    }
    if (variant->payload_initialized) {
        for (index = 0; index < variant->payload.length; index += 1) {
            RudaCheckEnumPayloadItemDeclaration *item = ruda_vector_at(
                &variant->payload,
                index
            );
            ruda_enum_payload_item_declaration_dispose(item);
        }
        ruda_vector_dispose(&variant->payload);
    }
    if (variant->name_initialized) {
        ruda_owned_bytes_dispose(&variant->owned_name);
    }
    memset(variant, 0, sizeof(*variant));
}

static void ruda_enum_declaration_dispose(
    RudaCheckEnumDeclaration *declaration
) {
    size_t index = 0;
    if (declaration == NULL) {
        return;
    }
    if (declaration->variants_initialized) {
        for (index = 0; index < declaration->variants.length; index += 1) {
            RudaCheckEnumVariantDeclaration *variant = ruda_vector_at(
                &declaration->variants,
                index
            );
            ruda_enum_variant_declaration_dispose(variant);
        }
        ruda_vector_dispose(&declaration->variants);
    }
    if (declaration->name_initialized) {
        ruda_owned_bytes_dispose(&declaration->owned_name);
    }
    memset(declaration, 0, sizeof(*declaration));
}

static bool ruda_declaration_name_is_duplicate(
    const RudaChecker *checker,
    RudaCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    return ruda_check_name_bytes(name, &bytes, &length)
        && ruda_map_get_const(
            &checker->top_level_index,
            bytes,
            length
        ) != NULL;
}

static bool ruda_parameter_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->parameter_names.length; index += 1) {
        const RudaCheckName *existing = ruda_vector_at_const(
            &declaration->parameter_names,
            index
        );
        if (existing != NULL
            && ruda_check_names_equal(checker, *existing, name)) {
            return true;
        }
    }
    return false;
}

static bool ruda_check_name_is_primitive_type(
    const RudaChecker *checker,
    RudaCheckName name
) {
    static const char *const names[] = {
        "bool", "i8", "i16", "i32", "i64", "int", "u8", "byte",
        "u16", "u32", "u64", "uint", "f32", "f64", "float", "rune",
        "string", "void", "never"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        if (ruda_check_text_equal(
                checker,
                name.start,
                name.end,
                names[index]
            )) {
            return true;
        }
    }
    return false;
}

static bool ruda_check_public_type_reference(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = NULL;
    RudaCheckName name;
    RudaCheckRecordDeclaration *record = NULL;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    bool is_public = false;
    if (checker == NULL || node == NULL || node->kind != RUDA_SYNTAX_TYPE
        || node->child_count != 1) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    name_token = ruda_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0);
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    name.snapshot = checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (ruda_check_name_is_primitive_type(checker, name)) {
        return true;
    }
    record = ruda_checker_find_record(checker, name);
    if (record != NULL) {
        is_public = record->is_public;
    } else {
        enum_declaration = ruda_checker_find_enum(checker, name);
        if (enum_declaration != NULL) {
            is_public = enum_declaration->is_public;
        } else {
            alias = ruda_checker_find_type_alias(checker, name);
            if (alias == NULL) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            is_public = alias->is_public;
        }
    }
    if (!is_public) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
            "public declaration exposes a package-private type",
            name.start,
            name.end
        );
        return false;
    }
    return true;
}

static bool ruda_collect_record_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckRecordDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && (element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE
                || element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS)) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->records.length >= checker->options.max_records) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.is_public = ruda_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    declaration.declaration_valid = true;
    duplicate_name = ruda_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || ruda_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_vector_init(
            &declaration.fields,
            checker->result->allocator,
            sizeof(RudaCheckRecordFieldDeclaration)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.fields_initialized = true;
    if (!ruda_symbol_id_from_parts(
            checker->package_name,
            ruda_source_path(checker->ast->snapshot),
            (const char *)declaration.owned_name.data,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!duplicate_name && !ruda_type_intern_nominal(
            &checker->result->types,
            RUDA_TYPE_KIND_NOMINAL,
            declaration.symbol,
            NULL,
            0,
            &declaration.type
        )) {
        ruda_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - RUDA_TYPE_FIRST_DYNAMIC
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_RECORD;
    top_level_entry.index = checker->records.length;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->records,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
    }
    if (!ruda_vector_push(&checker->records, &declaration)) {
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_record_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_collect_enum_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckEnumDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && (element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE
                || element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS)) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->enums.length >= checker->options.max_enums) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.is_public = ruda_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    declaration.declaration_valid = true;
    duplicate_name = ruda_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || ruda_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(RudaCheckEnumVariantDeclaration)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.variants_initialized = true;
    if (!ruda_symbol_id_from_parts(
            checker->package_name,
            ruda_source_path(checker->ast->snapshot),
            (const char *)declaration.owned_name.data,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!duplicate_name && !ruda_type_intern_nominal(
            &checker->result->types,
            RUDA_TYPE_KIND_NOMINAL,
            declaration.symbol,
            NULL,
            0,
            &declaration.type
        )) {
        ruda_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - RUDA_TYPE_FIRST_DYNAMIC
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_ENUM;
    top_level_entry.index = checker->enums.length;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->enums,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
    }
    if (!ruda_vector_push(&checker->enums, &declaration)) {
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_enum_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_record_field_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckRecordDeclaration *declaration,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->fields.length; index += 1) {
        const RudaCheckRecordFieldDeclaration *field = ruda_vector_at_const(
            &declaration->fields,
            index
        );
        if (field != NULL && ruda_check_names_equal(checker, field->name, name)) {
            return true;
        }
    }
    return false;
}

static bool ruda_resolve_record_fields(
    RudaChecker *checker,
    RudaCheckRecordDeclaration *declaration
) {
    size_t child_index = 0;
    size_t field_count = 0;
    bool valid = true;
    if (declaration == NULL || !declaration->declaration_valid
        || !declaration->fields_initialized
        || !ruda_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (child_index = 0; child_index < declaration->node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            declaration->node,
            child_index
        );
        RudaCheckRecordFieldDeclaration field;
        const RudaSyntaxToken *name_token = NULL;
        const RudaAstNode *type_node = NULL;
        bool duplicate = false;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE
            || element->as.node == NULL
            || element->as.node->kind != RUDA_SYNTAX_FIELD_DECL) {
            continue;
        }
        if (field_count >= checker->options.max_fields_per_record) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        field_count += 1;
        memset(&field, 0, sizeof(field));
        field.node = element->as.node;
        name_token = ruda_check_child_token(
            field.node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        type_node = ruda_check_child_node(field.node, RUDA_SYNTAX_TYPE, 0);
        if (name_token == NULL || name_token->synthetic || type_node == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            ruda_record_field_declaration_dispose(&field);
            return false;
        }
        field.name.snapshot = checker->ast->snapshot;
        field.name.start = name_token->start;
        field.name.end = name_token->end;
        field.type = RUDA_TYPE_INVALID;
        field.is_public = ruda_check_child_token(
            field.node,
            RUDA_TOKEN_KW_PUBLIC,
            0
        ) != NULL;
        field.is_mutable = ruda_check_child_token(
            field.node,
            RUDA_TOKEN_KW_MUTABLE,
            0
        ) != NULL;
        duplicate = ruda_record_field_name_is_duplicate(
            checker,
            declaration,
            field.name
        );
        if (duplicate) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "field name is already declared in this record",
                field.name.start,
                field.name.end
            );
            valid = false;
            if (ruda_checker_has_failed(checker)) {
                ruda_record_field_declaration_dispose(&field);
                return false;
            }
            continue;
        }
        if (!ruda_owned_bytes_copy(
                &field.owned_name,
                checker->result->allocator,
                checker->source + field.name.start,
                field.name.end - field.name.start
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            ruda_record_field_declaration_dispose(&field);
            return false;
        }
        field.name_initialized = true;
        if (!ruda_check_exact_type(checker, type_node, &field.type)) {
            if (ruda_checker_has_failed(checker)) {
                ruda_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        } else if (declaration->is_public && field.is_public
            && !ruda_check_public_type_reference(checker, type_node)) {
            if (ruda_checker_has_failed(checker)) {
                ruda_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        }
        if (!ruda_vector_push(&declaration->fields, &field)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            ruda_record_field_declaration_dispose(&field);
            return false;
        }
        memset(&field, 0, sizeof(field));
    }
    declaration->fields_resolved = valid;
    return valid && !ruda_checker_has_failed(checker);
}

static bool ruda_resolve_all_record_fields(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->records.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckRecordDeclaration *declaration = ruda_vector_at(
            &checker->records,
            index
        );
        if (declaration == NULL || !ruda_resolve_record_fields(
                checker,
                declaration
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

static bool ruda_enum_variant_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckEnumDeclaration *declaration,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->variants.length; index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = ruda_vector_at_const(
            &declaration->variants,
            index
        );
        if (variant != NULL
            && ruda_check_names_equal(checker, variant->name, name)) {
            return true;
        }
    }
    return false;
}

static bool ruda_enum_payload_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckEnumVariantDeclaration *variant,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < variant->payload.length; index += 1) {
        const RudaCheckEnumPayloadItemDeclaration *item = ruda_vector_at_const(
            &variant->payload,
            index
        );
        if (item != NULL && item->has_name
            && ruda_check_names_equal(checker, item->name, name)) {
            return true;
        }
    }
    return false;
}

static bool ruda_resolve_enum_payload_item(
    RudaChecker *checker,
    const RudaCheckEnumDeclaration *declaration,
    RudaCheckEnumVariantDeclaration *variant,
    const RudaAstNode *node,
    bool has_name,
    bool *valid
) {
    const RudaAstNode *type_node = has_name
        ? ruda_check_child_node(node, RUDA_SYNTAX_TYPE, 0)
        : node;
    const RudaSyntaxToken *name_token = has_name
        ? ruda_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0)
        : NULL;
    RudaCheckEnumPayloadItemDeclaration item;
    memset(&item, 0, sizeof(item));
    if (checker == NULL || declaration == NULL || variant == NULL
        || node == NULL || valid == NULL || type_node == NULL
        || type_node->kind != RUDA_SYNTAX_TYPE
        || (has_name && (name_token == NULL || name_token->synthetic))) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    item.node = node;
    item.type = RUDA_TYPE_INVALID;
    item.has_name = has_name;
    if (has_name) {
        item.name.snapshot = checker->ast->snapshot;
        item.name.start = name_token->start;
        item.name.end = name_token->end;
        if (ruda_enum_payload_name_is_duplicate(
                checker,
                variant,
                item.name
            )) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "payload field name is already declared in this variant",
                item.name.start,
                item.name.end
            );
            *valid = false;
            return !ruda_checker_has_failed(checker);
        }
        if (!ruda_owned_bytes_copy(
                &item.owned_name,
                checker->result->allocator,
                checker->source + item.name.start,
                item.name.end - item.name.start
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        item.name_initialized = true;
    }
    if (!ruda_check_exact_type(checker, type_node, &item.type)) {
        if (ruda_checker_has_failed(checker)) {
            ruda_enum_payload_item_declaration_dispose(&item);
            return false;
        }
        *valid = false;
    } else if (declaration->is_public
        && !ruda_check_public_type_reference(checker, type_node)) {
        if (ruda_checker_has_failed(checker)) {
            ruda_enum_payload_item_declaration_dispose(&item);
            return false;
        }
        *valid = false;
    }
    if (!ruda_vector_push(&variant->payload, &item)) {
        ruda_enum_payload_item_declaration_dispose(&item);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&item, 0, sizeof(item));
    return true;
}

static bool ruda_resolve_enum_variants(
    RudaChecker *checker,
    RudaCheckEnumDeclaration *declaration
) {
    size_t child_index = 0;
    size_t variant_count = 0;
    bool valid = true;
    if (declaration == NULL || !declaration->declaration_valid
        || !declaration->variants_initialized
        || !ruda_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (child_index = 0; child_index < declaration->node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            declaration->node,
            child_index
        );
        const RudaSyntaxToken *name_token = NULL;
        const RudaAstNode *payload_node = NULL;
        RudaCheckEnumVariantDeclaration variant;
        size_t payload_index = 0;
        size_t payload_count = 0;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE
            || element->as.node == NULL
            || element->as.node->kind != RUDA_SYNTAX_ENUM_VARIANT) {
            continue;
        }
        if (variant_count >= checker->options.max_variants_per_enum) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        variant_count += 1;
        memset(&variant, 0, sizeof(variant));
        variant.node = element->as.node;
        name_token = ruda_check_child_token(
            variant.node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (name_token == NULL || name_token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        variant.name.snapshot = checker->ast->snapshot;
        variant.name.start = name_token->start;
        variant.name.end = name_token->end;
        if (ruda_enum_variant_name_is_duplicate(
                checker,
                declaration,
                variant.name
            )) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "variant name is already declared in this enum",
                variant.name.start,
                variant.name.end
            );
            valid = false;
            if (ruda_checker_has_failed(checker)) {
                return false;
            }
            continue;
        }
        if (!ruda_owned_bytes_copy(
                &variant.owned_name,
                checker->result->allocator,
                checker->source + variant.name.start,
                variant.name.end - variant.name.start
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        variant.name_initialized = true;
        if (!ruda_vector_init(
                &variant.payload,
                checker->result->allocator,
                sizeof(RudaCheckEnumPayloadItemDeclaration)
            )) {
            ruda_enum_variant_declaration_dispose(&variant);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        variant.payload_initialized = true;
        payload_node = ruda_check_child_node(
            variant.node,
            RUDA_SYNTAX_POSITIONAL_PAYLOAD,
            0
        );
        if (payload_node != NULL) {
            variant.kind = RUDA_CHECKED_ENUM_VARIANT_POSITIONAL;
            for (payload_index = 0; payload_index < payload_node->child_count;
                payload_index += 1) {
                const RudaAstElement *payload = ruda_ast_child(
                    payload_node,
                    payload_index
                );
                if (payload != NULL && payload->kind == RUDA_AST_ELEMENT_NODE
                    && payload->as.node != NULL
                    && payload->as.node->kind == RUDA_SYNTAX_TYPE) {
                    if (payload_count
                            >= checker->options.max_payload_items_per_variant) {
                        ruda_enum_variant_declaration_dispose(&variant);
                        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
                        return false;
                    }
                    payload_count += 1;
                    if (!ruda_resolve_enum_payload_item(
                            checker,
                            declaration,
                            &variant,
                            payload->as.node,
                            false,
                            &valid
                        )) {
                        ruda_enum_variant_declaration_dispose(&variant);
                        return false;
                    }
                }
            }
        } else {
            payload_node = ruda_check_child_node(
                variant.node,
                RUDA_SYNTAX_NAMED_PAYLOAD,
                0
            );
            if (payload_node != NULL) {
                variant.kind = RUDA_CHECKED_ENUM_VARIANT_NAMED;
                for (payload_index = 0;
                    payload_index < payload_node->child_count;
                    payload_index += 1) {
                    const RudaAstElement *payload = ruda_ast_child(
                        payload_node,
                        payload_index
                    );
                    if (payload != NULL
                        && payload->kind == RUDA_AST_ELEMENT_NODE
                        && payload->as.node != NULL
                        && payload->as.node->kind == RUDA_SYNTAX_PAYLOAD_FIELD) {
                        if (payload_count >= checker->options
                                .max_payload_items_per_variant) {
                            ruda_enum_variant_declaration_dispose(&variant);
                            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
                            return false;
                        }
                        payload_count += 1;
                        if (!ruda_resolve_enum_payload_item(
                                checker,
                                declaration,
                                &variant,
                                payload->as.node,
                                true,
                                &valid
                            )) {
                            ruda_enum_variant_declaration_dispose(&variant);
                            return false;
                        }
                    }
                }
            } else {
                variant.kind = RUDA_CHECKED_ENUM_VARIANT_UNIT;
            }
        }
        if (!ruda_vector_push(&declaration->variants, &variant)) {
            ruda_enum_variant_declaration_dispose(&variant);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&variant, 0, sizeof(variant));
    }
    declaration->variants_resolved = valid;
    return valid && !ruda_checker_has_failed(checker);
}

static bool ruda_resolve_all_enum_variants(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->enums.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &checker->enums,
            index
        );
        if (declaration == NULL || !ruda_resolve_enum_variants(
                checker,
                declaration
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

static bool ruda_collect_type_alias_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *target = ruda_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    RudaCheckTypeAliasDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic || target == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && (element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE
                || element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS)) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->type_aliases.length >= checker->options.max_type_aliases) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.target = target;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.state = RUDA_CHECK_TYPE_ALIAS_UNRESOLVED;
    declaration.declaration_valid = true;
    declaration.is_public = ruda_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    duplicate_name = ruda_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || ruda_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_TYPE_ALIAS_INVALID;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_symbol_id_from_parts(
            checker->package_name,
            ruda_source_path(checker->ast->snapshot),
            (const char *)declaration.owned_name.data,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_TYPE_ALIAS;
    top_level_entry.index = checker->type_aliases.length;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->type_aliases,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
    }
    if (!ruda_vector_push(&checker->type_aliases, &declaration)) {
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_type_alias_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_evaluate_type_alias(
    RudaChecker *checker,
    RudaCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
) {
    const RudaAstTree *previous_ast = NULL;
    bool accepted = false;
    if (checker == NULL || declaration == NULL
        || !declaration->declaration_valid) {
        if (checker != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (declaration->state == RUDA_CHECK_TYPE_ALIAS_RESOLVED) {
        return true;
    }
    if (declaration->state == RUDA_CHECK_TYPE_ALIAS_INVALID) {
        return false;
    }
    if (declaration->state == RUDA_CHECK_TYPE_ALIAS_RESOLVING) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_ALIAS_CYCLE,
            "type alias dependency cycle",
            use_start,
            use_end
        );
        return false;
    }
    if (checker->type_alias_depth >= checker->options.max_type_alias_depth) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    previous_ast = checker->ast;
    declaration->state = RUDA_CHECK_TYPE_ALIAS_RESOLVING;
    checker->type_alias_depth += 1;
    if (!ruda_checker_set_ast(checker, declaration->ast)
        || !ruda_check_exact_type(
            checker,
            declaration->target,
            &declaration->type
        )) {
        goto cleanup;
    }
    if (declaration->type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration->state = RUDA_CHECK_TYPE_ALIAS_RESOLVED;
    accepted = true;

cleanup:
    if (!accepted) {
        declaration->state = RUDA_CHECK_TYPE_ALIAS_INVALID;
    }
    checker->type_alias_depth -= 1;
    if (previous_ast != NULL && !ruda_checker_set_ast(checker, previous_ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_evaluate_all_type_aliases(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->type_aliases.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckTypeAliasDeclaration *declaration = ruda_vector_at(
            &checker->type_aliases,
            index
        );
        if (declaration == NULL || !ruda_checker_set_ast(
                checker,
                declaration->ast
            ) || !ruda_evaluate_type_alias(
                checker,
                declaration,
                declaration->name.start,
                declaration->name.end
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        if (declaration->is_public && !ruda_check_public_type_reference(
                checker,
                declaration->target
            )) {
            if (ruda_checker_has_failed(checker)) {
                return false;
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

static bool ruda_publish_type_aliases(RudaChecker *checker) {
    size_t index = 0;
    if (!ruda_vector_reserve(
            &checker->result->type_aliases,
            checker->type_aliases.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        RudaCheckTypeAliasDeclaration *declaration = ruda_vector_at(
            &checker->type_aliases,
            index
        );
        RudaCheckedTypeAlias alias;
        if (declaration == NULL
            || declaration->state != RUDA_CHECK_TYPE_ALIAS_RESOLVED) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&alias, 0, sizeof(alias));
        alias.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        alias.symbol = declaration->symbol;
        alias.target_type = declaration->type;
        alias.is_public = declaration->is_public;
        if (!ruda_vector_push(&checker->result->type_aliases, &alias)) {
            ruda_owned_bytes_dispose(&alias.name);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}

static void ruda_checked_record_value_dispose(RudaCheckedRecord *record) {
    size_t index = 0;
    if (record == NULL) {
        return;
    }
    for (index = 0; index < record->fields.length; index += 1) {
        RudaCheckedRecordField *field = ruda_vector_at(&record->fields, index);
        if (field != NULL) {
            ruda_owned_bytes_dispose(&field->name);
            memset(field, 0, sizeof(*field));
        }
    }
    ruda_vector_dispose(&record->fields);
    ruda_owned_bytes_dispose(&record->name);
    memset(record, 0, sizeof(*record));
}

static bool ruda_publish_records(RudaChecker *checker) {
    size_t record_index = 0;
    if (!ruda_vector_reserve(
            &checker->result->records,
            checker->records.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (record_index = 0; record_index < checker->records.length;
        record_index += 1) {
        RudaCheckRecordDeclaration *declaration = ruda_vector_at(
            &checker->records,
            record_index
        );
        RudaCheckedRecord record;
        size_t field_index = 0;
        memset(&record, 0, sizeof(record));
        if (declaration == NULL || !declaration->declaration_valid
            || !declaration->fields_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_vector_init(
                &record.fields,
                checker->result->allocator,
                sizeof(RudaCheckedRecordField)
            ) || !ruda_vector_reserve(
                &record.fields,
                declaration->fields.length
            )) {
            ruda_checked_record_value_dispose(&record);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        record.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        record.symbol = declaration->symbol;
        record.type = declaration->type;
        record.is_public = declaration->is_public;
        for (field_index = 0; field_index < declaration->fields.length;
            field_index += 1) {
            RudaCheckRecordFieldDeclaration *source = ruda_vector_at(
                &declaration->fields,
                field_index
            );
            RudaCheckedRecordField field;
            if (source == NULL || !source->name_initialized
                || source->type == RUDA_TYPE_INVALID) {
                ruda_checked_record_value_dispose(&record);
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            memset(&field, 0, sizeof(field));
            field.name = source->owned_name;
            memset(&source->owned_name, 0, sizeof(source->owned_name));
            source->name_initialized = false;
            field.type = source->type;
            field.is_public = source->is_public;
            field.is_mutable = source->is_mutable;
            if (!ruda_vector_push(&record.fields, &field)) {
                ruda_owned_bytes_dispose(&field.name);
                ruda_checked_record_value_dispose(&record);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                return false;
            }
        }
        if (!ruda_vector_push(&checker->result->records, &record)) {
            ruda_checked_record_value_dispose(&record);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&record, 0, sizeof(record));
    }
    return true;
}

static void ruda_checked_enum_variant_value_dispose(
    RudaCheckedEnumVariant *variant
) {
    size_t payload_index = 0;
    if (variant == NULL) {
        return;
    }
    for (payload_index = 0; payload_index < variant->payload.length;
        payload_index += 1) {
        RudaCheckedEnumPayloadItem *item = ruda_vector_at(
            &variant->payload,
            payload_index
        );
        if (item != NULL) {
            ruda_owned_bytes_dispose(&item->name);
            memset(item, 0, sizeof(*item));
        }
    }
    ruda_vector_dispose(&variant->payload);
    ruda_owned_bytes_dispose(&variant->name);
    memset(variant, 0, sizeof(*variant));
}

static void ruda_checked_enum_value_dispose(RudaCheckedEnum *enum_value) {
    size_t variant_index = 0;
    if (enum_value == NULL) {
        return;
    }
    for (variant_index = 0; variant_index < enum_value->variants.length;
        variant_index += 1) {
        RudaCheckedEnumVariant *variant = ruda_vector_at(
            &enum_value->variants,
            variant_index
        );
        ruda_checked_enum_variant_value_dispose(variant);
    }
    ruda_vector_dispose(&enum_value->variants);
    ruda_owned_bytes_dispose(&enum_value->name);
    memset(enum_value, 0, sizeof(*enum_value));
}

static bool ruda_publish_enums(RudaChecker *checker) {
    size_t enum_index = 0;
    if (!ruda_vector_reserve(
            &checker->result->enums,
            checker->enums.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (enum_index = 0; enum_index < checker->enums.length;
        enum_index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &checker->enums,
            enum_index
        );
        RudaCheckedEnum enum_value;
        size_t variant_index = 0;
        memset(&enum_value, 0, sizeof(enum_value));
        if (declaration == NULL || !declaration->declaration_valid
            || !declaration->variants_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_vector_init(
                &enum_value.variants,
                checker->result->allocator,
                sizeof(RudaCheckedEnumVariant)
            ) || !ruda_vector_reserve(
                &enum_value.variants,
                declaration->variants.length
            )) {
            ruda_checked_enum_value_dispose(&enum_value);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        enum_value.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        enum_value.symbol = declaration->symbol;
        enum_value.type = declaration->type;
        enum_value.is_public = declaration->is_public;
        for (variant_index = 0; variant_index < declaration->variants.length;
            variant_index += 1) {
            RudaCheckEnumVariantDeclaration *source_variant = ruda_vector_at(
                &declaration->variants,
                variant_index
            );
            RudaCheckedEnumVariant variant;
            size_t payload_index = 0;
            memset(&variant, 0, sizeof(variant));
            if (source_variant == NULL || !source_variant->name_initialized
                || !source_variant->payload_initialized) {
                ruda_checked_enum_value_dispose(&enum_value);
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            if (!ruda_vector_init(
                    &variant.payload,
                    checker->result->allocator,
                    sizeof(RudaCheckedEnumPayloadItem)
                ) || !ruda_vector_reserve(
                    &variant.payload,
                    source_variant->payload.length
                )) {
                ruda_checked_enum_variant_value_dispose(&variant);
                ruda_checked_enum_value_dispose(&enum_value);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                return false;
            }
            variant.name = source_variant->owned_name;
            memset(
                &source_variant->owned_name,
                0,
                sizeof(source_variant->owned_name)
            );
            source_variant->name_initialized = false;
            variant.kind = source_variant->kind;
            for (payload_index = 0;
                payload_index < source_variant->payload.length;
                payload_index += 1) {
                RudaCheckEnumPayloadItemDeclaration *source_item =
                    ruda_vector_at(&source_variant->payload, payload_index);
                RudaCheckedEnumPayloadItem item;
                if (source_item == NULL || source_item->type == RUDA_TYPE_INVALID
                    || (source_item->has_name
                        && !source_item->name_initialized)) {
                    ruda_checked_enum_variant_value_dispose(&variant);
                    ruda_checked_enum_value_dispose(&enum_value);
                    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                    return false;
                }
                memset(&item, 0, sizeof(item));
                if (source_item->has_name) {
                    item.name = source_item->owned_name;
                    memset(
                        &source_item->owned_name,
                        0,
                        sizeof(source_item->owned_name)
                    );
                    source_item->name_initialized = false;
                }
                item.type = source_item->type;
                item.has_name = source_item->has_name;
                if (!ruda_vector_push(&variant.payload, &item)) {
                    ruda_owned_bytes_dispose(&item.name);
                    ruda_checked_enum_variant_value_dispose(&variant);
                    ruda_checked_enum_value_dispose(&enum_value);
                    ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                    return false;
                }
            }
            if (!ruda_vector_push(&enum_value.variants, &variant)) {
                ruda_checked_enum_variant_value_dispose(&variant);
                ruda_checked_enum_value_dispose(&enum_value);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                return false;
            }
            memset(&variant, 0, sizeof(variant));
        }
        if (!ruda_vector_push(&checker->result->enums, &enum_value)) {
            ruda_checked_enum_value_dispose(&enum_value);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&enum_value, 0, sizeof(enum_value));
    }
    return true;
}

static bool ruda_collect_function_signature(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *parameters = ruda_check_child_node(
        node,
        RUDA_SYNTAX_PARAMETERS,
        0
    );
    const RudaAstNode *return_type_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    RudaCheckFunctionDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t diagnostics_before = checker->result->diagnostics.length;
    size_t index = 0;
    bool attribute_valid = true;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    declaration.block = ruda_check_child_node(node, RUDA_SYNTAX_BLOCK, 0);
    if (name_token == NULL || name_token->synthetic || parameters == NULL
        || return_type_node == NULL || declaration.block == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE) {
            if (ruda_check_attribute_is(checker, element->as.node, "test")) {
                if (declaration.is_test || declaration.is_fuzz
                    || ruda_check_child_node(
                        element->as.node,
                        RUDA_SYNTAX_ARGUMENTS,
                        0
                    ) != NULL) {
                    (void)ruda_checker_add_diagnostic(
                        checker,
                        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                        "@test takes no arguments and cannot be combined with @fuzz",
                        element->as.node->start,
                        element->as.node->end
                    );
                    attribute_valid = false;
                } else {
                    declaration.is_test = true;
                }
                continue;
            }
            if (ruda_check_attribute_is(checker, element->as.node, "fuzz")) {
                if (declaration.is_test || declaration.is_fuzz
                    || ruda_check_child_node(
                        element->as.node,
                        RUDA_SYNTAX_ARGUMENTS,
                        0
                    ) != NULL) {
                    (void)ruda_checker_add_diagnostic(
                        checker,
                        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                        "@fuzz takes no arguments and cannot be combined with @test",
                        element->as.node->start,
                        element->as.node->end
                    );
                    attribute_valid = false;
                } else {
                    declaration.is_fuzz = true;
                }
                continue;
            }
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "attribute is not valid on an ordinary function",
                element->as.node->start,
                element->as.node->end
            );
            attribute_valid = false;
            continue;
        }
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.kind == RUDA_TOKEN_KW_UNSAFE) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->declarations.length >= checker->options.max_functions) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.signature_valid = attribute_valid;
    declaration.is_public = ruda_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    duplicate_name = ruda_declaration_name_is_duplicate(
        checker,
        declaration.name
    );
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.signature_valid = false;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_symbol_id_from_parts(
            checker->package_name,
            ruda_source_path(checker->ast->snapshot),
            (const char *)declaration.owned_name.data,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration.parameter_names_initialized = ruda_vector_init(
        &declaration.parameter_names,
        checker->result->allocator,
        sizeof(RudaCheckName)
    );
    declaration.parameter_types_initialized = ruda_vector_init(
        &declaration.parameter_types,
        checker->result->allocator,
        sizeof(RudaTypeId)
    );
    if (!declaration.parameter_names_initialized
        || !declaration.parameter_types_initialized) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (index = 0; index < parameters->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(parameters, index);
        const RudaSyntaxToken *parameter_name = NULL;
        const RudaAstNode *type_node = NULL;
        RudaCheckName name;
        RudaTypeId type = RUDA_TYPE_INVALID;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            continue;
        }
        if (element->as.node->kind != RUDA_SYNTAX_PARAMETER) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
        parameter_name = ruda_check_child_token(
            element->as.node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        type_node = ruda_check_child_node(
            element->as.node,
            RUDA_SYNTAX_TYPE,
            0
        );
        if (parameter_name == NULL || parameter_name->synthetic
            || type_node == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (declaration.parameter_names.length
            >= checker->options.max_locals_per_function
            || declaration.parameter_types.length
                >= checker->options.mir_options.max_registers) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        name.snapshot = checker->ast->snapshot;
        name.start = parameter_name->start;
        name.end = parameter_name->end;
        if (ruda_parameter_name_is_duplicate(checker, &declaration, name)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "name is already declared in this scope",
                name.start,
                name.end
            );
            declaration.signature_valid = false;
            if (ruda_checker_has_failed(checker)) {
                goto cleanup;
            }
        }
        if (!ruda_check_exact_type(checker, type_node, &type)) {
            if (ruda_checker_has_failed(checker)) {
                goto cleanup;
            }
            declaration.signature_valid = false;
        } else if (declaration.is_public
            && !ruda_check_public_type_reference(checker, type_node)) {
            if (ruda_checker_has_failed(checker)) {
                goto cleanup;
            }
            declaration.signature_valid = false;
        }
        if (!ruda_vector_push(&declaration.parameter_names, &name)
            || !ruda_vector_push(&declaration.parameter_types, &type)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
    }
    if (!ruda_check_exact_type(
            checker,
            return_type_node,
            &declaration.result_type
        )) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.signature_valid = false;
    } else if (declaration.is_public
        && !ruda_check_public_type_reference(checker, return_type_node)) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.signature_valid = false;
    }
    if (checker->result->diagnostics.length != diagnostics_before) {
        declaration.signature_valid = false;
    }
    if (declaration.signature_valid) {
        if ((declaration.is_test || declaration.is_fuzz)
            && !ruda_check_source_is_test_file(checker)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@test and @fuzz require a file ending _test.ruda",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if ((declaration.is_test || declaration.is_fuzz)
            && declaration.is_public) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "test and fuzz functions cannot be public",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if (declaration.is_test
            && (declaration.parameter_types.length != 0
                || declaration.result_type != RUDA_TYPE_VOID)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@test requires a non-generic fn(): void declaration",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if (declaration.is_fuzz
            && (declaration.parameter_types.length != 1
                || declaration.result_type != RUDA_TYPE_VOID)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@fuzz requires exactly one parameter and a void result",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        }
    }
    if (declaration.signature_valid) {
        if (declaration.parameter_types.length == SIZE_MAX
            || declaration.parameter_types.length + 1
                > checker->options.type_options.max_type_arguments) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        if (!ruda_type_intern_function(
                &checker->result->types,
                declaration.parameter_types.data,
                declaration.parameter_types.length,
                declaration.result_type,
                &declaration.signature_type
            )) {
            ruda_checker_fail(
                checker,
                checker->result->types.descriptors.length
                        >= checker->options.type_options.max_types
                            - RUDA_TYPE_FIRST_DYNAMIC
                    ? RUDA_CHECK_LIMIT_REACHED
                    : RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_FUNCTION;
    top_level_entry.index = checker->declarations.length;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->declarations,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
    }
    if (!ruda_vector_push(&checker->declarations, &declaration)) {
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_function_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_collect_constant_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *type_node = ruda_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    const RudaAstNode *initializer = ruda_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaCheckConstantDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t diagnostics_before = checker->result->diagnostics.length;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic || initializer == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->constants.length >= checker->options.max_constants) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.initializer = initializer;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.state = RUDA_CHECK_CONSTANT_UNEVALUATED;
    declaration.declaration_valid = true;
    declaration.is_public = ruda_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    duplicate_name = ruda_declaration_name_is_duplicate(
        checker,
        declaration.name
    );
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_symbol_id_from_parts(
            checker->package_name,
            ruda_source_path(checker->ast->snapshot),
            (const char *)declaration.owned_name.data,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (type_node != NULL
        && !ruda_check_exact_type(checker, type_node, &declaration.type)) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
    } else if (type_node != NULL && declaration.is_public
        && !ruda_check_public_type_reference(checker, type_node)) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
    }
    if (declaration.type != RUDA_TYPE_INVALID
        && declaration.type != RUDA_TYPE_I64
        && declaration.type != RUDA_TYPE_F64
        && declaration.type != RUDA_TYPE_BOOL) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (checker->result->diagnostics.length != diagnostics_before) {
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_CONSTANT;
    top_level_entry.index = checker->constants.length;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->constants,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
    }
    if (!ruda_vector_push(&checker->constants, &declaration)) {
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_constant_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_bind_function_parameters(
    RudaFunctionChecker *function,
    const RudaCheckFunctionDeclaration *declaration
) {
    size_t index = 0;
    if (declaration->parameter_names.length
        != declaration->parameter_types.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < declaration->parameter_names.length; index += 1) {
        const RudaCheckName *name = ruda_vector_at_const(
            &declaration->parameter_names,
            index
        );
        const RudaTypeId *type = ruda_vector_at_const(
            &declaration->parameter_types,
            index
        );
        RudaCheckLocal local;
        if (name == NULL || type == NULL || *type == RUDA_TYPE_INVALID
            || index > UINT32_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        local.name = *name;
        local.type = *type;
        local.register_id = (RudaMirRegister)index;
        local.scope_depth = function->scope_depth;
        local.mutable_binding = false;
        local.initialized = true;
        if (!ruda_vector_push(&function->locals, &local)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
    }
    return true;
}

static bool ruda_check_function_body(
    RudaChecker *checker,
    RudaCheckFunctionDeclaration *declaration
) {
    RudaFunctionChecker function;
    RudaCheckedFunction checked;
    RudaMirVerifyStatus verify_status;
    size_t function_index = 0;
    bool locals_initialized = false;
    bool loops_initialized = false;
    bool accepted = false;
    memset(&function, 0, sizeof(function));
    memset(&checked, 0, sizeof(checked));
    if (declaration == NULL || declaration->ast == NULL
        || !declaration->signature_valid
        || declaration->signature_type == RUDA_TYPE_INVALID
        || declaration->block == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!ruda_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function.checker = checker;
    function.block = RUDA_MIR_NO_BLOCK;
    function.result_type = declaration->result_type;
    function.diagnostic_start = checker->result->diagnostics.length;
    locals_initialized = ruda_vector_init(
        &function.locals,
        checker->result->allocator,
        sizeof(RudaCheckLocal)
    );
    loops_initialized = ruda_vector_init(
        &function.loops,
        checker->result->allocator,
        sizeof(RudaLoopContext)
    );
    if (!locals_initialized || !loops_initialized) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!ruda_bind_function_parameters(&function, declaration)) {
        goto cleanup;
    }
    if (!ruda_mir_function_init(
            &function.mir,
            checker->result->allocator,
            declaration->symbol,
            declaration->parameter_types.data,
            declaration->parameter_types.length,
            function.result_type,
            checker->options.mir_options
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    function.mir_initialized = true;
    if (!ruda_function_add_block(&function, &function.block)) {
        goto cleanup;
    }
    (void)ruda_compile_block(&function, declaration->block);
    if (ruda_checker_has_failed(checker)) {
        goto cleanup;
    }
    if (checker->result->diagnostics.length != function.diagnostic_start) {
        accepted = true;
        goto cleanup;
    }
    if (!function.terminated) {
        if (function.result_type == RUDA_TYPE_VOID) {
            if (!ruda_function_set_return(
                    &function,
                    RUDA_MIR_NO_REGISTER,
                    declaration->block->end,
                    declaration->block->end
                )) {
                goto cleanup;
            }
        } else {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_MISSING_RETURN,
                function.result_type == RUDA_TYPE_NEVER
                    ? "not every reachable path terminates"
                    : "not every reachable path returns a value",
                declaration->block->end,
                declaration->block->end
            );
            accepted = true;
            goto cleanup;
        }
    }
    verify_status = ruda_mir_verify(&function.mir);
    if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (verify_status == RUDA_MIR_VERIFY_LIMIT_REACHED) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function_index = checker->result->functions.length;
    if (ruda_map_get_const(
            &checker->result->function_index,
            function.mir.symbol.bytes,
            sizeof(function.mir.symbol.bytes)
        ) != NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_vector_reserve(
            &checker->result->functions,
            function_index + 1
        ) || !ruda_map_put(
            &checker->result->function_index,
            function.mir.symbol.bytes,
            sizeof(function.mir.symbol.bytes),
            &function_index
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    checked.name = declaration->owned_name;
    memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
    declaration->name_initialized = false;
    checked.signature_type = declaration->signature_type;
    checked.is_public = declaration->is_public;
    checked.is_test = declaration->is_test;
    checked.is_fuzz = declaration->is_fuzz;
    checked.mir = function.mir;
    memset(&function.mir, 0, sizeof(function.mir));
    function.mir_initialized = false;
    if (!ruda_vector_push(&checker->result->functions, &checked)) {
        (void)ruda_map_remove(
            &checker->result->function_index,
            checked.mir.symbol.bytes,
            sizeof(checked.mir.symbol.bytes)
        );
        ruda_owned_bytes_dispose(&checked.name);
        ruda_mir_function_dispose(&checked.mir);
        memset(&checked, 0, sizeof(checked));
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&checked, 0, sizeof(checked));
    accepted = true;

cleanup:
    if (function.mir_initialized) {
        ruda_mir_function_dispose(&function.mir);
    }
    if (locals_initialized) {
        ruda_vector_dispose(&function.locals);
    }
    if (loops_initialized) {
        ruda_vector_dispose(&function.loops);
    }
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_infer_constant_expression_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result
) {
    const RudaAstNode *inner = NULL;
    const RudaSyntaxToken *token = NULL;
    RudaCheckName name;
    RudaCheckConstantDeclaration *constant = NULL;
    if (checker == NULL || node == NULL || result == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (node->kind == RUDA_SYNTAX_EXPRESSION
        || node->kind == RUDA_SYNTAX_PAREN_GROUP) {
        inner = ruda_expression_only_node(node);
        if (inner == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        return ruda_infer_constant_expression_type(checker, inner, result);
    }
    if (node->kind == RUDA_SYNTAX_LITERAL_EXPRESSION) {
        token = ruda_check_first_token(node);
        if (token == NULL || token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (token->kind == RUDA_TOKEN_INTEGER) {
            *result = RUDA_TYPE_I64;
            return true;
        }
        if (token->kind == RUDA_TOKEN_FLOAT) {
            *result = RUDA_TYPE_F64;
            return true;
        }
        if (token->kind == RUDA_TOKEN_KW_TRUE
            || token->kind == RUDA_TOKEN_KW_FALSE) {
            *result = RUDA_TYPE_BOOL;
            return true;
        }
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (node->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        token = ruda_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0);
        if (token == NULL || token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        name.snapshot = checker->ast->snapshot;
        name.start = token->start;
        name.end = token->end;
        constant = ruda_checker_find_constant(checker, name);
        if (constant != NULL) {
            if (!ruda_evaluate_constant(
                    checker,
                    constant,
                    token->start,
                    token->end
                )) {
                return false;
            }
            *result = constant->value.type;
            return true;
        }
        if (ruda_checker_find_top_level(checker, name) != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            token->start,
            token->end
        );
        return false;
    }
    if (node->kind == RUDA_SYNTAX_UNARY_EXPRESSION) {
        token = ruda_check_first_token(node);
        if (token == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (token->kind == RUDA_TOKEN_BANG) {
            *result = RUDA_TYPE_BOOL;
            return true;
        }
        if (token->kind == RUDA_TOKEN_MINUS
            || token->kind == RUDA_TOKEN_TILDE) {
            if (token->kind == RUDA_TOKEN_MINUS) {
                const RudaAstNode *operand = ruda_expression_only_node(node);
                if (operand != NULL && ruda_infer_constant_expression_type(
                        checker,
                        operand,
                        result
                    )) {
                    return true;
                }
                return false;
            }
            *result = RUDA_TYPE_I64;
            return true;
        }
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (node->kind == RUDA_SYNTAX_BINARY_EXPRESSION) {
        const RudaAstNode *left = NULL;
        const RudaAstNode *right = NULL;
        RudaTypeId left_type = RUDA_TYPE_INVALID;
        RudaTypeId right_type = RUDA_TYPE_INVALID;
        if (!ruda_binary_parts(node, &left, &token, &right)) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        switch (token->kind) {
            case RUDA_TOKEN_PLUS:
            case RUDA_TOKEN_MINUS:
            case RUDA_TOKEN_STAR:
            case RUDA_TOKEN_SLASH:
                if (!ruda_infer_constant_expression_type(
                        checker,
                        left,
                        &left_type
                    ) || !ruda_infer_constant_expression_type(
                        checker,
                        right,
                        &right_type
                    )) {
                    return false;
                }
                if (left_type == right_type
                    && (left_type == RUDA_TYPE_I64
                        || left_type == RUDA_TYPE_F64)) {
                    *result = left_type;
                    return true;
                }
                *result = RUDA_TYPE_I64;
                return true;
            case RUDA_TOKEN_PERCENT:
            case RUDA_TOKEN_AMPERSAND:
            case RUDA_TOKEN_CARET:
            case RUDA_TOKEN_PIPE:
            case RUDA_TOKEN_SHIFT_LEFT:
            case RUDA_TOKEN_SHIFT_RIGHT:
                *result = RUDA_TYPE_I64;
                return true;
            case RUDA_TOKEN_LESS:
            case RUDA_TOKEN_LESS_EQUAL:
            case RUDA_TOKEN_GREATER:
            case RUDA_TOKEN_GREATER_EQUAL:
            case RUDA_TOKEN_EQUAL_EQUAL:
            case RUDA_TOKEN_BANG_EQUAL:
            case RUDA_TOKEN_AMPERSAND_AMPERSAND:
            case RUDA_TOKEN_PIPE_PIPE:
                *result = RUDA_TYPE_BOOL;
                return true;
            default:
                ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
                return false;
        }
    }
    if (node->kind == RUDA_SYNTAX_CALL_EXPRESSION) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "function calls are not permitted in this constant subset",
            node->start,
            node->end
        );
        return false;
    }
    ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

static bool ruda_evaluate_constant(
    RudaChecker *checker,
    RudaCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
) {
    const RudaAstTree *previous_ast = NULL;
    RudaFunctionChecker function;
    RudaExpressionValue expression;
    RudaTypeId inferred_type = RUDA_TYPE_INVALID;
    RudaMirVerifyStatus verify_status;
    RudaMirInterpretOptions interpret_options;
    RudaMirInterpretStatus interpret_status;
    bool locals_initialized = false;
    bool accepted = false;
    memset(&function, 0, sizeof(function));
    memset(&expression, 0, sizeof(expression));
    if (checker == NULL || declaration == NULL
        || !declaration->declaration_valid) {
        if (checker != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (declaration->state == RUDA_CHECK_CONSTANT_EVALUATED) {
        return true;
    }
    if (declaration->state == RUDA_CHECK_CONSTANT_INVALID) {
        return false;
    }
    if (declaration->state == RUDA_CHECK_CONSTANT_EVALUATING) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant dependency cycle",
            use_start,
            use_end
        );
        return false;
    }
    if (checker->constant_depth >= checker->options.max_constant_depth) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant dependency depth limit reached",
            use_start,
            use_end
        );
        declaration->state = RUDA_CHECK_CONSTANT_INVALID;
        return false;
    }
    previous_ast = checker->ast;
    declaration->state = RUDA_CHECK_CONSTANT_EVALUATING;
    checker->constant_depth += 1;
    if (!ruda_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_infer_constant_expression_type(
            checker,
            declaration->initializer,
            &inferred_type
        )) {
        goto cleanup;
    }
    if (declaration->type == RUDA_TYPE_INVALID) {
        declaration->type = inferred_type;
    } else if (declaration->type != inferred_type) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "constant initializer type does not match its declaration",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (declaration->type != RUDA_TYPE_I64
        && declaration->type != RUDA_TYPE_F64
        && declaration->type != RUDA_TYPE_BOOL) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    function.checker = checker;
    function.block = RUDA_MIR_NO_BLOCK;
    function.result_type = declaration->type;
    function.diagnostic_start = checker->result->diagnostics.length;
    function.constant_evaluation = true;
    locals_initialized = ruda_vector_init(
        &function.locals,
        checker->result->allocator,
        sizeof(RudaCheckLocal)
    );
    if (!locals_initialized) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!ruda_mir_function_init(
            &function.mir,
            checker->result->allocator,
            declaration->symbol,
            NULL,
            0,
            declaration->type,
            checker->options.mir_options
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    function.mir_initialized = true;
    if (!ruda_function_add_block(&function, &function.block)
        || !ruda_compile_expression(
            &function,
            declaration->initializer,
            declaration->type,
            &expression
        )) {
        goto cleanup;
    }
    if (expression.type != declaration->type) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "constant initializer type does not match its declaration",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (!ruda_function_set_return(
            &function,
            expression.register_id,
            declaration->initializer->end,
            declaration->initializer->end
        )) {
        goto cleanup;
    }
    verify_status = ruda_mir_verify(&function.mir);
    if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (verify_status == RUDA_MIR_VERIFY_LIMIT_REACHED) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    interpret_options = ruda_mir_interpret_default_options();
    interpret_options.max_steps = checker->options.max_constant_steps;
    interpret_options.remaining_shared_steps =
        &checker->remaining_constant_steps;
    interpret_status = ruda_mir_interpret(
        &function.mir,
        NULL,
        0,
        checker->result->allocator,
        interpret_options,
        &declaration->value
    );
    if (interpret_status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (interpret_status == RUDA_MIR_INTERPRET_STEP_LIMIT) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant evaluation step limit reached",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (interpret_status == RUDA_MIR_INTERPRET_OVERFLOW
        || interpret_status == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO
        || interpret_status == RUDA_MIR_INTERPRET_INVALID_SHIFT) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "constant expression can panic",
            declaration->initializer->start,
            declaration->initializer->end
        );
        goto cleanup;
    }
    if (interpret_status != RUDA_MIR_INTERPRET_OK
        || declaration->value.type != declaration->type) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration->state = RUDA_CHECK_CONSTANT_EVALUATED;
    accepted = true;

cleanup:
    if (!accepted) {
        declaration->state = RUDA_CHECK_CONSTANT_INVALID;
    }
    if (function.mir_initialized) {
        ruda_mir_function_dispose(&function.mir);
    }
    if (locals_initialized) {
        ruda_vector_dispose(&function.locals);
    }
    checker->constant_depth -= 1;
    if (previous_ast != NULL && !ruda_checker_set_ast(checker, previous_ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return accepted && !ruda_checker_has_failed(checker);
}

static bool ruda_evaluate_all_constants(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->constants.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckConstantDeclaration *declaration = ruda_vector_at(
            &checker->constants,
            index
        );
        if (declaration == NULL || !ruda_checker_set_ast(
                checker,
                declaration->ast
            ) || !ruda_evaluate_constant(
                checker,
                declaration,
                declaration->name.start,
                declaration->name.end
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

static bool ruda_publish_constants(RudaChecker *checker) {
    size_t index = 0;
    if (!ruda_vector_reserve(
            &checker->result->constants,
            checker->constants.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        RudaCheckConstantDeclaration *declaration = ruda_vector_at(
            &checker->constants,
            index
        );
        RudaCheckedConstant constant;
        if (declaration == NULL
            || declaration->state != RUDA_CHECK_CONSTANT_EVALUATED) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&constant, 0, sizeof(constant));
        constant.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        constant.symbol = declaration->symbol;
        constant.value = declaration->value;
        constant.is_public = declaration->is_public;
        if (!ruda_vector_push(&checker->result->constants, &constant)) {
            ruda_owned_bytes_dispose(&constant.name);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}

RudaCheckOptions ruda_check_default_options(void) {
    RudaCheckOptions options;
    options.max_source_files = 100000;
    options.max_functions = 100000;
    options.max_constants = 100000;
    options.max_type_aliases = 100000;
    options.max_records = 100000;
    options.max_fields_per_record = 100000;
    options.max_enums = 100000;
    options.max_variants_per_enum = 100000;
    options.max_payload_items_per_variant = 100000;
    options.max_locals_per_function = 100000;
    options.max_diagnostics = 1000;
    options.max_constant_steps = 10000000;
    options.max_constant_depth = 1024;
    options.max_type_alias_depth = 1024;
    options.max_float_literal_digits = 4096;
    options.type_options = ruda_type_table_default_options();
    options.mir_options = ruda_mir_function_default_options();
    return options;
}

RudaCheckInterpretOptions ruda_check_interpret_default_options(void) {
    RudaCheckInterpretOptions options;
    options.max_steps_per_function = 10000000;
    options.max_total_steps = 100000000;
    options.max_call_depth = 1024;
    options.execution = NULL;
    options.panic = NULL;
    return options;
}

static void ruda_check_dispose_function_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->functions.length; index += 1) {
        RudaCheckedFunction *function = ruda_vector_at(
            &result->functions,
            index
        );
        if (function != NULL) {
            ruda_owned_bytes_dispose(&function->name);
            ruda_mir_function_dispose(&function->mir);
            memset(function, 0, sizeof(*function));
        }
    }
    result->functions.length = 0;
}

static void ruda_check_dispose_constant_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->constants.length; index += 1) {
        RudaCheckedConstant *constant = ruda_vector_at(
            &result->constants,
            index
        );
        if (constant != NULL) {
            ruda_owned_bytes_dispose(&constant->name);
            memset(constant, 0, sizeof(*constant));
        }
    }
    result->constants.length = 0;
}

static void ruda_check_dispose_type_alias_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->type_aliases.length; index += 1) {
        RudaCheckedTypeAlias *alias = ruda_vector_at(
            &result->type_aliases,
            index
        );
        if (alias != NULL) {
            ruda_owned_bytes_dispose(&alias->name);
            memset(alias, 0, sizeof(*alias));
        }
    }
    result->type_aliases.length = 0;
}

static void ruda_check_dispose_record_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->records.length; index += 1) {
        RudaCheckedRecord *record = ruda_vector_at(&result->records, index);
        ruda_checked_record_value_dispose(record);
    }
    result->records.length = 0;
}

static void ruda_check_dispose_enum_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->enums.length; index += 1) {
        RudaCheckedEnum *enum_value = ruda_vector_at(&result->enums, index);
        ruda_checked_enum_value_dispose(enum_value);
    }
    result->enums.length = 0;
}

static void ruda_check_dispose_diagnostics(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        RudaCheckDiagnostic *diagnostic = ruda_vector_at(
            &result->diagnostics,
            index
        );
        if (diagnostic != NULL) {
            ruda_owned_bytes_dispose(&diagnostic->owned_message);
            memset(diagnostic, 0, sizeof(*diagnostic));
        }
    }
    result->diagnostics.length = 0;
}

static bool ruda_check_function_index_init(RudaCheckResult *result) {
    static const uint64_t key0 = UINT64_C(0x727564612d66756e);
    static const uint64_t key1 = UINT64_C(0x6374696f6e2d6964);
    return ruda_map_init(
        &result->function_index,
        result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

static void ruda_check_clear_functions(RudaCheckResult *result) {
    ruda_check_dispose_function_values(result);
    ruda_map_dispose(&result->function_index);
    (void)ruda_check_function_index_init(result);
}

void ruda_check_result_dispose(RudaCheckResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_check_dispose_function_values(result);
    ruda_check_dispose_constant_values(result);
    ruda_check_dispose_type_alias_values(result);
    ruda_check_dispose_record_values(result);
    ruda_check_dispose_enum_values(result);
    ruda_check_dispose_diagnostics(result);
    ruda_map_dispose(&result->function_index);
    ruda_vector_dispose(&result->constants);
    ruda_vector_dispose(&result->type_aliases);
    ruda_vector_dispose(&result->records);
    ruda_vector_dispose(&result->enums);
    ruda_vector_dispose(&result->functions);
    ruda_vector_dispose(&result->diagnostics);
    ruda_type_table_dispose(&result->types);
    memset(result, 0, sizeof(*result));
}

static void ruda_checker_dispose_declarations(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->declarations.length; index += 1) {
        RudaCheckFunctionDeclaration *declaration = ruda_vector_at(
            &checker->declarations,
            index
        );
        ruda_function_declaration_dispose(declaration);
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        RudaCheckConstantDeclaration *constant = ruda_vector_at(
            &checker->constants,
            index
        );
        ruda_constant_declaration_dispose(constant);
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        RudaCheckTypeAliasDeclaration *alias = ruda_vector_at(
            &checker->type_aliases,
            index
        );
        ruda_type_alias_declaration_dispose(alias);
    }
    for (index = 0; index < checker->records.length; index += 1) {
        RudaCheckRecordDeclaration *record = ruda_vector_at(
            &checker->records,
            index
        );
        ruda_record_declaration_dispose(record);
    }
    for (index = 0; index < checker->enums.length; index += 1) {
        RudaCheckEnumDeclaration *enum_declaration = ruda_vector_at(
            &checker->enums,
            index
        );
        ruda_enum_declaration_dispose(enum_declaration);
    }
    ruda_map_dispose(&checker->top_level_index);
    ruda_vector_dispose(&checker->constants);
    ruda_vector_dispose(&checker->type_aliases);
    ruda_vector_dispose(&checker->records);
    ruda_vector_dispose(&checker->enums);
    ruda_vector_dispose(&checker->declarations);
}

static bool ruda_checker_top_level_index_init(RudaChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d6e616d);
    static const uint64_t key1 = UINT64_C(0x652d696e64657821);
    return ruda_map_init(
        &checker->top_level_index,
        checker->result->allocator,
        sizeof(RudaCheckTopLevelEntry),
        key0,
        key1
    );
}

static int ruda_check_compare_ast_paths(const void *left, const void *right) {
    const RudaAstTree *left_ast = *(const RudaAstTree *const *)left;
    const RudaAstTree *right_ast = *(const RudaAstTree *const *)right;
    const RudaSourceSnapshot *left_snapshot = left_ast->snapshot;
    const RudaSourceSnapshot *right_snapshot = right_ast->snapshot;
    const unsigned char *left_path = (const unsigned char *)ruda_source_path(
        left_snapshot
    );
    const unsigned char *right_path = (const unsigned char *)ruda_source_path(
        right_snapshot
    );
    size_t left_length = ruda_source_path_length(left_snapshot);
    size_t right_length = ruda_source_path_length(right_snapshot);
    size_t common_length = left_length < right_length
        ? left_length
        : right_length;
    int comparison = memcmp(left_path, right_path, common_length);
    if (comparison != 0) {
        return comparison;
    }
    if (left_length < right_length) {
        return -1;
    }
    return left_length > right_length ? 1 : 0;
}

static bool ruda_check_options_valid(
    RudaAllocator allocator,
    RudaCheckOptions options
) {
    return allocator.allocate != NULL && allocator.resize != NULL
        && allocator.deallocate != NULL && options.max_source_files != 0
        && options.max_functions != 0 && options.max_constants != 0
        && options.max_type_aliases != 0
        && options.max_records != 0 && options.max_fields_per_record != 0
        && options.max_enums != 0 && options.max_variants_per_enum != 0
        && options.max_payload_items_per_variant != 0
        && options.max_locals_per_function != 0
        && options.max_diagnostics != 0 && options.max_constant_steps != 0
        && options.max_constant_depth != 0
        && options.max_type_alias_depth != 0
        && options.max_float_literal_digits != 0
        && options.type_options.max_types >= RUDA_TYPE_FIRST_DYNAMIC
        && options.type_options.max_types <= UINT32_MAX
        && options.type_options.max_type_arguments != 0
        && options.mir_options.max_registers != 0
        && options.mir_options.max_registers <= UINT32_MAX
        && options.mir_options.max_blocks != 0
        && options.mir_options.max_blocks <= UINT32_MAX
        && options.mir_options.max_instructions != 0
        && options.mir_options.max_calls != 0
        && options.mir_options.max_calls <= UINT32_MAX
        && options.mir_options.max_call_arguments != 0
        && options.mir_options.max_record_layouts != 0
        && options.mir_options.max_record_layouts <= UINT32_MAX
        && options.mir_options.max_record_fields != 0
        && options.mir_options.max_record_constructions != 0
        && options.mir_options.max_record_constructions <= UINT32_MAX
        && options.mir_options.max_record_arguments != 0
        && options.mir_options.max_enum_layouts != 0
        && options.mir_options.max_enum_layouts <= UINT32_MAX
        && options.mir_options.max_enum_variants != 0
        && options.mir_options.max_enum_payloads != 0
        && options.mir_options.max_enum_constructions != 0
        && options.mir_options.max_enum_constructions <= UINT32_MAX
        && options.mir_options.max_enum_arguments != 0
        && options.mir_options.max_string_constants != 0
        && options.mir_options.max_string_constants <= UINT32_MAX
        && options.mir_options.max_string_bytes != 0
        && options.mir_options.max_verify_work != 0;
}

static bool ruda_check_ast_valid(const RudaAstTree *ast) {
    return ast != NULL && ast->initialized && ast->root != NULL
        && ast->root->kind == RUDA_SYNTAX_ROOT && ast->snapshot != NULL
        && ruda_source_path(ast->snapshot) != NULL
        && ruda_source_path_length(ast->snapshot) != 0
        && (ruda_source_bytes(ast->snapshot) != NULL
            || ruda_source_length(ast->snapshot) == 0);
}

RudaCheckStatus ruda_check_package(
    const RudaAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
) {
    RudaChecker checker;
    RudaVector sorted_asts;
    bool sorted_asts_initialized = false;
    bool diagnostics_initialized = false;
    bool functions_initialized = false;
    bool constants_initialized = false;
    bool type_aliases_initialized = false;
    bool records_initialized = false;
    bool enums_initialized = false;
    bool function_index_initialized = false;
    bool declarations_initialized = false;
    bool constant_declarations_initialized = false;
    bool type_alias_declarations_initialized = false;
    bool record_declarations_initialized = false;
    bool enum_declarations_initialized = false;
    bool top_level_index_initialized = false;
    size_t ast_index = 0;
    size_t index = 0;
    if (result == NULL) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (asts == NULL || ast_count == 0 || package_name == NULL
        || package_name[0] == '\0'
        || !ruda_check_options_valid(allocator, options)) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    if (ast_count > options.max_source_files) {
        return RUDA_CHECK_LIMIT_REACHED;
    }
    sorted_asts_initialized = ruda_vector_init(
        &sorted_asts,
        allocator,
        sizeof(RudaAstTree *)
    );
    if (!sorted_asts_initialized) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    for (ast_index = 0; ast_index < ast_count; ast_index += 1) {
        const RudaAstTree *ast = asts[ast_index];
        if (!ruda_check_ast_valid(ast)) {
            ruda_vector_dispose(&sorted_asts);
            return RUDA_CHECK_INVALID_ARGUMENT;
        }
        if (!ruda_vector_push(&sorted_asts, &ast)) {
            ruda_vector_dispose(&sorted_asts);
            return RUDA_CHECK_ALLOCATION_FAILED;
        }
    }
    qsort(
        sorted_asts.data,
        sorted_asts.length,
        sorted_asts.element_size,
        ruda_check_compare_ast_paths
    );
    for (ast_index = 1; ast_index < sorted_asts.length; ast_index += 1) {
        const RudaAstTree *const *previous = ruda_vector_at_const(
            &sorted_asts,
            ast_index - 1
        );
        const RudaAstTree *const *current = ruda_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (previous == NULL || current == NULL
            || ruda_check_compare_ast_paths(previous, current) == 0) {
            ruda_vector_dispose(&sorted_asts);
            return RUDA_CHECK_INVALID_ARGUMENT;
        }
    }
    memset(&checker, 0, sizeof(checker));
    checker.package_name = package_name;
    checker.options = options;
    checker.result = result;
    checker.failure = RUDA_CHECK_OK;
    checker.remaining_constant_steps = options.max_constant_steps;
    result->allocator = allocator;
    result->snapshot = (*(const RudaAstTree *const *)sorted_asts.data)->snapshot;
    if (!ruda_type_table_init(&result->types, allocator, options.type_options)) {
        ruda_vector_dispose(&sorted_asts);
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    diagnostics_initialized = ruda_vector_init(
        &result->diagnostics,
        allocator,
        sizeof(RudaCheckDiagnostic)
    );
    functions_initialized = ruda_vector_init(
        &result->functions,
        allocator,
        sizeof(RudaCheckedFunction)
    );
    constants_initialized = ruda_vector_init(
        &result->constants,
        allocator,
        sizeof(RudaCheckedConstant)
    );
    type_aliases_initialized = ruda_vector_init(
        &result->type_aliases,
        allocator,
        sizeof(RudaCheckedTypeAlias)
    );
    records_initialized = ruda_vector_init(
        &result->records,
        allocator,
        sizeof(RudaCheckedRecord)
    );
    enums_initialized = ruda_vector_init(
        &result->enums,
        allocator,
        sizeof(RudaCheckedEnum)
    );
    function_index_initialized = ruda_check_function_index_init(result);
    declarations_initialized = ruda_vector_init(
        &checker.declarations,
        allocator,
        sizeof(RudaCheckFunctionDeclaration)
    );
    constant_declarations_initialized = ruda_vector_init(
        &checker.constants,
        allocator,
        sizeof(RudaCheckConstantDeclaration)
    );
    type_alias_declarations_initialized = ruda_vector_init(
        &checker.type_aliases,
        allocator,
        sizeof(RudaCheckTypeAliasDeclaration)
    );
    record_declarations_initialized = ruda_vector_init(
        &checker.records,
        allocator,
        sizeof(RudaCheckRecordDeclaration)
    );
    enum_declarations_initialized = ruda_vector_init(
        &checker.enums,
        allocator,
        sizeof(RudaCheckEnumDeclaration)
    );
    top_level_index_initialized = ruda_checker_top_level_index_init(
        &checker
    );
    if (!diagnostics_initialized || !functions_initialized
        || !constants_initialized || !type_aliases_initialized
        || !records_initialized
        || !enums_initialized
        || !function_index_initialized
        || !declarations_initialized || !constant_declarations_initialized
        || !type_alias_declarations_initialized
        || !record_declarations_initialized
        || !enum_declarations_initialized
        || !top_level_index_initialized) {
        if (top_level_index_initialized) {
            ruda_map_dispose(&checker.top_level_index);
        }
        if (declarations_initialized) {
            ruda_vector_dispose(&checker.declarations);
        }
        if (constant_declarations_initialized) {
            ruda_vector_dispose(&checker.constants);
        }
        if (type_alias_declarations_initialized) {
            ruda_vector_dispose(&checker.type_aliases);
        }
        if (record_declarations_initialized) {
            ruda_vector_dispose(&checker.records);
        }
        if (enum_declarations_initialized) {
            ruda_vector_dispose(&checker.enums);
        }
        if (functions_initialized) {
            ruda_vector_dispose(&result->functions);
        }
        if (constants_initialized) {
            ruda_vector_dispose(&result->constants);
        }
        if (type_aliases_initialized) {
            ruda_vector_dispose(&result->type_aliases);
        }
        if (records_initialized) {
            ruda_vector_dispose(&result->records);
        }
        if (enums_initialized) {
            ruda_vector_dispose(&result->enums);
        }
        if (function_index_initialized) {
            ruda_map_dispose(&result->function_index);
        }
        if (diagnostics_initialized) {
            ruda_vector_dispose(&result->diagnostics);
        }
        ruda_type_table_dispose(&result->types);
        ruda_vector_dispose(&sorted_asts);
        memset(result, 0, sizeof(*result));
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    result->initialized = true;
    for (ast_index = 0; ast_index < sorted_asts.length
        && !ruda_checker_has_failed(&checker); ast_index += 1) {
        const RudaAstTree *const *ast = ruda_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !ruda_checker_set_ast(&checker, *ast)) {
            ruda_checker_fail(&checker, RUDA_CHECK_INTERNAL_ERROR);
            break;
        }
        for (index = 0; index < (*ast)->root->child_count
            && !ruda_checker_has_failed(&checker); index += 1) {
            const RudaAstElement *element = ruda_ast_child(
                (*ast)->root,
                index
            );
            if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE
                || element->as.node == NULL) {
                ruda_checker_fail(&checker, RUDA_CHECK_INVALID_ARGUMENT);
                break;
            }
            if (element->as.node->kind == RUDA_SYNTAX_TYPE_ALIAS_DECL) {
                (void)ruda_collect_type_alias_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_RECORD_DECL) {
                (void)ruda_collect_record_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_ENUM_DECL) {
                (void)ruda_collect_enum_declaration(
                    &checker,
                    element->as.node
                );
            }
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_evaluate_all_type_aliases(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_resolve_all_record_fields(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_resolve_all_enum_variants(&checker);
    }
    for (ast_index = 0; ast_index < sorted_asts.length
        && !ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0; ast_index += 1) {
        const RudaAstTree *const *ast = ruda_vector_at_const(
            &sorted_asts,
            ast_index
        );
        if (ast == NULL || !ruda_checker_set_ast(&checker, *ast)) {
            ruda_checker_fail(&checker, RUDA_CHECK_INTERNAL_ERROR);
            break;
        }
        for (index = 0; index < (*ast)->root->child_count
            && !ruda_checker_has_failed(&checker); index += 1) {
            const RudaAstElement *element = ruda_ast_child(
                (*ast)->root,
                index
            );
            if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE
                || element->as.node == NULL) {
                ruda_checker_fail(&checker, RUDA_CHECK_INVALID_ARGUMENT);
                break;
            }
            if (element->as.node->kind == RUDA_SYNTAX_FUNCTION_DECL) {
                (void)ruda_collect_function_signature(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind == RUDA_SYNTAX_CONST_DECL) {
                (void)ruda_collect_constant_declaration(
                    &checker,
                    element->as.node
                );
            } else if (element->as.node->kind
                == RUDA_SYNTAX_TYPE_ALIAS_DECL) {
                continue;
            } else if (element->as.node->kind == RUDA_SYNTAX_RECORD_DECL) {
                continue;
            } else if (element->as.node->kind == RUDA_SYNTAX_ENUM_DECL) {
                continue;
            } else {
                ruda_checker_fail(&checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
                break;
            }
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_evaluate_all_constants(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_publish_constants(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        for (index = 0; index < checker.declarations.length
            && !ruda_checker_has_failed(&checker); index += 1) {
            RudaCheckFunctionDeclaration *declaration = ruda_vector_at(
                &checker.declarations,
                index
            );
            if (declaration == NULL || !declaration->signature_valid) {
                ruda_checker_fail(&checker, RUDA_CHECK_INTERNAL_ERROR);
                break;
            }
            (void)ruda_check_function_body(&checker, declaration);
        }
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_publish_type_aliases(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_publish_records(&checker);
    }
    if (!ruda_checker_has_failed(&checker)
        && result->diagnostics.length == 0) {
        (void)ruda_publish_enums(&checker);
    }
    if (result->diagnostics.length != 0) {
        ruda_check_clear_functions(result);
        ruda_check_dispose_constant_values(result);
        ruda_check_dispose_type_alias_values(result);
        ruda_check_dispose_record_values(result);
        ruda_check_dispose_enum_values(result);
    }
    ruda_checker_dispose_declarations(&checker);
    ruda_vector_dispose(&sorted_asts);
    if (checker.failure != RUDA_CHECK_OK) {
        RudaCheckStatus status = checker.failure;
        ruda_check_result_dispose(result);
        return status;
    }
    return RUDA_CHECK_OK;
}

RudaCheckStatus ruda_check_ast(
    const RudaAstTree *ast,
    const char *package_name,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
) {
    const RudaAstTree *asts[1];
    asts[0] = ast;
    return ruda_check_package(
        asts,
        1,
        package_name,
        allocator,
        options,
        result
    );
}

const RudaCheckDiagnostic *ruda_check_diagnostic(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->diagnostics, index)
        : NULL;
}

const RudaCheckedFunction *ruda_checked_function(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->functions, index)
        : NULL;
}

const RudaCheckedConstant *ruda_checked_constant(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->constants, index)
        : NULL;
}

const RudaCheckedTypeAlias *ruda_checked_type_alias(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->type_aliases, index)
        : NULL;
}

const RudaCheckedRecord *ruda_checked_record(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->records, index)
        : NULL;
}

const RudaCheckedEnum *ruda_checked_enum(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->enums, index)
        : NULL;
}

typedef struct RudaCheckedInterpretContext {
    const RudaCheckResult *checked;
    RudaAllocator allocator;
    RudaCheckInterpretOptions options;
    RudaMirExecution *execution;
    size_t depth;
    size_t remaining_steps;
} RudaCheckedInterpretContext;

static RudaMirInterpretStatus ruda_check_interpret_checked(
    RudaCheckedInterpretContext *context,
    const RudaCheckedFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
);

static const RudaCheckedFunction *ruda_checked_function_by_symbol(
    const RudaCheckResult *checked,
    RudaSymbolId symbol
) {
    const size_t *index = ruda_map_get_const(
        &checked->function_index,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    const RudaCheckedFunction *function = index != NULL
        ? ruda_checked_function(checked, *index)
        : NULL;
    if (function != NULL
        && ruda_symbol_id_equal(function->mir.symbol, symbol)) {
        return function;
    }
    return NULL;
}

static RudaMirInterpretStatus ruda_check_interpret_call(
    void *opaque,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaCheckedInterpretContext *context = opaque;
    const RudaCheckedFunction *function = NULL;
    RudaMirInterpretStatus status;
    if (context == NULL || result == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (context->depth >= context->options.max_call_depth) {
        return RUDA_MIR_INTERPRET_STEP_LIMIT;
    }
    function = ruda_checked_function_by_symbol(context->checked, callee);
    if (function == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    context->depth += 1;
    status = ruda_check_interpret_checked(
        context,
        function,
        arguments,
        argument_count,
        result
    );
    context->depth -= 1;
    return status;
}

static RudaMirInterpretStatus ruda_check_interpret_checked(
    RudaCheckedInterpretContext *context,
    const RudaCheckedFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    options.max_steps = context->options.max_steps_per_function;
    options.remaining_shared_steps = &context->remaining_steps;
    options.call_context = context;
    options.call = ruda_check_interpret_call;
    options.execution = context->execution;
    options.panic = context->options.panic;
    return ruda_mir_interpret(
        &function->mir,
        arguments,
        argument_count,
        context->allocator,
        options,
        result
    );
}

static bool ruda_checked_type_is_managed(
    const RudaCheckResult *checked,
    RudaTypeId type
) {
    size_t index = 0;
    if (type == RUDA_TYPE_STRING) {
        return true;
    }
    for (index = 0; checked != NULL && index < checked->records.length;
        index += 1) {
        const RudaCheckedRecord *record = ruda_checked_record(checked, index);
        if (record != NULL && record->type == type) {
            return true;
        }
    }
    for (index = 0; checked != NULL && index < checked->enums.length;
        index += 1) {
        const RudaCheckedEnum *enum_value = ruda_checked_enum(checked, index);
        if (enum_value != NULL && enum_value->type == type) {
            return true;
        }
    }
    return false;
}

RudaMirInterpretStatus ruda_check_interpret_function(
    const RudaCheckResult *checked,
    size_t function_index,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaCheckInterpretOptions options,
    RudaMirValue *result
) {
    RudaCheckedInterpretContext context;
    RudaMirExecution local_execution;
    const RudaCheckedFunction *function = ruda_checked_function(
        checked,
        function_index
    );
    RudaMirInterpretStatus status;
    size_t argument_index = 0;
    bool owns_execution = false;
    if (function == NULL || result == NULL || options.max_steps_per_function == 0
        || options.max_total_steps == 0
        || options.max_call_depth == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || (arguments == NULL && argument_count != 0)
        || (options.execution != NULL && !options.execution->initialized)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (options.execution == NULL
        && ruda_checked_type_is_managed(checked, function->mir.result_type)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    for (argument_index = 0; argument_index < argument_count;
        argument_index += 1) {
        if (ruda_checked_type_is_managed(
                checked,
                arguments[argument_index].type
            )) {
            if (options.execution == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (!ruda_mir_execution_owns_reference(
                    options.execution,
                    arguments[argument_index]
                )) {
                return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
        }
    }
    if (options.execution == NULL) {
        if (!ruda_mir_execution_init(
                &local_execution,
                allocator,
                ruda_mir_execution_default_options()
            )) {
            return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        owns_execution = true;
    }
    context.checked = checked;
    context.allocator = allocator;
    context.options = options;
    context.execution = options.execution != NULL
        ? options.execution
        : &local_execution;
    context.depth = 1;
    context.remaining_steps = options.max_total_steps;
    status = ruda_check_interpret_checked(
        &context,
        function,
        arguments,
        argument_count,
        result
    );
    if (owns_execution) {
        ruda_mir_execution_dispose(&local_execution);
    }
    return status;
}
