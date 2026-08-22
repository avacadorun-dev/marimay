#include "c_emitter_internal.h"

#include <string.h>

static bool ruda_native_c_instruction_uses_first(
    RudaMirInstructionKind kind
) {
    return kind != RUDA_MIR_CONSTANT_I64
        && kind != RUDA_MIR_CONSTANT_INTEGER
        && kind != RUDA_MIR_CONSTANT_F64
        && kind != RUDA_MIR_CONSTANT_BOOL
        && kind != RUDA_MIR_CONSTANT_STRING && kind != RUDA_MIR_RECORD_NEW
        && kind != RUDA_MIR_ENUM_NEW && kind != RUDA_MIR_ARRAY_NEW
        && kind != RUDA_MIR_INTERFACE_NEW && kind != RUDA_MIR_CALL
        && kind != RUDA_MIR_DEFER_REGISTER
        && kind != RUDA_MIR_DEFER_CALL
        && kind != RUDA_MIR_CALL_INTERFACE;
}

static bool ruda_native_c_instruction_uses_second(
    RudaMirInstructionKind kind
) {
    switch (kind) {
        case RUDA_MIR_ADD_CHECKED_I64:
        case RUDA_MIR_ADD_PROVEN_I64:
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
        case RUDA_MIR_SUBTRACT_PROVEN_I64:
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
        case RUDA_MIR_MULTIPLY_PROVEN_I64:
        case RUDA_MIR_DIVIDE_CHECKED_I64:
        case RUDA_MIR_REMAINDER_CHECKED_I64:
        case RUDA_MIR_BITWISE_AND_I64:
        case RUDA_MIR_BITWISE_XOR_I64:
        case RUDA_MIR_BITWISE_OR_I64:
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
        case RUDA_MIR_SHIFT_RIGHT_I64:
        case RUDA_MIR_ADD_F64:
        case RUDA_MIR_SUBTRACT_F64:
        case RUDA_MIR_MULTIPLY_F64:
        case RUDA_MIR_DIVIDE_F64:
        case RUDA_MIR_EQUAL_I64:
        case RUDA_MIR_EQUAL_F64:
        case RUDA_MIR_EQUAL_BOOL:
        case RUDA_MIR_EQUAL_STRING:
        case RUDA_MIR_LESS_I64:
        case RUDA_MIR_LESS_EQUAL_I64:
        case RUDA_MIR_LESS_F64:
        case RUDA_MIR_LESS_EQUAL_F64:
        case RUDA_MIR_RECORD_SET:
        case RUDA_MIR_RECORD_IDENTITY_SAME:
        case RUDA_MIR_ARRAY_GET:
        case RUDA_MIR_ARRAY_SET:
        case RUDA_MIR_ARRAY_GET_IN_BOUNDS:
        case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
        case RUDA_MIR_ARRAY_ASSUME_INDEX:
        case RUDA_MIR_ARRAY_PUSH:
        case RUDA_MIR_ARRAY_TRUNCATE:
        case RUDA_MIR_ARRAY_RESERVE:
        case RUDA_MIR_ARRAY_REQUIRE_GENERATION:
        case RUDA_MIR_INTEGER_POLICY:
            return true;
        default:
            return false;
    }
}

static void ruda_native_c_add_register(
    unsigned char *live, size_t register_count, RudaMirRegister register_id
) {
    if (register_id < register_count) live[register_id] = 1;
}

static void ruda_native_c_add_call_arguments(
    const RudaMirFunction *function, RudaMirCallId call_id,
    unsigned char *live, size_t register_count
) {
    const RudaMirCall *call = ruda_native_c_call(function, call_id);
    size_t index = 0;
    if (call == NULL) return;
    for (index = 0; index < call->argument_count; index += 1) {
        const RudaMirCallArgument *argument = ruda_native_c_call_argument(
            function, call->argument_start + index
        );
        if (argument != NULL) ruda_native_c_add_register(
            live, register_count, argument->register_id
        );
    }
}

