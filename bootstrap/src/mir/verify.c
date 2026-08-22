#include "mir_internal.h"
#include "ruda/base/utf8.h"
#include <limits.h>
#include <string.h>
RudaMirVerifyStatus ruda_mir_verify(const RudaMirFunction *function) {
    size_t block_count = 0, register_count = 0, cell_count = 0;
    size_t cell_allocation_count = 0, scratch_allocation_count = 0;
    size_t offset_count = 0, edge_count = 0;
    size_t predecessor_counts_size = 0, predecessor_offsets_size = 0;
    size_t predecessors_size = 0, block_index = 0, instruction_total = 0;
    size_t *predecessor_counts = NULL;
    size_t *predecessor_offsets = NULL;
    RudaMirBlockId *predecessors = NULL;
    unsigned char *incoming = NULL, *outgoing = NULL, *scratch = NULL;
    uint64_t *checked_incoming = NULL, *checked_outgoing = NULL;
    uint64_t *checked_scratch = NULL;
    RudaMap managed_types;
    size_t iteration = 0;
    size_t work = 0;
    bool changed = false, has_checked_index_operations = false;
    bool has_defer_operations = false;
    bool managed_types_initialized = false;
    size_t checked_cell_bytes = 0;
    size_t checked_scratch_bytes = 0;
    RudaMirVerifyStatus status = RUDA_MIR_VERIFY_OK;
    if (function == NULL || !function->initialized
        || function->result_type == RUDA_TYPE_INVALID
        || function->parameter_count > function->register_types.length
        || function->blocks.length == 0
        || function->blocks.length > function->options.max_blocks
        || function->register_types.length > function->options.max_registers
        || function->instruction_count > function->options.max_instructions
        || function->calls.length > function->options.max_calls
        || function->call_arguments.length
            > function->options.max_call_arguments
        || function->records.length > function->options.max_record_layouts
        || function->record_fields.length
            > function->options.max_record_fields
        || function->record_constructions.length
            > function->options.max_record_constructions
        || function->record_arguments.length
            > function->options.max_record_arguments
        || function->enums.length > function->options.max_enum_layouts
        || function->enum_variants.length
            > function->options.max_enum_variants
        || function->enum_payloads.length
            > function->options.max_enum_payloads
        || function->enum_constructions.length
            > function->options.max_enum_constructions
        || function->enum_arguments.length
            > function->options.max_enum_arguments
        || function->string_constants.length
            > function->options.max_string_constants
        || function->string_bytes > function->options.max_string_bytes
        || function->arrays.length > function->options.max_array_layouts
        || function->array_constructions.length
            > function->options.max_array_constructions
        || function->array_arguments.length
            > function->options.max_array_arguments
        || function->interfaces.length
            > function->options.max_interface_layouts
        || function->interface_constructions.length
            > function->options.max_interface_constructions
        || function->interface_methods.length
            > function->options.max_interface_methods
        || function->options.max_verify_work == 0) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (function->records.length != 0 || function->enums.length != 0
        || function->arrays.length != 0 || function->interfaces.length != 0
        || function->interface_constructions.length != 0
        || function->interface_methods.length != 0) {
        if (!ruda_map_init(
                &managed_types,
                function->allocator,
                sizeof(RudaMirManagedLayoutInfo),
                UINT64_C(0x725564614d495231),
                UINT64_C(0x725564614d495232)
            )) {
            return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        }
        managed_types_initialized = true;
    }
    status = ruda_mir_verify_managed_layouts(
        function,
        &managed_types,
        &work
    );
    if (status != RUDA_MIR_VERIFY_OK) {
        goto cleanup;
    }
    status = managed_types_initialized
        ? ruda_mir_verify_interface_layouts(
            function,
            &managed_types,
            &work
        )
        : RUDA_MIR_VERIFY_OK;
    if (status != RUDA_MIR_VERIFY_OK) {
        goto cleanup;
    }
    block_count = function->blocks.length;
    register_count = function->register_types.length;
    if (!ruda_size_multiply(block_count, register_count, &cell_count)
        || cell_count > RUDA_MIR_MAX_DATAFLOW_CELLS
        || !ruda_size_add(block_count, 1, &offset_count)
        || !ruda_size_multiply(
            block_count,
            sizeof(size_t),
            &predecessor_counts_size
        ) || !ruda_size_multiply(
            offset_count,
            sizeof(size_t),
            &predecessor_offsets_size
        )) {
        status = RUDA_MIR_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    cell_allocation_count = cell_count == 0 ? 1 : cell_count;
    scratch_allocation_count = register_count == 0 ? 1 : register_count;
    predecessor_counts = ruda_allocate_array(
        &function->allocator,
        block_count,
        sizeof(size_t)
    );
    incoming = ruda_allocate_array(
        &function->allocator,
        cell_allocation_count,
        sizeof(unsigned char)
    );
    outgoing = ruda_allocate_array(
        &function->allocator,
        cell_allocation_count,
        sizeof(unsigned char)
    );
    scratch = ruda_allocate_array(
        &function->allocator,
        scratch_allocation_count,
        sizeof(unsigned char)
    );
    if (predecessor_counts == NULL || incoming == NULL || outgoing == NULL
        || scratch == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(predecessor_counts, 0, predecessor_counts_size);
    memset(incoming, 0, cell_allocation_count);
    memset(outgoing, 0, cell_allocation_count);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (!ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        if (block == NULL || !block->initialized
            || block->terminator.kind == RUDA_MIR_TERMINATOR_UNSET) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
        if (!ruda_size_add(
                instruction_total,
                block->instructions.length,
                &instruction_total
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction != NULL
                && (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                    || instruction->kind == RUDA_MIR_ARRAY_ASSUME_INDEX
                    || instruction->kind == RUDA_MIR_ARRAY_SET
                    || instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS
                    || instruction->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS)) {
                has_checked_index_operations = true;
            }
            if (instruction != NULL
                && (instruction->kind == RUDA_MIR_DEFER_REGISTER
                    || instruction->kind == RUDA_MIR_DEFER_CALL)) {
                has_defer_operations = true;
            }
            size_t instruction_work = 1;
            if (instruction != NULL
                && (instruction->kind == RUDA_MIR_CALL
                    || instruction->kind == RUDA_MIR_CALL_INTERFACE
                    || instruction->kind == RUDA_MIR_DEFER_REGISTER
                    || instruction->kind == RUDA_MIR_DEFER_CALL)) {
                const RudaMirCall *call = ruda_mir_call(
                    function,
                    instruction->call
                );
                size_t argument_end = 0;
                if (call == NULL || !ruda_size_add(
                        call->argument_start,
                        call->argument_count,
                        &argument_end
                    ) || argument_end > function->call_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_size_add(
                        instruction_work,
                        call->argument_count,
                        &instruction_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (instruction != NULL
                && instruction->kind == RUDA_MIR_RECORD_NEW) {
                const RudaMirRecordConstruction *construction =
                    ruda_mir_record_construction(
                        function,
                        instruction->record_construction
                    );
                if (construction == NULL || !ruda_size_add(
                        instruction_work,
                        construction->argument_count,
                        &instruction_work
                    )) {
                    status = construction == NULL
                        ? RUDA_MIR_VERIFY_INVALID_ARGUMENT
                        : RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (instruction != NULL
                && instruction->kind == RUDA_MIR_ENUM_NEW) {
                const RudaMirEnumConstruction *construction =
                    ruda_mir_enum_construction(
                        function,
                        instruction->enum_construction
                    );
                if (construction == NULL || !ruda_size_add(
                        instruction_work,
                        construction->argument_count,
                        &instruction_work
                    )) {
                    status = construction == NULL
                        ? RUDA_MIR_VERIFY_INVALID_ARGUMENT
                        : RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (instruction != NULL
                && instruction->kind == RUDA_MIR_ARRAY_NEW) {
                const RudaMirArrayConstruction *construction =
                    ruda_mir_array_construction(
                        function,
                        instruction->array_construction
                    );
                if (construction == NULL || !ruda_size_add(
                        instruction_work,
                        construction->argument_count,
                        &instruction_work
                    )) {
                    status = construction == NULL
                        ? RUDA_MIR_VERIFY_INVALID_ARGUMENT
                        : RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (instruction != NULL
                && instruction->kind == RUDA_MIR_INTERFACE_NEW) {
                if (instruction->interface_construction
                    >= function->interface_constructions.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_size_add(
                        instruction_work,
                        1,
                        &instruction_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    instruction_work,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            status = instruction != NULL
                ? ruda_mir_validate_instruction_types(
                    function,
                    instruction,
                    managed_types_initialized ? &managed_types : NULL
                )
                : RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            if (status != RUDA_MIR_VERIFY_OK) {
                goto cleanup;
            }
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (block->terminator.target >= block_count) {
                status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            if (!ruda_size_add(
                    predecessor_counts[block->terminator.target],
                    1,
                    &predecessor_counts[block->terminator.target]
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (block->terminator.target >= block_count
                || block->terminator.otherwise >= block_count) {
                status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            if (!ruda_mir_register_is(
                    function,
                    block->terminator.value,
                    RUDA_TYPE_BOOL
                )) {
                status = block->terminator.value >= register_count
                    ? RUDA_MIR_VERIFY_INVALID_REGISTER
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
                goto cleanup;
            }
            if (!ruda_size_add(
                    predecessor_counts[block->terminator.target],
                    1,
                    &predecessor_counts[block->terminator.target]
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (block->terminator.otherwise != block->terminator.target) {
                if (!ruda_size_add(
                        predecessor_counts[block->terminator.otherwise],
                        1,
                        &predecessor_counts[block->terminator.otherwise]
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
        } else if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_mir_call(
                function,
                block->terminator.call
            );
            size_t argument_end = 0;
            size_t argument_index = 0;
            if (call == NULL || call->result_type != RUDA_TYPE_NEVER
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    call->argument_count,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            for (argument_index = call->argument_start;
                argument_index < argument_end; argument_index += 1) {
                const RudaMirCallArgument *argument =
                    ruda_mir_call_argument(function, argument_index);
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    status = RUDA_MIR_VERIFY_INVALID_REGISTER;
                    goto cleanup;
                }
                if (actual != argument->type) {
                    status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
                    goto cleanup;
                }
            }
        } else if (block->terminator.kind != RUDA_MIR_RETURN
            && block->terminator.kind != RUDA_MIR_PANIC) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
    }
    if (instruction_total != function->instruction_count) {
        status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        goto cleanup;
    }
    if (has_defer_operations) {
        status = ruda_mir_verify_defer_control_flow(function, &work);
        if (status != RUDA_MIR_VERIFY_OK) {
            goto cleanup;
        }
    }
    predecessor_offsets = ruda_allocate_array(
        &function->allocator,
        offset_count,
        sizeof(size_t)
    );
    if (predecessor_offsets == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    predecessor_offsets[0] = 0;
    for (block_index = 0; block_index < block_count; block_index += 1) {
        if (!ruda_size_add(
                predecessor_offsets[block_index],
                predecessor_counts[block_index],
                &predecessor_offsets[block_index + 1]
            ) || !ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
    }
    edge_count = predecessor_offsets[block_count];
    if (!ruda_size_multiply(
            edge_count == 0 ? 1 : edge_count,
            sizeof(RudaMirBlockId),
            &predecessors_size
        )) {
        status = RUDA_MIR_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    predecessors = ruda_allocate_array(
        &function->allocator,
        edge_count == 0 ? 1 : edge_count,
        sizeof(RudaMirBlockId)
    );
    if (predecessors == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(predecessor_counts, 0, predecessor_counts_size);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        RudaMirBlockId targets[2];
        size_t target_count = 0;
        size_t target_index = 0;
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            targets[target_count] = block->terminator.target;
            target_count += 1;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            targets[target_count] = block->terminator.target;
            target_count += 1;
            if (block->terminator.otherwise != block->terminator.target) {
                targets[target_count] = block->terminator.otherwise;
                target_count += 1;
            }
        }
        for (target_index = 0; target_index < target_count;
            target_index += 1) {
            size_t target = targets[target_index];
            size_t position = predecessor_offsets[target]
                + predecessor_counts[target];
            if (position >= edge_count) {
                status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            predecessors[position] = (RudaMirBlockId)block_index;
            predecessor_counts[target] += 1;
            if (!ruda_mir_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
    }
    if (register_count != 0) {
        size_t register_index = 0;
        for (register_index = 0;
            register_index < function->parameter_count;
            register_index += 1) {
            incoming[register_index] = 1;
        }
        for (block_index = 1; block_index < block_count; block_index += 1) {
            if (predecessor_offsets[block_index]
                != predecessor_offsets[block_index + 1]) {
                memset(
                    incoming + block_index * register_count,
                    1,
                    register_count
                );
                memset(
                    outgoing + block_index * register_count,
                    1,
                    register_count
                );
            }
        }
    }
    do {
        changed = false;
        for (block_index = 0; block_index < block_count; block_index += 1) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->blocks,
                block_index
            );
            unsigned char *block_in = incoming + block_index * register_count;
            unsigned char *block_out = outgoing + block_index * register_count;
            size_t register_index = 0;
            size_t predecessor_start = predecessor_offsets[block_index];
            size_t predecessor_end = predecessor_offsets[block_index + 1];
            size_t predecessor_count = predecessor_end - predecessor_start;
            size_t predecessor_work = 0;
            size_t transfer_work = 0;
            if (!ruda_size_multiply(
                    register_count,
                    predecessor_count,
                    &predecessor_work
                ) || !ruda_size_add(
                    block->instructions.length,
                    register_count,
                    &transfer_work
                ) || !ruda_size_add(
                    transfer_work,
                    register_count,
                    &transfer_work
                ) || !ruda_mir_consume_verify_work(
                    &work,
                    predecessor_work,
                    function->options.max_verify_work
                ) || !ruda_mir_consume_verify_work(
                    &work,
                    transfer_work,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (block_index != 0 && predecessor_count != 0) {
                for (register_index = 0;
                    register_index < register_count;
                    register_index += 1) {
                    size_t predecessor_index = predecessor_start;
                    RudaMirBlockId predecessor = predecessors[
                        predecessor_index
                    ];
                    unsigned char intersection = outgoing[
                        (size_t)predecessor * register_count + register_index
                    ];
                    for (predecessor_index += 1;
                        predecessor_index < predecessor_end;
                        predecessor_index += 1) {
                        predecessor = predecessors[predecessor_index];
                        intersection = (unsigned char)(intersection
                            && outgoing[
                                (size_t)predecessor * register_count
                                    + register_index
                            ]);
                    }
                    if (block_in[register_index] != intersection) {
                        block_in[register_index] = intersection;
                        changed = true;
                    }
                }
            }
            memcpy(scratch, block_in, register_count);
            ruda_mir_transfer_assignments(block, scratch, register_count);
            if (memcmp(block_out, scratch, register_count) != 0) {
                memcpy(block_out, scratch, register_count);
                changed = true;
            }
        }
        iteration += 1;
        if (iteration > block_count + 1) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
    } while (changed);
    if (has_checked_index_operations) {
        const RudaMirBlock *entry = ruda_vector_at_const(
            &function->blocks, 0
        );
        size_t index = 0;
        size_t entry_work = 0;
        if (!ruda_size_multiply(
                cell_allocation_count,
                sizeof(uint64_t),
                &checked_cell_bytes
            ) || !ruda_size_multiply(
                scratch_allocation_count,
                sizeof(uint64_t),
                &checked_scratch_bytes
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        checked_incoming = ruda_allocate_array(
            &function->allocator, cell_allocation_count, sizeof(uint64_t)
        );
        checked_outgoing = ruda_allocate_array(
            &function->allocator, cell_allocation_count, sizeof(uint64_t)
        );
        checked_scratch = ruda_allocate_array(
            &function->allocator, scratch_allocation_count, sizeof(uint64_t)
        );
        if (checked_incoming == NULL || checked_outgoing == NULL
            || checked_scratch == NULL) {
            status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (index = 0; index < cell_count; index += 1) {
            checked_incoming[index] = ruda_mir_checked_index_top;
            checked_outgoing[index] = ruda_mir_checked_index_top;
        }
        for (index = 0; index < register_count; index += 1) {
            checked_incoming[index] = ruda_mir_checked_index_none;
            checked_outgoing[index] = ruda_mir_checked_index_none;
        }
        if (!ruda_size_multiply(
                entry != NULL ? entry->instructions.length : 0,
                register_count,
                &entry_work
            ) || !ruda_mir_consume_verify_work(
                &work, entry_work, function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        ruda_mir_transfer_checked_indices(
            entry, checked_outgoing, register_count
        );
        do {
            changed = false;
            for (block_index = 1; block_index < block_count;
                block_index += 1) {
                const RudaMirBlock *block = ruda_vector_at_const(
                    &function->blocks, block_index
                );
                uint64_t *incoming_row = checked_incoming
                    + block_index * register_count;
                uint64_t *outgoing_row = checked_outgoing
                    + block_index * register_count;
                size_t predecessor_index = 0;
                size_t register_index = 0;
                size_t transfer_work = 0;
                for (register_index = 0; register_index < register_count;
                    register_index += 1) {
                    checked_scratch[register_index] =
                        ruda_mir_checked_index_top;
                }
                for (predecessor_index = predecessor_offsets[block_index];
                    predecessor_index < predecessor_offsets[block_index + 1];
                    predecessor_index += 1) {
                    const uint64_t *predecessor_row = checked_outgoing
                        + (size_t)predecessors[predecessor_index]
                            * register_count;
                    for (register_index = 0; register_index < register_count;
                        register_index += 1) {
                        checked_scratch[register_index] =
                            ruda_mir_meet_checked_index(
                                checked_scratch[register_index],
                                predecessor_row[register_index]
                            );
                    }
                    if (!ruda_mir_consume_verify_work(
                            &work, register_count,
                            function->options.max_verify_work
                        )) {
                        status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                        goto cleanup;
                    }
                }
                if (register_count != 0 && memcmp(
                        incoming_row, checked_scratch,
                        checked_scratch_bytes
                    ) != 0) {
                    memcpy(
                        incoming_row, checked_scratch, checked_scratch_bytes
                    );
                    changed = true;
                }
                if (!ruda_size_multiply(
                        block->instructions.length,
                        register_count,
                        &transfer_work
                    ) || !ruda_mir_consume_verify_work(
                        &work, transfer_work,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                ruda_mir_transfer_checked_indices(
                    block, checked_scratch, register_count
                );
                if (register_count != 0 && memcmp(
                        outgoing_row, checked_scratch,
                        checked_scratch_bytes
                    ) != 0) {
                    memcpy(
                        outgoing_row, checked_scratch, checked_scratch_bytes
                    );
                    changed = true;
                }
            }
        } while (changed);
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        unsigned char *assigned = incoming + block_index * register_count;
        unsigned char *working = scratch;
        size_t instruction_index = 0;
        size_t validation_work = 0;
        if (!ruda_size_add(
                register_count,
                block->instructions.length,
                &validation_work
            ) || !ruda_size_add(
                validation_work,
                1,
                &validation_work
            ) || !ruda_mir_consume_verify_work(
                &work,
                validation_work,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        memcpy(working, assigned, register_count);
        if (has_checked_index_operations && register_count != 0) {
            memcpy(
                checked_scratch,
                checked_incoming + block_index * register_count,
                checked_scratch_bytes
            );
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction->kind == RUDA_MIR_ARRAY_SET
                && (checked_scratch == NULL
                    || checked_scratch[instruction->first]
                        != instruction->second)) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (ruda_mir_instruction_uses_first(instruction->kind)
                && !ruda_mir_assigned_register(
                    working,
                    register_count,
                    instruction->first
                )) {
                status = RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER;
                goto cleanup;
            }
            if (ruda_mir_instruction_uses_second(instruction->kind)
                && !ruda_mir_assigned_register(
                    working,
                    register_count,
                    instruction->second
                )) {
                status = RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER;
                goto cleanup;
            }
            if (ruda_mir_instruction_uses_third(instruction->kind)
                && !ruda_mir_assigned_register(
                    working,
                    register_count,
                    instruction->third
                )) {
                status = instruction->third < register_count
                    ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                    : RUDA_MIR_VERIFY_INVALID_REGISTER;
                goto cleanup;
            }
            if (instruction->kind == RUDA_MIR_CALL
                || instruction->kind == RUDA_MIR_CALL_INTERFACE
                || instruction->kind == RUDA_MIR_DEFER_REGISTER
                || instruction->kind == RUDA_MIR_DEFER_CALL) {
                const RudaMirCall *call = ruda_mir_call(
                    function,
                    instruction->call
                );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (call == NULL || !ruda_size_add(
                        call->argument_start,
                        call->argument_count,
                        &argument_end
                    ) || argument_end > function->call_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        call->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = call->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirCallArgument *argument =
                        ruda_mir_call_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            } else if (instruction->kind == RUDA_MIR_INTERFACE_NEW) {
                const RudaMirInterfaceConstruction *construction =
                    instruction->interface_construction
                            < function->interface_constructions.length
                        ? ruda_vector_at_const(
                            &function->interface_constructions,
                            instruction->interface_construction
                        )
                        : NULL;
                if (!ruda_mir_consume_verify_work(
                        &work,
                        1,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                if (construction == NULL
                    || !ruda_mir_assigned_register(
                        working,
                        register_count,
                        construction->payload
                    )) {
                    status = construction != NULL
                            && construction->payload < register_count
                        ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                        : RUDA_MIR_VERIFY_INVALID_REGISTER;
                    goto cleanup;
                }
            } else if (instruction->kind == RUDA_MIR_RECORD_NEW) {
                const RudaMirRecordConstruction *construction =
                    ruda_mir_record_construction(
                        function,
                        instruction->record_construction
                    );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (construction == NULL || !ruda_size_add(
                        construction->argument_start,
                        construction->argument_count,
                        &argument_end
                    ) || argument_end > function->record_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        construction->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = construction->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirRecordArgument *argument =
                        ruda_mir_record_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            } else if (instruction->kind == RUDA_MIR_ENUM_NEW) {
                const RudaMirEnumConstruction *construction =
                    ruda_mir_enum_construction(
                        function,
                        instruction->enum_construction
                    );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (construction == NULL || !ruda_size_add(
                        construction->argument_start,
                        construction->argument_count,
                        &argument_end
                    ) || argument_end > function->enum_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        construction->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = construction->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirEnumArgument *argument =
                        ruda_mir_enum_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            } else if (instruction->kind == RUDA_MIR_ARRAY_NEW) {
                const RudaMirArrayConstruction *construction =
                    ruda_mir_array_construction(
                        function,
                        instruction->array_construction
                    );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (construction == NULL || !ruda_size_add(
                        construction->argument_start,
                        construction->argument_count,
                        &argument_end
                    ) || argument_end > function->array_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        construction->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = construction->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirArrayArgument *argument =
                        ruda_mir_array_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            }
            working[instruction->target] = 1;
            if (has_checked_index_operations) {
                if (!ruda_mir_consume_verify_work(
                        &work, register_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                ruda_mir_transfer_checked_index_instruction(
                    instruction, checked_scratch, register_count
                );
            }
        }
        if (block->terminator.kind == RUDA_MIR_RETURN) {
            if (function->result_type == RUDA_TYPE_NEVER) {
                status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
            } else if (function->result_type == RUDA_TYPE_VOID) {
                if (block->terminator.value != RUDA_MIR_NO_REGISTER) {
                    status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            } else if (!ruda_mir_assigned_register(
                    working,
                    register_count,
                    block->terminator.value
                )) {
                status = block->terminator.value < register_count
                    ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                    : RUDA_MIR_VERIFY_INVALID_REGISTER;
            } else if (ruda_mir_register_type(
                    function,
                    block->terminator.value
                ) != function->result_type) {
                status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
        } else if (block->terminator.kind == RUDA_MIR_PANIC) {
            if (!ruda_mir_assigned_register(
                    working,
                    register_count,
                    block->terminator.value
                )) {
                status = block->terminator.value < register_count
                    ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                    : RUDA_MIR_VERIFY_INVALID_REGISTER;
            } else if (ruda_mir_register_type(
                    function,
                    block->terminator.value
                ) != RUDA_TYPE_STRING) {
                status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
        } else if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_mir_call(
                function,
                block->terminator.call
            );
            size_t argument_end = 0;
            size_t argument_index = 0;
            if (call == NULL || call->result_type != RUDA_TYPE_NEVER
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            } else if (!ruda_mir_consume_verify_work(
                    &work,
                    call->argument_count,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
            for (argument_index = call != NULL ? call->argument_start : 0;
                status == RUDA_MIR_VERIFY_OK && argument_index < argument_end;
                argument_index += 1) {
                const RudaMirCallArgument *argument =
                    ruda_mir_call_argument(function, argument_index);
                if (argument == NULL || !ruda_mir_assigned_register(
                        working,
                        register_count,
                        argument->register_id
                    )) {
                    status = argument != NULL
                            && argument->register_id < register_count
                        ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                        : RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
            }
        } else if (block->terminator.kind == RUDA_MIR_BRANCH
            && !ruda_mir_assigned_register(
                working,
                register_count,
                block->terminator.value
            )) {
            status = block->terminator.value < register_count
                ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                : RUDA_MIR_VERIFY_INVALID_REGISTER;
        }
        if (status != RUDA_MIR_VERIFY_OK) {
            goto cleanup;
        }
    }
    status = ruda_mir_verify_range_proofs(function, &work);
    if (status != RUDA_MIR_VERIFY_OK) {
        goto cleanup;
    }
cleanup:
    if (managed_types_initialized) {
        ruda_map_dispose(&managed_types);
    }
    if (checked_scratch != NULL) {
        ruda_deallocate(
            &function->allocator, checked_scratch, checked_scratch_bytes
        );
    }
    if (checked_outgoing != NULL) {
        ruda_deallocate(
            &function->allocator, checked_outgoing, checked_cell_bytes
        );
    }
    if (checked_incoming != NULL) {
        ruda_deallocate(
            &function->allocator, checked_incoming, checked_cell_bytes
        );
    }
    if (scratch != NULL) {
        ruda_deallocate(
            &function->allocator,
            scratch,
            scratch_allocation_count
        );
    }
    if (predecessors != NULL) {
        ruda_deallocate(
            &function->allocator,
            predecessors,
            predecessors_size
        );
    }
    if (predecessor_offsets != NULL) {
        ruda_deallocate(
            &function->allocator,
            predecessor_offsets,
            predecessor_offsets_size
        );
    }
    if (outgoing != NULL) {
        ruda_deallocate(
            &function->allocator,
            outgoing,
            cell_allocation_count
        );
    }
    if (incoming != NULL) {
        ruda_deallocate(
            &function->allocator,
            incoming,
            cell_allocation_count
        );
    }
    if (predecessor_counts != NULL) {
        ruda_deallocate(
            &function->allocator,
            predecessor_counts,
            predecessor_counts_size
        );
    }
    return status;
}
