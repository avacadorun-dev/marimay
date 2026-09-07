#include "checker_collection_loop_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_collections_api.h"
#include "checker_expr_tuples_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"

#include <string.h>

static bool mmay_collection_loop_emit(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId target_type,
    MarimayMirRegister first,
    MarimayMirRegister second,
    MarimayMirCollectionId collection,
    size_t start,
    size_t end,
    MarimayMirRegister *target
) {
    MarimayMirInstruction instruction;
    if (!mmay_function_add_register(function, target_type, target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = *target;
    instruction.first = first;
    instruction.second = second;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.collection = collection;
    instruction.span.start = start;
    instruction.span.end = end;
    return mmay_function_append_instruction(function, &instruction);
}

static bool mmay_collection_loop_tuple(
    MarimayFunctionChecker *function,
    MarimayTypeId key_type,
    MarimayMirRegister key,
    MarimayTypeId value_type,
    MarimayMirRegister value,
    size_t start,
    size_t end,
    MarimayTypeId *tuple_type,
    MarimayMirRegister *target
) {
    MarimayTypeId types[2] = {key_type, value_type};
    MarimayMirRecordArgument arguments[2];
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirInstruction instruction;
    arguments[0].register_id = key;
    arguments[0].type = key_type;
    arguments[1].register_id = value;
    arguments[1].type = value_type;
    if (!mmay_type_intern_tuple(
            &function->checker->result->types, types, 2, tuple_type
        ) || !mmay_checker_private_function_tuple_layout(
            function, *tuple_type, &record
        ) || !mmay_mir_add_record_construction(
            &function->mir, record, arguments, 2, &construction
        ) || !mmay_function_add_register(function, *tuple_type, target)) {
        if (!mmay_checker_has_failed(function->checker)) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = *target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.record = record;
    instruction.record_construction = construction;
    instruction.span.start = start;
    instruction.span.end = end;
    return mmay_function_append_instruction(function, &instruction);
}

bool mmay_checker_private_compile_collection_for(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayExpressionValue *iterable,
    bool *resolved
) {
    const MarimayAstNode *header = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_FOR_HEADER, 0
    );
    const MarimayAstNode *body = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_BLOCK, 0
    );
    const MarimayAstNode *iterable_node = mmay_checker_private_check_child_node(
        header, MMAY_SYNTAX_EXPRESSION, 0
    );
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        header, MMAY_TOKEN_IDENTIFIER, 0
    );
    MarimayMirCollectionId collection = MMAY_MIR_NO_COLLECTION;
    MarimayTypeId key_type = MMAY_TYPE_INVALID;
    MarimayTypeId value_type = MMAY_TYPE_INVALID;
    MarimayTypeId item_type = MMAY_TYPE_INVALID;
    MarimayMirRegister generation = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister checked = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister advanced = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister key = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister item_value = MMAY_MIR_NO_REGISTER;
    MarimayExpressionValue index_value;
    MarimayExpressionValue zero;
    MarimayExpressionValue condition;
    MarimayCheckName name;
    MarimayCheckLocal *item = NULL;
    MarimayInitializationSnapshot initial;
    MarimayLoopContext loop;
    MarimayMirBlockId entry = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId condition_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId exit_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_end = MMAY_MIR_NO_BLOCK;
    size_t outer_local_count = function != NULL ? function->locals.length : 0;
    size_t previous_depth = function != NULL ? function->scope_depth : 0;
    bool is_set = false;
    bool scope_entered = false;
    bool initial_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&index_value, 0, sizeof(index_value));
    memset(&zero, 0, sizeof(zero));
    memset(&condition, 0, sizeof(condition));
    memset(&initial, 0, sizeof(initial));
    if (function == NULL || node == NULL || iterable == NULL
        || resolved == NULL) return false;
    *resolved = false;
    if (!mmay_type_map_parts(
            &function->checker->result->types,
            iterable->type,
            &key_type,
            &value_type
        ) && !mmay_type_set_element(
            &function->checker->result->types, iterable->type, &key_type
        )) return true;
    *resolved = true;
    is_set = value_type == MMAY_TYPE_INVALID;
    if (header == NULL || body == NULL || iterable_node == NULL
        || name_token == NULL || name_token->synthetic
        || previous_depth == SIZE_MAX
        || !mmay_checker_private_collection_layout(
            function, iterable->type, iterable_node->start, iterable_node->end,
            &collection, NULL, NULL, NULL
        ) || !mmay_collection_loop_emit(
            function, MMAY_MIR_COLLECTION_GENERATION, MMAY_TYPE_U64,
            iterable->register_id, MMAY_MIR_NO_REGISTER, collection,
            node->start, header->end, &generation
        ) || !mmay_function_emit_instruction(
            function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false,
            header->start, header->start, &index_value
        ) || !mmay_function_emit_instruction(
            function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false,
            header->start, header->start, &zero
        )) goto cleanup;
    function->scope_depth = previous_depth + 1;
    scope_entered = true;
    if (!is_set && !mmay_type_intern_tuple(
            &function->checker->result->types,
            (MarimayTypeId[2]){key_type, value_type}, 2, &item_type
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (is_set) item_type = key_type;
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!mmay_checker_private_function_add_local(
            function, name, item_type, false, true, &item
        ) || item == NULL) goto cleanup;
    entry = function->block;
    initial_initialized = mmay_checker_private_initialization_snapshot_init(
        function, outer_local_count, &initial
    );
    if (!initial_initialized
        || !mmay_checker_private_function_add_block(function, &condition_block)
        || !mmay_checker_private_function_add_block(function, &body_block)
        || !mmay_checker_private_function_add_block(function, &exit_block)
        || !mmay_checker_private_function_set_goto(
            function, entry, condition_block, node->start, node->start
        )) goto cleanup;
    function->block = condition_block;
    function->terminated = false;
    if (!mmay_collection_loop_emit(
            function, MMAY_MIR_COLLECTION_REQUIRE_GENERATION,
            iterable->type, iterable->register_id, generation, collection,
            node->start, header->end, &checked
        ) || !mmay_collection_loop_emit(
            function, MMAY_MIR_COLLECTION_NEXT, MMAY_TYPE_I64,
            checked, index_value.register_id, collection,
            node->start, header->end, &advanced
        ) || !mmay_checker_private_function_emit_copy(
            function, index_value.register_id, advanced,
            header->start, header->end
        ) || !mmay_function_emit_instruction(
            function, MMAY_MIR_LESS_I64, MMAY_TYPE_BOOL,
            zero.register_id, index_value.register_id, 0, false,
            iterable_node->start, iterable_node->end, &condition
        ) || !mmay_checker_private_function_set_branch(
            function, condition_block, condition.register_id,
            body_block, exit_block, iterable_node->start, iterable_node->end
        )) goto cleanup;
    loop.break_target = exit_block;
    loop.continue_target = condition_block;
    loop.scope_depth = function->scope_depth;
    if (function->loops.length >= function->mir.options.max_blocks) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!mmay_vector_push(&function->loops, &loop)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    loop_pushed = true;
    function->block = body_block;
    function->terminated = false;
    if (!mmay_collection_loop_emit(
            function, MMAY_MIR_COLLECTION_CURSOR_KEY, key_type,
            checked, index_value.register_id, collection,
            node->start, header->end, &key
        )) goto cleanup;
    if (is_set) {
        item_value = key;
    } else if (!mmay_collection_loop_emit(
            function, MMAY_MIR_COLLECTION_CURSOR_VALUE, value_type,
            checked, index_value.register_id, collection,
            node->start, header->end, &value
        ) || !mmay_collection_loop_tuple(
            function, key_type, key, value_type, value,
            node->start, header->end, &item_type, &item_value
        )) goto cleanup;
    if (!mmay_checker_private_function_emit_copy(
            function, item->register_id, item_value,
            name_token->start, name_token->end
        ) || !mmay_checker_private_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !mmay_checker_private_function_set_goto(
            function, body_end, condition_block, body->end, body->end
        )) goto cleanup;
    if (!mmay_checker_private_initialization_snapshot_restore(
            function, &initial
        )) goto cleanup;
    function->locals.length = outer_local_count;
    function->scope_depth = previous_depth;
    scope_entered = false;
    function->block = exit_block;
    function->terminated = false;
    accepted = true;

cleanup:
    if (loop_pushed) function->loops.length -= 1;
    if (initial_initialized) {
        mmay_checker_private_initialization_snapshot_dispose(&initial);
    }
    if (scope_entered) {
        function->locals.length = outer_local_count;
        function->scope_depth = previous_depth;
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}
