#include "mmay/bytecode/bytecode.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/utf8.h"
#include "bytecode_internal.h"
#include "bytecode_verify_provenance.h"

static bool mmay_bytecode_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (used == NULL || !mmay_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_with_work_limit(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
) {
    size_t register_count = 0;
    size_t block_count = 0;
    size_t instruction_total = 0;
    size_t edge_count = 0;
    size_t cell_count = 0;
    size_t block_index = 0;
    size_t work = 0;
    size_t *predecessor_counts = NULL;
    size_t *predecessor_offsets = NULL;
    size_t *predecessor_cursors = NULL;
    uint32_t *predecessors = NULL;
    unsigned char *reachable = NULL;
    uint32_t *queue = NULL;
    unsigned char *incoming = NULL;
    unsigned char *outgoing = NULL;
    unsigned char *scratch = NULL;
    MarimayBytecodeProvenanceAnalysis *provenance = NULL;
    size_t predecessor_counts_size = 0;
    size_t predecessor_offsets_size = 0;
    size_t predecessor_cursors_size = 0;
    size_t predecessors_size = 0;
    size_t reachable_size = 0;
    size_t queue_size = 0;
    size_t cells_size = 0;
    size_t scratch_size = 0;
    size_t work_limit = 0;
    bool has_checked_index_operations = false;
    bool has_required_variant_operations = false;
    bool has_defer_operations = false;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (work_used == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    *work_used = 0;
    if (function == NULL || !function->initialized
        || !mmay_bytecode_type_is_result(function->result_type)
        || !mmay_bytecode_type_layout_is_valid(
            function,
            function->result_type,
            function->result_layout
        )
        || function->source_count == 0
        || function->source_count > function->options.max_source_files
        || function->parameter_count > function->register_types.length
        || function->register_types.length > function->options.max_registers
        || function->register_types.length > UINT32_MAX
        || function->blocks.length == 0
        || function->blocks.length > function->options.max_blocks
        || function->blocks.length > UINT32_MAX
        || function->instruction_count > function->options.max_instructions
        || function->calls.length > function->options.max_calls
        || function->calls.length > UINT32_MAX
        || function->call_arguments.length
            > function->options.max_call_arguments
        || function->string_constants.length
            > function->options.max_string_constants
        || function->string_constants.length > UINT32_MAX
        || function->string_bytes > function->options.max_string_bytes
        || function->array_constructions.length
            > function->options.max_array_constructions
        || function->array_constructions.length > UINT32_MAX
        || function->array_arguments.length
            > function->options.max_array_arguments
        || function->options.max_registers == 0
        || function->options.max_blocks == 0
        || function->options.max_instructions == 0
        || function->options.max_calls == 0
        || function->options.max_call_arguments == 0
        || function->options.max_string_constants == 0
        || function->options.max_string_bytes == 0
        || function->options.max_array_constructions == 0
        || function->options.max_array_arguments == 0
        || function->options.max_source_files == 0
        || function->options.max_encoded_bytes < MMAY_BYTECODE_MAGIC_LENGTH
        || function->options.max_verify_work == 0) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (available_work == 0) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    work_limit = available_work < function->options.max_verify_work
        ? available_work
        : function->options.max_verify_work;
    register_count = function->register_types.length;
    block_count = function->blocks.length;
    {
        size_t array_index = 0;
        for (array_index = 0; array_index < function->array_types.length;
            array_index += 1) {
            const MarimayBytecodeArrayType *array_type =
                mmay_bytecode_array_type(function, (uint32_t)array_index);
            if (array_type == NULL
                || !mmay_bytecode_type_layout_is_valid(
                    function,
                    array_type->element_type,
                    array_type->element_layout
                ) || (array_type->element_type == MMAY_BYTECODE_TYPE_ARRAY
                    && array_type->element_layout >= array_index)) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_bytecode_consume_verify_work(&work, 1, work_limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
    }
    {
        size_t construction_index = 0;
        size_t expected_argument_start = 0;
        for (construction_index = 0;
            construction_index < function->array_constructions.length;
            construction_index += 1) {
            const MarimayBytecodeArrayConstruction *construction =
                mmay_bytecode_array_construction(
                    function,
                    (MarimayBytecodeArrayConstructionId)construction_index
                );
            uint32_t element_layout = MMAY_BYTECODE_NO_LAYOUT;
            MarimayBytecodeType element_type = construction != NULL
                ? mmay_bytecode_function_array_element_type(
                    function,
                    construction->array_type,
                    construction->array_layout,
                    &element_layout
                )
                : MMAY_BYTECODE_TYPE_INVALID;
            size_t argument_end = 0;
            size_t argument_index = 0;
            if (construction == NULL
                || element_type == MMAY_BYTECODE_TYPE_INVALID
                || !mmay_bytecode_type_layout_is_valid(
                    function, construction->array_type,
                    construction->array_layout
                )
                || construction->argument_start != expected_argument_start
                || !mmay_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->array_arguments.length) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_bytecode_consume_verify_work(&work, 1, work_limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
            for (argument_index = construction->argument_start;
                argument_index < argument_end; argument_index += 1) {
                const MarimayBytecodeArrayArgument *argument =
                    mmay_bytecode_array_argument(function, argument_index);
                const MarimayBytecodeType *register_type = argument != NULL
                        && argument->register_id < register_count
                    ? mmay_vector_at_const(
                        &function->register_types, argument->register_id
                    )
                    : NULL;
                if (argument == NULL || register_type == NULL
                    || argument->type != element_type
                    || *register_type != element_type
                    || (construction->array_type == MMAY_BYTECODE_TYPE_ARRAY
                        && (argument->layout != element_layout
                            || mmay_bytecode_register_layout(
                                function, argument->register_id
                            ) != element_layout))) {
                    return register_type == NULL
                        ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
                        : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
                }
                if (!mmay_bytecode_consume_verify_work(&work, 1, work_limit)) {
                    return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                }
            }
            expected_argument_start = argument_end;
        }
        if (expected_argument_start != function->array_arguments.length) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    {
        size_t verified_string_bytes = 0;
        size_t string_index = 0;
        for (string_index = 0;
            string_index < function->string_constants.length;
            string_index += 1) {
            const MarimayBytecodeStringConstant *constant =
                mmay_bytecode_string_constant(
                    function,
                    (MarimayBytecodeStringId)string_index
                );
            size_t string_work = 0;
            if (constant == NULL || constant->value.data == NULL
                || !mmay_utf8_validate(
                    (const char *)constant->value.data,
                    constant->value.length,
                    NULL
                ) || !mmay_size_add(
                    verified_string_bytes,
                    constant->value.length,
                    &verified_string_bytes
                ) || !mmay_size_add(
                    constant->value.length,
                    1,
                    &string_work
                )) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_bytecode_consume_verify_work(
                    &work,
                    string_work,
                    work_limit
                )) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        if (verified_string_bytes != function->string_bytes
            || verified_string_bytes > function->options.max_string_bytes) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    {
        size_t nominal_work = 0;
        status = mmay_bytecode_verify_nominal_metadata(
            function,
            work_limit - work,
            &nominal_work
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            return status;
        }
        work += nominal_work;
    }
    for (block_index = 0; block_index < register_count; block_index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            block_index
        );
        if (type == NULL || !mmay_bytecode_type_is_local(*type)
            || (block_index < function->parameter_count
                && !mmay_bytecode_type_is_register(*type))) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    {
        size_t call_index = 0;
        size_t expected_argument_start = 0;
        for (call_index = 0; call_index < function->calls.length;
            call_index += 1) {
            const MarimayBytecodeCall *call = mmay_vector_at_const(
                &function->calls,
                call_index
            );
            size_t argument_index = 0;
            size_t argument_end = 0;
            if (call == NULL || !mmay_bytecode_type_is_result(call->result_type)
                || !mmay_bytecode_type_layout_is_valid(
                    function,
                    call->result_type,
                    call->result_layout
                )
                || call->argument_start != expected_argument_start
                || !mmay_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_bytecode_consume_verify_work(
                    &work,
                    1,
                    work_limit
                )) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
            for (argument_index = call->argument_start;
                argument_index < argument_end;
                argument_index += 1) {
                const MarimayBytecodeCallArgument *argument = mmay_vector_at_const(
                    &function->call_arguments,
                    argument_index
                );
                const MarimayBytecodeType *register_type = argument != NULL
                        && argument->register_id < register_count
                    ? mmay_vector_at_const(
                        &function->register_types,
                        argument->register_id
                    )
                    : NULL;
                if (argument == NULL || register_type == NULL
                    || !mmay_bytecode_type_is_register(argument->type)
                    || *register_type != argument->type
                    || mmay_bytecode_register_layout(
                        function,
                        argument->register_id
                    ) != argument->layout) {
                    return register_type == NULL
                        ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
                        : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
                }
                if (!mmay_bytecode_consume_verify_work(
                        &work,
                        1,
                        work_limit
                    )) {
                    return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                }
            }
            expected_argument_start = argument_end;
        }
        if (expected_argument_start != function->call_arguments.length) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    if (!mmay_size_multiply(block_count, sizeof(size_t), &predecessor_counts_size)
        || !mmay_size_add(block_count, 1, &predecessor_offsets_size)
        || !mmay_size_multiply(
            predecessor_offsets_size,
            sizeof(size_t),
            &predecessor_offsets_size
        ) || !mmay_size_multiply(
            block_count,
            sizeof(size_t),
            &predecessor_cursors_size
        ) || !mmay_size_multiply(block_count, sizeof(unsigned char), &reachable_size)
        || !mmay_size_multiply(block_count, sizeof(uint32_t), &queue_size)) {
        return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    predecessor_counts = mmay_allocate_array(
        &function->allocator,
        block_count,
        sizeof(size_t)
    );
    if (predecessor_counts == NULL) {
        return MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED;
    }
    memset(predecessor_counts, 0, predecessor_counts_size);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL || !block->initialized
            || block->instructions.length == 0
            || !mmay_size_add(
                instruction_total,
                block->instructions.length,
                &instruction_total
            ) || instruction_total > function->options.max_instructions) {
            status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                instruction_index
            );
            const MarimayOpcodeMetadata *metadata = instruction != NULL
                ? mmay_opcode_metadata(instruction->opcode)
                : NULL;
            bool should_terminate = instruction_index + 1
                == block->instructions.length;
            size_t operand_index = 0;
            size_t instruction_work = 1;
            if (metadata == NULL
                || (metadata->flags & MMAY_OPCODE_FLAG_INVALID) != 0) {
                status = MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
                goto cleanup;
            }
            if (instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX
                || instruction->opcode == MMAY_OPCODE_ARRAY_SET
                || instruction->opcode == MMAY_OPCODE_ARRAY_GET_IN_BOUNDS
                || instruction->opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS) {
                has_checked_index_operations = true;
            }
            if (instruction->opcode == MMAY_OPCODE_ENUM_REQUIRE_VARIANT
                || instruction->opcode == MMAY_OPCODE_ENUM_GET_PAYLOAD) {
                has_required_variant_operations = true;
            }
            if (instruction->opcode == MMAY_OPCODE_DEFER_REGISTER
                || instruction->opcode == MMAY_OPCODE_DEFER_CALL) {
                has_defer_operations = true;
            }
            if (((metadata->flags & MMAY_OPCODE_FLAG_TERMINATOR) != 0)
                != should_terminate) {
                status = MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            if (instruction->span.source >= function->source_count
                || instruction->span.end < instruction->span.start) {
                status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                MarimayBytecodeOperandKind kind = metadata->operands[operand_index];
                if (kind == MMAY_BYTECODE_OPERAND_REGISTER
                    || kind == MMAY_BYTECODE_OPERAND_BLOCK
                    || kind == MMAY_BYTECODE_OPERAND_CALL
                    || kind == MMAY_BYTECODE_OPERAND_STRING
                    || kind == MMAY_BYTECODE_OPERAND_ARRAY_CONSTRUCTION
                    || kind == MMAY_BYTECODE_OPERAND_BOOL) {
                    uint64_t operand = instruction->operands[operand_index]
                        .unsigned_value;
                    if (kind == MMAY_BYTECODE_OPERAND_REGISTER
                        && operand >= register_count) {
                        status = MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                    if (kind == MMAY_BYTECODE_OPERAND_BLOCK
                        && operand >= block_count) {
                        status = MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                        goto cleanup;
                    }
                    if (kind == MMAY_BYTECODE_OPERAND_CALL) {
                        const MarimayBytecodeCall *call = operand
                                < function->calls.length
                            ? mmay_vector_at_const(
                                &function->calls,
                                (size_t)operand
                            )
                            : NULL;
                        if (call == NULL) {
                            status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
                            goto cleanup;
                        }
                        if (!mmay_size_add(
                                instruction_work,
                                call->argument_count,
                                &instruction_work
                            )) {
                            status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                            goto cleanup;
                        }
                    }
                    if (kind == MMAY_BYTECODE_OPERAND_STRING
                        && operand >= function->string_constants.length) {
                        status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    if (kind == MMAY_BYTECODE_OPERAND_ARRAY_CONSTRUCTION) {
                        const MarimayBytecodeArrayConstruction *construction =
                            operand < function->array_constructions.length
                            ? mmay_bytecode_array_construction(
                                function,
                                (MarimayBytecodeArrayConstructionId)operand
                            )
                            : NULL;
                        if (construction == NULL) {
                            status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
                            goto cleanup;
                        }
                        if (!mmay_size_add(
                                instruction_work,
                                construction->argument_count,
                                &instruction_work
                            )) {
                            status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                            goto cleanup;
                        }
                    }
                    if (kind == MMAY_BYTECODE_OPERAND_BOOL && operand > 1) {
                        status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                }
            }
            if (!mmay_bytecode_consume_verify_work(
                    &work,
                    instruction_work,
                    work_limit
                )) {
                status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
        {
            const MarimayBytecodeInstruction *terminator =
                mmay_bytecode_block_terminator(block);
            if (terminator->opcode == MMAY_OPCODE_JUMP) {
                size_t target = (size_t)terminator->operands[0].unsigned_value;
                if (!mmay_size_add(predecessor_counts[target], 1,
                        &predecessor_counts[target])
                    || !mmay_size_add(edge_count, 1, &edge_count)) {
                    status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (terminator->opcode == MMAY_OPCODE_BRANCH_BOOL) {
                size_t then_block = (size_t)terminator->operands[1].unsigned_value;
                size_t otherwise = (size_t)terminator->operands[2].unsigned_value;
                if (!mmay_size_add(predecessor_counts[then_block], 1,
                        &predecessor_counts[then_block])
                    || !mmay_size_add(predecessor_counts[otherwise], 1,
                        &predecessor_counts[otherwise])
                    || !mmay_size_add(edge_count, 2, &edge_count)) {
                    status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
        }
    }
    if (instruction_total != function->instruction_count) {
        status = MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        goto cleanup;
    }
    if (has_defer_operations) {
        size_t defer_work = 0;
        status = mmay_bytecode_verify_defer_control_flow(
            function, work_limit - work, &defer_work
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            goto cleanup;
        }
        work += defer_work;
    }
    predecessor_offsets = mmay_allocate_array(
        &function->allocator,
        block_count + 1,
        sizeof(size_t)
    );
    predecessor_cursors = mmay_allocate_array(
        &function->allocator,
        block_count,
        sizeof(size_t)
    );
    if (!mmay_size_multiply(
            edge_count == 0 ? 1 : edge_count,
            sizeof(uint32_t),
            &predecessors_size
        )) {
        status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    predecessors = mmay_allocate_array(
        &function->allocator,
        edge_count == 0 ? 1 : edge_count,
        sizeof(uint32_t)
    );
    reachable = mmay_allocate_array(
        &function->allocator,
        block_count,
        sizeof(unsigned char)
    );
    queue = mmay_allocate_array(
        &function->allocator,
        block_count,
        sizeof(uint32_t)
    );
    if (predecessor_offsets == NULL || predecessor_cursors == NULL
        || predecessors == NULL || reachable == NULL || queue == NULL) {
        status = MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    predecessor_offsets[0] = 0;
    for (block_index = 0; block_index < block_count; block_index += 1) {
        if (!mmay_size_add(
                predecessor_offsets[block_index],
                predecessor_counts[block_index],
                &predecessor_offsets[block_index + 1]
            )) {
            status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        predecessor_cursors[block_index] = predecessor_offsets[block_index];
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            block_index
        );
        const MarimayBytecodeInstruction *terminator =
            mmay_bytecode_block_terminator(block);
        if (terminator->opcode == MMAY_OPCODE_JUMP) {
            size_t target = (size_t)terminator->operands[0].unsigned_value;
            predecessors[predecessor_cursors[target]] = (uint32_t)block_index;
            predecessor_cursors[target] += 1;
        } else if (terminator->opcode == MMAY_OPCODE_BRANCH_BOOL) {
            size_t then_block = (size_t)terminator->operands[1].unsigned_value;
            size_t otherwise = (size_t)terminator->operands[2].unsigned_value;
            predecessors[predecessor_cursors[then_block]] = (uint32_t)block_index;
            predecessor_cursors[then_block] += 1;
            predecessors[predecessor_cursors[otherwise]] = (uint32_t)block_index;
            predecessor_cursors[otherwise] += 1;
        }
    }
    memset(reachable, 0, reachable_size);
    {
        size_t queue_read = 0;
        size_t queue_write = 1;
        queue[0] = 0;
        reachable[0] = 1;
        while (queue_read < queue_write) {
            uint32_t current = queue[queue_read];
            const MarimayBytecodeBlock *block = mmay_vector_at_const(
                &function->blocks,
                current
            );
            const MarimayBytecodeInstruction *terminator =
                mmay_bytecode_block_terminator(block);
            uint64_t targets[2];
            size_t target_count = 0;
            size_t target_index = 0;
            queue_read += 1;
            if (terminator->opcode == MMAY_OPCODE_JUMP) {
                targets[0] = terminator->operands[0].unsigned_value;
                target_count = 1;
            } else if (terminator->opcode == MMAY_OPCODE_BRANCH_BOOL) {
                targets[0] = terminator->operands[1].unsigned_value;
                targets[1] = terminator->operands[2].unsigned_value;
                target_count = 2;
            }
            for (target_index = 0; target_index < target_count;
                target_index += 1) {
                size_t target = (size_t)targets[target_index];
                if (reachable[target] == 0) {
                    reachable[target] = 1;
                    queue[queue_write] = (uint32_t)target;
                    queue_write += 1;
                }
            }
            if (!mmay_bytecode_consume_verify_work(
                    &work,
                    target_count + 1,
                    work_limit
                )) {
                status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        if (reachable[block_index] == 0) {
            status = MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
    }
    if (!mmay_size_multiply(block_count, register_count, &cell_count)
        || cell_count > work_limit
        || !mmay_size_multiply(
            cell_count == 0 ? 1 : cell_count,
            sizeof(unsigned char),
            &cells_size
        ) || !mmay_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(unsigned char),
            &scratch_size
        )) {
        status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    incoming = mmay_allocate_array(
        &function->allocator,
        cell_count == 0 ? 1 : cell_count,
        sizeof(unsigned char)
    );
    outgoing = mmay_allocate_array(
        &function->allocator,
        cell_count == 0 ? 1 : cell_count,
        sizeof(unsigned char)
    );
    scratch = mmay_allocate_array(
        &function->allocator,
        register_count == 0 ? 1 : register_count,
        sizeof(unsigned char)
    );
    if (incoming == NULL || outgoing == NULL || scratch == NULL) {
        status = MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (cell_count != 0) {
        memset(incoming, 1, cells_size);
        memset(outgoing, 1, cells_size);
        memset(incoming, 0, register_count);
        memset(outgoing, 0, register_count);
        memset(incoming, 1, function->parameter_count);
        memcpy(outgoing, incoming, register_count);
        mmay_bytecode_transfer_assignments(
            mmay_vector_at_const(&function->blocks, 0),
            outgoing,
            register_count
        );
    }
    {
        bool changed = false;
        do {
            changed = false;
            for (block_index = 1; block_index < block_count; block_index += 1) {
                size_t predecessor_index = 0;
                unsigned char *incoming_row = incoming
                    + block_index * register_count;
                unsigned char *outgoing_row = outgoing
                    + block_index * register_count;
                const MarimayBytecodeBlock *block = mmay_vector_at_const(
                    &function->blocks,
                    block_index
                );
                if (register_count != 0) {
                    memset(scratch, 1, register_count);
                }
                for (predecessor_index = predecessor_offsets[block_index];
                    predecessor_index < predecessor_offsets[block_index + 1];
                    predecessor_index += 1) {
                    size_t register_index = 0;
                    const unsigned char *predecessor_row = outgoing
                        + (size_t)predecessors[predecessor_index] * register_count;
                    for (register_index = 0; register_index < register_count;
                        register_index += 1) {
                        scratch[register_index] = (unsigned char)(
                            scratch[register_index]
                            & predecessor_row[register_index]
                        );
                    }
                    if (!mmay_bytecode_consume_verify_work(
                            &work,
                            register_count,
                            work_limit
                        )) {
                        status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                        goto cleanup;
                    }
                }
                if (register_count != 0
                    && memcmp(incoming_row, scratch, register_count) != 0) {
                    memcpy(incoming_row, scratch, register_count);
                    changed = true;
                }
                mmay_bytecode_transfer_assignments(
                    block,
                    scratch,
                    register_count
                );
                if (register_count != 0
                    && memcmp(outgoing_row, scratch, register_count) != 0) {
                    memcpy(outgoing_row, scratch, register_count);
                    changed = true;
                }
                if (!mmay_bytecode_consume_verify_work(
                        &work,
                        register_count * 2,
                        work_limit
                    )) {
                    status = MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
        } while (changed);
    }
    {
        size_t provenance_work = 0;
        status = mmay_bytecode_provenance_analyze(
            function,
            predecessor_offsets,
            predecessors,
            has_checked_index_operations,
            has_required_variant_operations,
            work_limit - work,
            &provenance_work,
            &provenance
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            goto cleanup;
        }
        work += provenance_work;
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (register_count != 0) {
            memcpy(
                scratch,
                incoming + block_index * register_count,
                register_count
            );
        }
        mmay_bytecode_provenance_begin_block(provenance, block_index);
        for (instruction_index = 0;
            instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                instruction_index
            );
            status = mmay_bytecode_provenance_validate_instruction(
                provenance,
                instruction
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                goto cleanup;
            }
            status = mmay_bytecode_verify_value_instruction(
                function,
                instruction,
                scratch
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                goto cleanup;
            }
            {
                size_t provenance_work = 0;
                status = mmay_bytecode_provenance_advance_instruction(
                    provenance,
                    instruction,
                    work_limit - work,
                    &provenance_work
                );
                if (status != MMAY_BYTECODE_VERIFY_OK) {
                    goto cleanup;
                }
                work += provenance_work;
            }
        }
        status = mmay_bytecode_verify_terminator(
            function,
            mmay_bytecode_block_terminator(block),
            scratch
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            goto cleanup;
        }
    }
    {
        size_t integer_range_work = 0;
        status = mmay_bytecode_verify_integer_range_proofs(
            function, work_limit - work, &integer_range_work
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            goto cleanup;
        }
        work += integer_range_work;
    }

cleanup:
    mmay_bytecode_provenance_dispose(provenance);
    mmay_deallocate(
        &function->allocator,
        scratch,
        scratch_size
    );
    mmay_deallocate(
        &function->allocator,
        outgoing,
        cells_size
    );
    mmay_deallocate(
        &function->allocator,
        incoming,
        cells_size
    );
    mmay_deallocate(
        &function->allocator,
        queue,
        queue_size
    );
    mmay_deallocate(
        &function->allocator,
        reachable,
        reachable_size
    );
    mmay_deallocate(
        &function->allocator,
        predecessors,
        predecessors_size
    );
    mmay_deallocate(
        &function->allocator,
        predecessor_cursors,
        predecessor_cursors_size
    );
    mmay_deallocate(
        &function->allocator,
        predecessor_offsets,
        predecessor_offsets_size
    );
    mmay_deallocate(
        &function->allocator,
        predecessor_counts,
        predecessor_counts_size
    );
    if (status == MMAY_BYTECODE_VERIFY_OK) {
        *work_used = work;
    }
    return status;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify(
    const MarimayBytecodeFunction *function
) {
    size_t work_used = 0;
    size_t available_work = function != NULL
        ? function->options.max_verify_work
        : 0;
    return mmay_bytecode_verify_with_work_limit(
        function,
        available_work,
        &work_used
    );
}
