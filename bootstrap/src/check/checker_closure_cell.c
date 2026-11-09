#include "checker_closure_internal.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_lookup_api.h"
#include "editor_facts.h"

/* Conservative lexical planning is used only to choose storage. Shadowing may
 * retain an unnecessary cell, but never changes binding resolution/captures. */
static bool mentioned_in_closure(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayCheckName name, bool inside) {
    if (node == NULL) return false;
    if (function->capture_scan_work >= function->checker->options.mir_options.max_verify_work) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    function->capture_scan_work += 1;
    inside = inside || node->kind == MMAY_SYNTAX_CLOSURE_EXPRESSION;
    if (inside && name.snapshot == NULL) return true;
    for (size_t i = 0; i < node->child_count; i += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, i);
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (mentioned_in_closure(function, element->as.node, name, inside)) return true;
        } else if (inside && element->as.token.kind == MMAY_TOKEN_IDENTIFIER
            && mmay_checker_private_check_names_equal(function->checker, name,
                (MarimayCheckName){function->checker->ast->snapshot,
                    element->as.token.start, element->as.token.end})) return true;
    }
    return false;
}

static bool option_for_cell(MarimayFunctionChecker *function, MarimayTypeId type,
    MarimayCheckEnumDeclaration *option) {
    const MarimayCheckEnumDeclaration *base = mmay_vector_at_const(&function->checker->prelude_enums, 0);
    if (base == NULL) { mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR); return false; }
    *option = *base;
    if (!mmay_type_intern_nominal(&function->checker->result->types, MMAY_TYPE_KIND_NOMINAL,
            base->symbol, &type, 1, &option->type)) {
        mmay_function_value_intern_failed(function); return false;
    }
    return true;
}

bool mmay_closure_prepare_local(MarimayFunctionChecker *function, MarimayCheckLocal *local) {
    if (!function->capture_scan_done) {
        function->capture_scan_needed = mentioned_in_closure(function, function->body_node, (MarimayCheckName){0}, false);
        function->capture_scan_done = true;
    }
    if (!local->mutable_binding) return !mmay_checker_has_failed(function->checker);
    if (!function->capture_scan_needed
        || !mentioned_in_closure(function, function->body_node, local->name, false))
        return !mmay_checker_has_failed(function->checker);
    MarimaySymbolId symbol;
    MarimayCheckEnumDeclaration option;
    MarimayExpressionValue none, cell;
    MarimayMirRecordId layout;
    MarimayAstNode location = {0};
    location.start = local->name.start; location.end = local->name.end;
    if (!mmay_function_value_symbol(function, local->name.start, 5, &symbol, &local->cell_type)) return false;
    local->cell_payload_type = local->type;
    if (!local->initialized) {
        if (!option_for_cell(function, local->type, &option)) return false;
        local->cell_payload_type = option.type;
    }
    MarimayMirRecordField field = {local->cell_payload_type, true};
    if (!mmay_function_value_record(function, local->cell_type, &field, 1, &layout)) return false;
    if (local->initialized) return mmay_function_add_register(function, local->cell_type, &local->cell_register);
    if (!mmay_checker_private_function_emit_enum_construction(function, &option, 1, NULL, 0,
            location.start, location.end, &none)) return false;
    MarimayMirRecordArgument argument = {none.register_id, none.type};
    if (!mmay_function_value_environment(function, &location, layout, &argument, 1, &cell)) return false;
    local->cell_register = cell.register_id;
    local->cell_constructed = true;
    return true;
}