static void ruda_native_c_add_construction_arguments(
    const RudaMirFunction *function, const RudaMirInstruction *instruction,
    unsigned char *live, size_t register_count
) {
    size_t index = 0;
    if (instruction->kind == RUDA_MIR_RECORD_NEW) {
        const RudaMirRecordConstruction *construction = ruda_vector_at_const(
            &function->record_constructions,
            instruction->record_construction
        );
        if (construction == NULL) return;
        for (index = 0; index < construction->argument_count; index += 1) {
            const RudaMirRecordArgument *argument = ruda_vector_at_const(
                &function->record_arguments,
                construction->argument_start + index
            );
            if (argument != NULL) ruda_native_c_add_register(
                live, register_count, argument->register_id
            );
        }
    } else if (instruction->kind == RUDA_MIR_ENUM_NEW) {
        const RudaMirEnumConstruction *construction = ruda_vector_at_const(
            &function->enum_constructions, instruction->enum_construction
        );
        if (construction == NULL) return;
        for (index = 0; index < construction->argument_count; index += 1) {
            const RudaMirEnumArgument *argument = ruda_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + index
            );
            if (argument != NULL) ruda_native_c_add_register(
                live, register_count, argument->register_id
            );
        }
    } else if (instruction->kind == RUDA_MIR_ARRAY_NEW) {
        const RudaMirArrayConstruction *construction =
            ruda_native_c_array_construction(
                function, instruction->array_construction
            );
        if (construction == NULL) return;
        for (index = 0; index < construction->argument_count; index += 1) {
            const RudaMirArrayArgument *argument =
                ruda_native_c_array_argument(
                    function, construction->argument_start + index
                );
            if (argument != NULL) ruda_native_c_add_register(
                live, register_count, argument->register_id
            );
        }
    } else if (instruction->kind == RUDA_MIR_INTERFACE_NEW) {
        const RudaMirInterfaceConstruction *construction =
            ruda_vector_at_const(
                &function->interface_constructions,
                instruction->interface_construction
            );
        if (construction != NULL) ruda_native_c_add_register(
            live, register_count, construction->payload
        );
    }
}

static void ruda_native_c_add_instruction_uses(
    const RudaMirFunction *function, const RudaMirInstruction *instruction,
    unsigned char *live, size_t register_count
) {
    if (ruda_native_c_instruction_uses_first(instruction->kind)) {
        ruda_native_c_add_register(live, register_count, instruction->first);
    }
    if (ruda_native_c_instruction_uses_second(instruction->kind)) {
        ruda_native_c_add_register(live, register_count, instruction->second);
    }
    if (instruction->kind == RUDA_MIR_ARRAY_SET
        || instruction->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS) {
        ruda_native_c_add_register(live, register_count, instruction->third);
    }
    if (instruction->kind == RUDA_MIR_CALL
        || instruction->kind == RUDA_MIR_CALL_INTERFACE
        || instruction->kind == RUDA_MIR_DEFER_REGISTER
        || instruction->kind == RUDA_MIR_DEFER_CALL) {
        ruda_native_c_add_call_arguments(
            function, instruction->call, live, register_count
        );
    }
    ruda_native_c_add_construction_arguments(
        function, instruction, live, register_count
    );
}

static bool ruda_native_c_instruction_is_safepoint(
    const RudaMirFunction *function, const RudaMirInstruction *instruction,
    const RudaMirFunction *const *functions, size_t count
) {
    if (instruction->kind == RUDA_MIR_CALL
        || instruction->kind == RUDA_MIR_DEFER_CALL) {
        const RudaMirCall *call = ruda_native_c_call(
            function, instruction->call
        );
        const RudaMirFunction *callee = call != NULL
            ? ruda_native_c_find(functions, count, call->callee) : NULL;
        return callee == NULL || ruda_native_c_function_requires_roots(
            callee, functions, count, 0
        );
    }
    switch (instruction->kind) {
        case RUDA_MIR_RECORD_NEW:
        case RUDA_MIR_ARRAY_NEW:
        case RUDA_MIR_ARRAY_PUSH:
        case RUDA_MIR_ARRAY_RESERVE:
        case RUDA_MIR_INTERFACE_NEW:
        case RUDA_MIR_CALL_INTERFACE:
        case RUDA_MIR_FORMAT_F32:
        case RUDA_MIR_FORMAT_F64:
        case RUDA_MIR_IO_PRINTLN:
        case RUDA_MIR_TEST_OUTPUT_BEGIN:
            return true;
        case RUDA_MIR_ENUM_NEW:
            return !ruda_native_c_enum_construction_immediate(
                function, instruction
            );
        case RUDA_MIR_INTEGER_POLICY:
            return instruction->integer_policy
                == RUDA_MIR_INTEGER_CHECKED_OPTION;
        case RUDA_MIR_INTEGER_CONVERT: {
            const RudaTypeId *target = ruda_vector_at_const(
                &function->register_types, instruction->target
            );
            return target == NULL || *target >= RUDA_TYPE_FIRST_DYNAMIC;
        }
        default:
            return false;
    }
}

