#include "vm_internal.h"

#include <stdint.h>
#include <string.h>

MarimayBytecodeRunStatus mmay_bytecode_run_deferred_program_call(
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeFunction *caller,
    const MarimayBytecodeCall *call,
    const MarimayBytecodeInstruction *instruction,
    const MarimayBytecodeValue *registers,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    bool preserve_primary
) {
    MarimayBytecodeProgram nested;
    const MarimayBytecodeProgramFunction *callee = NULL;
    MarimayBytecodeValue *arguments = NULL;
    MarimayBytecodeValue result;
    MarimayBytecodePanic cleanup_panic;
    MarimayBytecodePanic *primary_panic = options.panic;
    MarimaySymbolId callee_symbol;
    MarimayManagedReference interface_payload = mmay_managed_invalid_reference();
    size_t callee_index = 0;
    size_t argument_index = 0;
    size_t argument_bytes = 0;
    MarimayBytecodeRunStatus status;
    bool cleanup_panic_ready = false;
    memset(&cleanup_panic, 0, sizeof(cleanup_panic));
    if (program == NULL || caller == NULL || call == NULL
        || instruction == NULL || registers == NULL
        || call->result_type != MMAY_BYTECODE_TYPE_VOID
        || options.max_call_depth <= 1
        || !mmay_size_multiply(
            call->argument_count, sizeof(*arguments), &argument_bytes
        )) {
        return options.max_call_depth <= 1
            ? MMAY_BYTECODE_RUN_STEP_LIMIT
            : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    callee_symbol = call->callee;
    if (call->interface_slot != UINT32_MAX) {
        status = mmay_bytecode_run_interface_resolve(
            instruction, registers, options.execution, caller,
            &callee_symbol, &interface_payload
        );
        if (status != MMAY_BYTECODE_RUN_OK) {
            return status;
        }
    }
    callee = mmay_bytecode_program_function_by_symbol(program, callee_symbol);
    if (callee == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    while (callee_index < program->functions.length
        && mmay_bytecode_program_function(program, callee_index) != callee) {
        callee_index += 1;
    }
    if (callee_index == program->functions.length) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (argument_bytes != 0) {
        arguments = mmay_allocate(&allocator, argument_bytes);
        if (arguments == NULL) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_bytecode_call_argument(
            caller, call->argument_start + argument_index
        );
        if (argument == NULL) {
            status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            goto cleanup;
        }
        if (call->interface_slot != UINT32_MAX && argument_index == 0) {
            const MarimayBytecodeType *parameter = mmay_vector_at_const(
                &callee->body.register_types, 0
            );
            if (parameter == NULL || !mmay_bytecode_type_is_managed(*parameter)) {
                status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                goto cleanup;
            }
            arguments[0].type = *parameter;
            arguments[0].as.managed_reference = interface_payload;
        } else {
            arguments[argument_index] = registers[argument->register_id];
        }
    }
    nested = *program;
    nested.entry_function = (uint32_t)callee_index;
    options.max_call_depth -= 1;
    options.trap = NULL;
    if (preserve_primary && primary_panic != NULL) {
        if (!mmay_bytecode_panic_init(&cleanup_panic, allocator)) {
            status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
            goto cleanup;
        }
        cleanup_panic_ready = true;
        options.panic = &cleanup_panic;
    }
    memset(&result, 0, sizeof(result));
    status = mmay_bytecode_run_program(
        &nested, arguments, call->argument_count, allocator, options, &result
    );
    if (status == MMAY_BYTECODE_RUN_PANIC && cleanup_panic_ready
        && !mmay_bytecode_run_append_suppressed_panic(
            primary_panic, &cleanup_panic
        )) {
        status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }

cleanup:
    if (cleanup_panic_ready) {
        mmay_bytecode_panic_dispose(&cleanup_panic);
    }
    mmay_deallocate(&allocator, arguments, argument_bytes);
    return status;
}
