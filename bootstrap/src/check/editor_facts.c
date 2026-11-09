#include "editor_facts.h"
#include "checker_function_context.h"
#include "checker_core_api.h"

#include <string.h>

typedef struct EditorScope {
    size_t start, end, visible_from;
} EditorScope;

/* This walks only the declaration's containing AST path. Scope choice follows
 * the scope the checker has already created; it does not resolve a source name. */
static void editor_scope(const MarimayAstNode *node, MarimayCheckName name,
    EditorScope *scope, size_t depth) {
    size_t index;
    if (node == NULL || depth > 512 || name.start < node->start || name.end > node->end)
        return;
    if (node->kind == MMAY_SYNTAX_BLOCK || node->kind == MMAY_SYNTAX_MATCH_ARM) {
        scope->start = node->start;
        scope->end = node->end;
        if (node->kind == MMAY_SYNTAX_MATCH_ARM) {
            const MarimayAstNode *pattern = mmay_checker_private_check_child_node(
                node, MMAY_SYNTAX_PATTERN, 0);
            if (pattern != NULL && name.end <= pattern->end) scope->visible_from = pattern->end;
        }
    }
    if (node->kind == MMAY_SYNTAX_FOR_STATEMENT
        || node->kind == MMAY_SYNTAX_WHILE_STATEMENT
        || node->kind == MMAY_SYNTAX_IF_STATEMENT) {
        const MarimayAstNode *block = mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_BLOCK, 0);
        if (block != NULL && name.end <= block->start) {
            scope->start = block->start;
            scope->end = block->end;
            scope->visible_from = block->start;
        }
    }
    if (node->kind == MMAY_SYNTAX_CONST_DECL || node->kind == MMAY_SYNTAX_LET_DECL)
        scope->visible_from = node->end;
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node->start <= name.start && element->as.node->end >= name.end)
            editor_scope(element->as.node, name, scope, depth + 1);
    }
}

static bool editor_field_symbol(MarimaySymbolId record,
    MarimayCheckName name, MarimaySymbolId *symbol) {
    const unsigned char *source = mmay_source_bytes(name.snapshot);
    if (source == NULL || name.start >= name.end
        || name.end > mmay_source_length(name.snapshot)) return false;
    return mmay_symbol_id_from_bytes("mmay/editor/field", 17,
        record.bytes, sizeof(record.bytes), source + name.start,
        name.end - name.start, 0, symbol);
}

bool mmay_check_editor_local(MarimayFunctionChecker *function,
    MarimayCheckLocal *local, bool parameter) {
    unsigned char key[24];
    MarimayCheckSymbolSeed seed = {0};
    EditorScope scope;
    size_t index;
    /* Generic instantiations can bind the same source slot to multiple
     * concrete types. Do not publish a misleading single cursor-local type. */
    if (function->type_arguments != NULL || function->constant_evaluation) return true;
    if (function->body_node == NULL || local->name.snapshot == NULL
        || local->name.start >= local->name.end) return true;
    memcpy(key, function->source_symbol.bytes, sizeof(function->source_symbol.bytes));
    for (index = 0; index < 8; index += 1)
        key[16 + index] = (unsigned char)((uint64_t)local->name.start >> (8 * index));
    if (!mmay_symbol_id_from_bytes("mmay/editor/local", 17,
            mmay_source_path(local->name.snapshot), mmay_source_path_length(local->name.snapshot),
            key, sizeof(key), 0, &local->editor_symbol)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    scope = (EditorScope){function->body_node->start, function->body_node->end,
        parameter ? function->body_node->start : local->name.end};
    if (!parameter) editor_scope(function->body_node, local->name, &scope, 0);
    seed.name = local->name;
    seed.symbol = local->editor_symbol;
    seed.type = local->type;
    seed.kind = parameter ? MMAY_CHECKED_SYMBOL_PARAMETER : MMAY_CHECKED_SYMBOL_LOCAL;
    seed.owner_symbol = function->source_symbol;
    seed.scope_start = scope.start;
    seed.scope_end = scope.end;
    seed.visible_from = scope.visible_from;
    seed.scope_depth = function->scope_depth;
    if (!mmay_check_append_symbol_seed(function->checker, &seed)) return false;
    local->editor_symbol_valid = true;
    return true;
}

bool mmay_check_editor_local_occurrence(MarimayFunctionChecker *function,
    const MarimayCheckLocal *local, size_t start, size_t end) {
    MarimayCheckName name = {function->checker->ast->snapshot, start, end};
    const unsigned char *source = mmay_source_bytes(name.snapshot);
    const unsigned char *declaration = mmay_source_bytes(local->name.snapshot);
    if (!local->editor_symbol_valid) return true;
    /* Some MIR cell helpers receive an initializer/whole-expression span.
     * Only an exact resolved source-name occurrence is an editor reference. */
    if (source == NULL || declaration == NULL || start >= end
        || end > mmay_source_length(name.snapshot)
        || end - start != local->name.end - local->name.start
        || memcmp(source + start, declaration + local->name.start, end - start) != 0)
        return true;
    return mmay_check_record_occurrence(function->checker, name, local->editor_symbol);
}

bool mmay_check_editor_field_occurrence(MarimayChecker *checker,
    MarimaySymbolId record, MarimayCheckName name) {
    MarimaySymbolId symbol;
    if (!editor_field_symbol(record, name, &symbol)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return mmay_check_record_occurrence(checker, name, symbol);
}

bool mmay_check_editor_publish_fields(MarimayChecker *checker) {
    size_t record_index, field_index;
    for (record_index = 0; record_index < checker->records.length; record_index += 1) {
        const MarimayCheckRecordDeclaration *record = mmay_vector_at_const(
            &checker->records, record_index);
        for (field_index = 0; field_index < record->fields.length; field_index += 1) {
            const MarimayCheckRecordFieldDeclaration *field = mmay_vector_at_const(
                &record->fields, field_index);
            MarimayCheckSymbolSeed seed = {0};
            seed.name = field->name;
            seed.type = field->type;
            seed.kind = MMAY_CHECKED_SYMBOL_FIELD;
            seed.is_public = field->is_public;
            seed.owner_symbol = record->symbol;
            if (!editor_field_symbol(record->symbol, field->name, &seed.symbol)) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
            if (!mmay_check_append_symbol_seed(checker, &seed)) return false;
        }
    }
    return true;
}