bool ruda_native_c_function_requires_roots(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    size_t depth
) {
    size_t block_index = 0;
    if (function == NULL || functions == NULL || depth >= count) return true;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return true;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) return true;
            if (instruction->kind == RUDA_MIR_CALL
                || instruction->kind == RUDA_MIR_DEFER_CALL) {
                const RudaMirCall *call = ruda_native_c_call(
                    function, instruction->call
                );
                const RudaMirFunction *callee = call != NULL
                    ? ruda_native_c_find(functions, count, call->callee) : NULL;
                if (callee == NULL || ruda_native_c_function_requires_roots(
                        callee, functions, count, depth + 1
                    )) return true;
            } else if (ruda_native_c_instruction_is_safepoint(
                    function, instruction, functions, count
                )) return true;
        }
        if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_native_c_call(
                function, block->terminator.call
            );
            const RudaMirFunction *callee = call != NULL
                ? ruda_native_c_find(functions, count, call->callee) : NULL;
            if (callee == NULL || ruda_native_c_function_requires_roots(
                    callee, functions, count, depth + 1
                )) return true;
        }
    }
    return false;
}

static void ruda_native_c_add_terminator_uses(
    const RudaMirFunction *function, const RudaMirTerminator *terminator,
    unsigned char *live, size_t register_count
) {
    if (terminator->kind == RUDA_MIR_RETURN
        || terminator->kind == RUDA_MIR_BRANCH
        || terminator->kind == RUDA_MIR_PANIC) {
        ruda_native_c_add_register(live, register_count, terminator->value);
    } else if (terminator->kind == RUDA_MIR_CALL_NEVER) {
        ruda_native_c_add_call_arguments(
            function, terminator->call, live, register_count
        );
    }
}

static void ruda_native_c_add_successor_live(
    const RudaMirFunction *function, const RudaMirTerminator *terminator,
    const unsigned char *incoming, unsigned char *live,
    size_t register_count
) {
    size_t index = 0;
    RudaMirBlockId first = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId second = RUDA_MIR_NO_BLOCK;
    if (terminator->kind == RUDA_MIR_GOTO) first = terminator->target;
    if (terminator->kind == RUDA_MIR_BRANCH) {
        first = terminator->target;
        second = terminator->otherwise;
    }
    if (first < function->blocks.length) for (index = 0;
        index < register_count; index += 1) {
        live[index] |= incoming[(size_t)first * register_count + index];
    }
    if (second < function->blocks.length) for (index = 0;
        index < register_count; index += 1) {
        live[index] |= incoming[(size_t)second * register_count + index];
    }
}

static void ruda_native_c_transfer_block(
    const RudaMirFunction *function, const RudaMirBlock *block,
    const unsigned char *incoming, unsigned char *live,
    size_t register_count
) {
    size_t instruction_index = block->instructions.length;
    memset(live, 0, register_count);
    ruda_native_c_add_successor_live(
        function, &block->terminator, incoming, live, register_count
    );
    ruda_native_c_add_terminator_uses(
        function, &block->terminator, live, register_count
    );
    while (instruction_index != 0) {
        const RudaMirInstruction *instruction = ruda_vector_at_const(
            &block->instructions, --instruction_index
        );
        if (instruction == NULL) continue;
        if (instruction->target < register_count) {
            live[instruction->target] = 0;
        }
        ruda_native_c_add_instruction_uses(
            function, instruction, live, register_count
        );
    }
}

