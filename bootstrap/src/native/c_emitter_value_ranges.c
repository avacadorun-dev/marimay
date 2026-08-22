#include "c_emitter_internal.h"

static bool ruda_native_c_value_source(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    size_t depth,
    RudaMirRegister *origin,
    const RudaMirInstruction **definition
) {
    const RudaMirInstruction *found = NULL;
    size_t block_index = 0;
    if (function == NULL || origin == NULL || definition == NULL
        || depth > function->register_types.length) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) return false;
            if (instruction->target != register_id) continue;
            if (found != NULL) return false;
            found = instruction;
        }
    }
    if (found != NULL && found->kind == RUDA_MIR_COPY) {
        return ruda_native_c_value_source(
            function, found->first, depth + 1, origin, definition
        );
    }
    *origin = register_id;
    *definition = found;
    return true;
}

static bool ruda_native_c_local_array_is_empty(
    const RudaMirFunction *function,
    RudaMirRegister array_origin
) {
    size_t block_index = 0;
    bool found = false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            const RudaMirArrayConstruction *construction = NULL;
            if (instruction == NULL) return false;
            if (instruction->target != array_origin
                || instruction->kind != RUDA_MIR_ARRAY_NEW) continue;
            if (found) return false;
            construction = ruda_vector_at_const(
                &function->array_constructions,
                instruction->array_construction
            );
            if (construction == NULL
                || construction->argument_count != 0) return false;
            found = true;
        }
    }
    return found;
}

static bool ruda_native_c_value_pushes_are_indices(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t function_count,
    RudaMirRegister source_origin,
    RudaMirRegister target_array
) {
    size_t block_index = 0;
    bool found_push = false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block != NULL
            && block->terminator.kind == RUDA_MIR_CALL_NEVER) return false;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister instruction_origin = RUDA_MIR_NO_REGISTER;
            bool same_source = false;
            if (instruction == NULL) return false;
            if (instruction->kind == RUDA_MIR_CALL
                || instruction->kind == RUDA_MIR_CALL_INTERFACE) return false;
            if (instruction->kind != RUDA_MIR_ARRAY_PUSH
                && instruction->kind != RUDA_MIR_ARRAY_SET
                && instruction->kind != RUDA_MIR_ARRAY_SET_IN_BOUNDS
                && instruction->kind != RUDA_MIR_ARRAY_POP
                && instruction->kind != RUDA_MIR_ARRAY_TRUNCATE
                && instruction->kind != RUDA_MIR_ARRAY_CLEAR) continue;
            same_source = ruda_native_c_array_register_origin(
                function, instruction->first, &instruction_origin
            ) && instruction_origin == source_origin;
            if (!same_source) continue;
            if (instruction->kind != RUDA_MIR_ARRAY_PUSH) return false;
            if (!ruda_native_c_array_filled_index_proven(
                    function, instruction, target_array, instruction->second
                ) && !ruda_native_c_array_guarded_index_proven(
                    function, instruction, target_array, instruction->second
                ) && !ruda_native_c_array_relational_index_proven(
                    function, functions, function_count, instruction,
                    target_array, instruction->second
                )) return false;
            found_push = true;
        }
    }
    return found_push;
}

bool ruda_native_c_array_loaded_index_proven(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t function_count,
    const RudaMirInstruction *instruction,
    RudaMirRegister array_register,
    RudaMirRegister index_register
) {
    const RudaMirInstruction *definition = NULL;
    const RudaMirArray *source_array = NULL;
    RudaMirRegister index_origin = RUDA_MIR_NO_REGISTER;
    RudaMirRegister source_origin = RUDA_MIR_NO_REGISTER;
    if (function == NULL || functions == NULL || instruction == NULL
        || !ruda_native_c_value_source(
            function, index_register, 0, &index_origin, &definition
        ) || definition == NULL
        || (definition->kind != RUDA_MIR_ARRAY_GET
            && definition->kind != RUDA_MIR_ARRAY_GET_IN_BOUNDS)
        || !ruda_native_c_array_register_origin(
            function, definition->first, &source_origin
        ) || !ruda_native_c_local_array_is_empty(
            function, source_origin
        )) return false;
    source_array = ruda_vector_at_const(
        &function->arrays, definition->array
    );
    return source_array != NULL && source_array->element_type == RUDA_TYPE_I64
        && ruda_native_c_value_pushes_are_indices(
            function, functions, function_count, source_origin,
            array_register
        );
}