bool mmay_closure_read_local(MarimayFunctionChecker *function, const MarimayCheckLocal *local,
    size_t start, size_t end, MarimayExpressionValue *result) {
    if (!mmay_check_editor_local_occurrence(function, local, start, end)) return false;
    if (local->cell_type == MMAY_TYPE_INVALID) {
        *result = (MarimayExpressionValue){local->type, local->register_id, true}; return true;
    }
    MarimayCheckLocal saved = *local;
    MarimayCheckEnumDeclaration option;
    MarimayMirRecordId record;
    MarimayMirEnumId enumeration;
    MarimayMirRecordField field = {saved.cell_payload_type, true};
    MarimayExpressionValue wrapped, required;
    if (saved.cell_payload_type == saved.type) {
        return mmay_function_value_record(function, saved.cell_type, &field, 1, &record)
            && mmay_checker_private_function_emit_record_access(function, MMAY_MIR_RECORD_GET,
                saved.type, saved.cell_register, MMAY_MIR_NO_REGISTER, record, 0, start, end, result);
    }
    if (!option_for_cell(function, saved.type, &option)
        || !mmay_function_value_record(function, saved.cell_type, &field, 1, &record)
        || !mmay_checker_private_function_enum_layout(function, &option, &enumeration)
        || !mmay_checker_private_function_emit_record_access(function, MMAY_MIR_RECORD_GET,
            option.type, saved.cell_register, MMAY_MIR_NO_REGISTER, record, 0, start, end, &wrapped)
        || !mmay_checker_private_function_emit_enum_access(function, MMAY_MIR_ENUM_REQUIRE_VARIANT,
            option.type, wrapped.register_id, enumeration, 0, 0, start, end, &required)) return false;
    return mmay_checker_private_function_emit_enum_access(function, MMAY_MIR_ENUM_GET_PAYLOAD,
        saved.type, required.register_id, enumeration, 0, 0, start, end, result);
}

bool mmay_closure_write_local(MarimayFunctionChecker *function, MarimayCheckLocal *local,
    MarimayMirRegister source, size_t start, size_t end) {
    MarimayCheckLocal saved = *local;
    if (saved.cell_type == MMAY_TYPE_INVALID) {
        MarimayMirInstruction copy = {0};
        copy.kind = MMAY_MIR_COPY; copy.target = saved.register_id; copy.first = source;
        copy.span = (MarimayMirSpan){start, end};
        return mmay_function_append_instruction(function, &copy);
    }
    MarimayCheckEnumDeclaration option;
    MarimayMirRecordId record;
    MarimayMirRecordField field = {saved.cell_payload_type, true};
    MarimayMirEnumArgument argument = {source, saved.type};
    MarimayExpressionValue wrapped, ignored;
    if (saved.cell_payload_type == saved.type) {
        if (!mmay_function_value_record(function, saved.cell_type, &field, 1, &record)) return false;
        if (!saved.cell_constructed) {
            MarimayMirInstruction instruction = {0};
            MarimayMirRecordArgument initial = {source, saved.type};
            if (function->mir.record_constructions.length >= function->mir.options.max_record_constructions
                || function->mir.record_arguments.length >= function->mir.options.max_record_arguments) {
                mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
            }
            if (!mmay_mir_add_record_construction(&function->mir, record, &initial, 1,
                    &instruction.record_construction)) {
                mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED); return false;
            }
            instruction.kind = MMAY_MIR_RECORD_NEW;
            instruction.record = record;
            instruction.target = saved.cell_register;
            instruction.span = (MarimayMirSpan){start, end};
            if (!mmay_function_append_instruction(function, &instruction)) return false;
            local->cell_constructed = true;
            return true;
        }
        return mmay_checker_private_function_emit_record_access(function, MMAY_MIR_RECORD_SET,
            saved.type, saved.cell_register, source, record, 0, start, end, &ignored);
    }
    if (!option_for_cell(function, saved.type, &option)
        || !mmay_function_value_record(function, saved.cell_type, &field, 1, &record)
        || !mmay_checker_private_function_emit_enum_construction(function, &option, 0, &argument, 1,
            start, end, &wrapped)) return false;
    return mmay_checker_private_function_emit_record_access(function, MMAY_MIR_RECORD_SET,
        option.type, saved.cell_register, wrapped.register_id, record, 0, start, end, &ignored);
}