static bool ruda_native_c_has_defer(const RudaMirFunction *function) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return true;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL
                || instruction->kind == RUDA_MIR_DEFER_REGISTER
                || instruction->kind == RUDA_MIR_DEFER_CALL) return true;
        }
    }
    return false;
}

static bool ruda_native_c_type_is_immediate_enum(
    const RudaMirFunction *const *functions, size_t count, RudaTypeId type
) {
    RudaNativeCManagedLayout layout;
    const RudaMirEnum *enumeration = NULL;
    size_t variant_index = 0;
    if (!ruda_native_c_find_managed_layout(
            functions, count, type, &layout
        ) || layout.kind != RUDA_NATIVE_C_MANAGED_ENUM) return false;
    enumeration = ruda_vector_at_const(
        &layout.function->enums, layout.index
    );
    if (enumeration == NULL || enumeration->variant_count != 2) return false;
    for (variant_index = 0; variant_index < enumeration->variant_count;
        variant_index += 1) {
        const RudaMirEnumVariant *variant = ruda_vector_at_const(
            &layout.function->enum_variants,
            enumeration->variant_start + variant_index
        );
        const RudaMirEnumPayload *payload = variant != NULL
                && variant->payload_count == 1
            ? ruda_vector_at_const(
                &layout.function->enum_payloads, variant->payload_start
            ) : NULL;
        if (variant == NULL || (variant->payload_count != 0
            && (variant->payload_count != 1 || payload == NULL
                || payload->type <= RUDA_TYPE_INVALID
                || payload->type >= RUDA_TYPE_FIRST_DYNAMIC
                || payload->type == RUDA_TYPE_STRING
                || payload->type == RUDA_TYPE_VOID
                || payload->type == RUDA_TYPE_NEVER))) return false;
    }
    return true;
}

static bool ruda_native_c_register_is_managed(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    size_t index
) {
    const RudaTypeId *type = ruda_vector_at_const(
        &function->register_types, index
    );
    return type != NULL
        && (*type == RUDA_TYPE_STRING
            || (*type >= RUDA_TYPE_FIRST_DYNAMIC
                && !ruda_native_c_type_is_immediate_enum(
                    functions, count, *type
                )));
}

static bool ruda_native_c_root_alias_source(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirRegister *source
) {
    const RudaTypeId *target = ruda_vector_at_const(
        &function->register_types, instruction->target
    );
    if (target == NULL || (*target != RUDA_TYPE_STRING
        && *target < RUDA_TYPE_FIRST_DYNAMIC)) return false;
    switch (instruction->kind) {
        case RUDA_MIR_COPY:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
        case RUDA_MIR_ARRAY_ASSUME_INDEX:
        case RUDA_MIR_ARRAY_PUSH:
        case RUDA_MIR_ARRAY_TRUNCATE:
        case RUDA_MIR_ARRAY_CLEAR:
        case RUDA_MIR_ARRAY_RESERVE:
        case RUDA_MIR_ARRAY_REQUIRE_GENERATION:
            *source = instruction->first;
            return true;
        case RUDA_MIR_RECORD_SET:
            *source = instruction->second;
            return true;
        case RUDA_MIR_ARRAY_SET:
        case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
            *source = instruction->third;
            return true;
        default:
            return false;
    }
}

static bool ruda_native_c_root_child_source(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirRegister *source,
    bool *immutable
) {
    const RudaTypeId *target = ruda_vector_at_const(
        &function->register_types, instruction->target
    );
    if (target == NULL || (*target != RUDA_TYPE_STRING
        && *target < RUDA_TYPE_FIRST_DYNAMIC)) return false;
    if (instruction->kind == RUDA_MIR_ENUM_GET_PAYLOAD) {
        *source = instruction->first;
        *immutable = true;
        return true;
    }
    if (instruction->kind == RUDA_MIR_RECORD_GET
        || instruction->kind == RUDA_MIR_ARRAY_GET
        || instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS) {
        *source = instruction->first;
        *immutable = false;
        return true;
    }
    return false;
}

static bool ruda_native_c_instruction_mutates_owner(
    const RudaMirInstruction *instruction
) {
    switch (instruction->kind) {
        case RUDA_MIR_RECORD_SET:
        case RUDA_MIR_ARRAY_SET:
        case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
        case RUDA_MIR_ARRAY_PUSH:
        case RUDA_MIR_ARRAY_POP:
        case RUDA_MIR_ARRAY_TRUNCATE:
        case RUDA_MIR_ARRAY_CLEAR:
        case RUDA_MIR_ARRAY_RESERVE:
            return true;
        default:
            return false;
    }
}

static RudaMirRegister ruda_native_c_root_origin(
    RudaMirRegister *origins, RudaMirRegister register_id
) {
    RudaMirRegister root = register_id;
    while (origins[root] != root) root = origins[root];
    while (origins[register_id] != register_id) {
        RudaMirRegister next = origins[register_id];
        origins[register_id] = root;
        register_id = next;
    }
    return root;
}

static void ruda_native_c_merge_root_origin(
    RudaMirRegister *origins,
    RudaMirRegister target,
    RudaMirRegister source
) {
    RudaMirRegister target_root = ruda_native_c_root_origin(origins, target);
    RudaMirRegister source_root = ruda_native_c_root_origin(origins, source);
    if (target_root != source_root) origins[target_root] = source_root;
}

static bool ruda_native_c_coalesce_root_aliases(
    const RudaMirFunction *function,
    RudaAllocator allocator,
    unsigned char *roots,
    unsigned char *mutated,
    unsigned char *coalesced,
    size_t register_count
) {
    RudaMirRegister *origins = NULL;
    size_t origin_bytes = 0;
    size_t index = 0;
    if (!ruda_size_multiply(
            register_count, sizeof(*origins), &origin_bytes
        )) return false;
    origins = ruda_allocate(&allocator, origin_bytes);
    if (origins == NULL) return false;
    for (index = 0; index < register_count; index += 1) {
        origins[index] = (RudaMirRegister)index;
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, index
        );
        size_t instruction_index = 0;
        if (block == NULL) goto fail;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister source = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) goto fail;
            if (instruction->target < register_count
                && ruda_native_c_root_alias_source(
                    function, instruction, &source
                ) && source < register_count) {
                ruda_native_c_merge_root_origin(
                    origins, instruction->target, source
                );
            }
        }
    }
    memset(mutated, 0, register_count);
    for (index = 0; index < function->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, index
        );
        size_t instruction_index = 0;
        if (block == NULL) goto fail;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) goto fail;
            if (ruda_native_c_instruction_mutates_owner(instruction)
                && instruction->first < register_count) {
                mutated[ruda_native_c_root_origin(
                    origins, instruction->first
                )] = 1;
            }
        }
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister source = RUDA_MIR_NO_REGISTER;
            bool immutable = false;
            if (ruda_native_c_root_child_source(
                    function, instruction, &source, &immutable
                ) && instruction->target < register_count
                && source < register_count) {
                RudaMirRegister source_root = ruda_native_c_root_origin(
                    origins, source
                );
                RudaMirRegister target_root = ruda_native_c_root_origin(
                    origins, instruction->target
                );
                if (immutable
                    || (!mutated[source_root] && !mutated[target_root])) {
                    ruda_native_c_merge_root_origin(
                        origins, instruction->target, source
                    );
                }
            }
        }
    }
    memset(coalesced, 0, register_count);
    for (index = 0; index < register_count; index += 1) {
        if (roots[index]) coalesced[
            ruda_native_c_root_origin(origins, (RudaMirRegister)index)
        ] = 1;
    }
    memcpy(roots, coalesced, register_count);
    ruda_deallocate(&allocator, origins, origin_bytes);
    return true;
fail:
    ruda_deallocate(&allocator, origins, origin_bytes);
    return false;
}

bool ruda_native_c_plan_root_registers(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    RudaAllocator allocator,
    unsigned char **registers,
    size_t *register_bytes,
    size_t *root_count
) {
    unsigned char *incoming = NULL;
    unsigned char *live = NULL;
    size_t register_count = function != NULL
        ? function->register_types.length : 0;
    size_t incoming_bytes = 0;
    size_t scratch_bytes = 0;
    size_t block_index = 0;
    bool changed = false;
    if (registers == NULL || register_bytes == NULL || root_count == NULL
        || function == NULL || functions == NULL) return false;
    *registers = NULL;
    *register_bytes = register_count;
    *root_count = 0;
    if (register_count == 0) return true;
    *registers = ruda_allocate(&allocator, register_count);
    if (*registers == NULL) return false;
    memset(*registers, 0, register_count);
    if (!ruda_native_c_function_requires_roots(
            function, functions, count, 0
        ) || ruda_native_c_has_defer(function)) {
        for (block_index = 0; block_index < register_count; block_index += 1) {
            if (ruda_native_c_register_is_managed(
                    function, functions, count, block_index
                )) {
                (*registers)[block_index] = 1;
                *root_count += 1;
            }
        }
        return true;
    }
    if (!ruda_size_multiply(
            function->blocks.length, register_count, &incoming_bytes
        ) || !ruda_size_add(incoming_bytes, register_count, &scratch_bytes)) {
        goto fail;
    }
    incoming = ruda_allocate(&allocator, scratch_bytes);
    if (incoming == NULL) goto fail;
    memset(incoming, 0, scratch_bytes);
    live = incoming + incoming_bytes;
    do {
        changed = false;
        for (block_index = function->blocks.length; block_index != 0;) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->blocks, --block_index
            );
            unsigned char *block_in = incoming
                + block_index * register_count;
            if (block == NULL) goto fail;
            ruda_native_c_transfer_block(
                function, block, incoming, live, register_count
            );
            if (memcmp(block_in, live, register_count) != 0) {
                memcpy(block_in, live, register_count);
                changed = true;
            }
        }
    } while (changed);
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = block->instructions.length;
        memset(live, 0, register_count);
        ruda_native_c_add_successor_live(
            function, &block->terminator, incoming, live, register_count
        );
        ruda_native_c_add_terminator_uses(
            function, &block->terminator, live, register_count
        );
        if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_native_c_call(
                function, block->terminator.call
            );
            const RudaMirFunction *callee = call != NULL
                ? ruda_native_c_find(functions, count, call->callee) : NULL;
            if (callee == NULL || ruda_native_c_function_requires_roots(
                    callee, functions, count, 0
                )) for (size_t index = 0; index < register_count; index += 1) {
                if (live[index]
                    && ruda_native_c_register_is_managed(
                        function, functions, count, index
                    )) {
                    (*registers)[index] = 1;
                }
            }
        }
        while (instruction_index != 0) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, --instruction_index
            );
            size_t index = 0;
            if (instruction == NULL) goto fail;
            if (instruction->target < register_count) {
                live[instruction->target] = 0;
            }
            ruda_native_c_add_instruction_uses(
                function, instruction, live, register_count
            );
            if (ruda_native_c_instruction_is_safepoint(
                    function, instruction, functions, count
                )) for (index = 0; index < register_count; index += 1) {
                if (live[index]
                    && ruda_native_c_register_is_managed(
                        function, functions, count, index
                    )) {
                    (*registers)[index] = 1;
                }
            }
        }
    }
    if (!ruda_native_c_coalesce_root_aliases(
            function, allocator, *registers, incoming, live, register_count
        )) goto fail;
    for (block_index = 0; block_index < register_count; block_index += 1) {
        if ((*registers)[block_index]) *root_count += 1;
    }
    ruda_deallocate(&allocator, incoming, scratch_bytes);
    return true;
fail:
    ruda_deallocate(&allocator, incoming, scratch_bytes);
    ruda_deallocate(&allocator, *registers, register_count);
    *registers = NULL;
    *register_bytes = 0;
    *root_count = 0;
    return false;
}
