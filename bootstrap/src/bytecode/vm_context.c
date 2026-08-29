#include "vm_internal.h"

#include <string.h>

#include "mmay/runtime/string.h"

typedef struct MarimayBytecodeContextDescriptors {
    const MarimayManagedEnumDescriptor *result;
    const MarimayManagedEnumDescriptor *error;
    const MarimayManagedEnumDescriptor *option;
} MarimayBytecodeContextDescriptors;

static const MarimayBytecodeEnumPayload *mmay_bytecode_context_vm_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant_index
) {
    const MarimayBytecodeEnumVariant *variant = NULL;
    size_t relative = 0;
    if (layout == NULL || variant_index >= layout->variant_count
        || layout->variant_start > function->enum_variants.length
        || variant_index >= function->enum_variants.length
            - layout->variant_start) return NULL;
    variant = mmay_vector_at_const(
        &function->enum_variants, layout->variant_start + variant_index
    );
    if (variant == NULL || variant->payload_count != 1
        || !mmay_size_add(variant->payload_start, 0, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length
            - layout->payload_start) return NULL;
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_descriptors(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    uint32_t result_id,
    bool environment,
    MarimayBytecodeContextDescriptors *descriptors
) {
    const MarimayBytecodeEnumLayout *result = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumPayload *ok = mmay_bytecode_context_vm_payload(
        function, result, 0
    );
    const MarimayBytecodeEnumPayload *error = mmay_bytecode_context_vm_payload(
        function, result, 1
    );
    const void *raw = NULL;
    MarimayBytecodeRunStatus status;
    if (result == NULL || ok == NULL || error == NULL
        || error->type != MMAY_BYTECODE_TYPE_ENUM
        || (environment && ok->type != MMAY_BYTECODE_TYPE_ENUM)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    memset(descriptors, 0, sizeof(*descriptors));
    status = mmay_bytecode_run_nominal_descriptor(
        execution, function, MMAY_BYTECODE_TYPE_ENUM, result_id, &raw
    );
    descriptors->result = raw;
    if (status == MMAY_BYTECODE_RUN_OK) status =
        mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            error->layout, &raw
        );
    descriptors->error = raw;
    if (status == MMAY_BYTECODE_RUN_OK && environment) status =
        mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            ok->layout, &raw
        );
    descriptors->option = environment ? raw : NULL;
    return status;
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_pop(
    MarimayBytecodeExecution *execution,
    MarimayManagedRootFrame *roots,
    MarimayManagedStatus status
) {
    MarimayManagedStatus pop = mmay_managed_root_pop(
        &execution->managed_heap, roots
    );
    if (status == MMAY_MANAGED_OK) status = pop;
    return mmay_bytecode_run_managed_status(status);
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_error(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeContextDescriptors *descriptors,
    size_t variant,
    const unsigned char *text,
    size_t text_length,
    int64_t detail,
    MarimayBytecodeValue *target
) {
    MarimayManagedReference text_value = mmay_managed_invalid_reference();
    MarimayManagedReference error_value = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[2] = {&text_value, &error_value};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell payload = {0};
    MarimayManagedStatus status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 2
    );
    if (status == MMAY_MANAGED_OK && text != NULL) {
        status = mmay_managed_string_new(
            &execution->managed_heap, text, text_length, &text_value
        );
        payload.reference = text_value;
    } else if (text == NULL) {
        payload.f64_bits = (uint64_t)detail;
    }
    if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(
        &execution->managed_heap, descriptors->error, variant,
        text != NULL || detail >= 0 ? &payload : NULL,
        text != NULL || detail >= 0 ? 1U : 0U, &error_value
    );
    if (status == MMAY_MANAGED_OK) {
        payload.reference = error_value;
        status = mmay_managed_enum_new(
            &execution->managed_heap, descriptors->result, 1,
            &payload, 1, &target->as.managed_reference
        );
    }
    return mmay_bytecode_context_vm_pop(execution, &roots, status);
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_env_success(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeContextDescriptors *descriptors,
    const MarimayOwnedBytes *value,
    bool present,
    MarimayBytecodeValue *target
) {
    MarimayManagedReference string = mmay_managed_invalid_reference();
    MarimayManagedReference option = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[2] = {&string, &option};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell payload = {0};
    MarimayManagedStatus status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 2
    );
    if (status == MMAY_MANAGED_OK && present) status = mmay_managed_string_new(
        &execution->managed_heap, value->data, value->length, &string
    );
    if (present) payload.reference = string;
    if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(
        &execution->managed_heap, descriptors->option, present ? 0U : 1U,
        present ? &payload : NULL, present ? 1U : 0U, &option
    );
    if (status == MMAY_MANAGED_OK) {
        payload.reference = option;
        status = mmay_managed_enum_new(
            &execution->managed_heap, descriptors->result, 0,
            &payload, 1, &target->as.managed_reference
        );
    }
    return mmay_bytecode_context_vm_pop(execution, &roots, status);
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_process_success(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeContextDescriptors *descriptors,
    const MarimayOwnedBytes *path,
    MarimayBytecodeValue *target
) {
    MarimayManagedReference string = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[1] = {&string};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell payload = {0};
    MarimayManagedStatus status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 1
    );
    if (status == MMAY_MANAGED_OK) status = mmay_managed_string_new(
        &execution->managed_heap, path->data, path->length, &string
    );
    if (status == MMAY_MANAGED_OK) {
        payload.reference = string;
        status = mmay_managed_enum_new(
            &execution->managed_heap, descriptors->result, 0,
            &payload, 1, &target->as.managed_reference
        );
    }
    return mmay_bytecode_context_vm_pop(execution, &roots, status);
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_env(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeContextDescriptors *descriptors
) {
    size_t source = (size_t)instruction->operands[1].unsigned_value;
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    const unsigned char *name = NULL;
    size_t name_length = 0;
    size_t invalid_offset = 0;
    MarimayOwnedBytes value = {0};
    MarimayEnvStatus status;
    MarimayBytecodeRunStatus result;
    if (!mmay_bytecode_execution_string_bytes(
            execution, registers[source], &name, &name_length
        )) return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    status = mmay_env_read(
        &execution->env, (const char *)name, name_length,
        &value, &invalid_offset
    );
    if (status == MMAY_ENV_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (status == MMAY_ENV_OK || status == MMAY_ENV_MISSING) {
        result = mmay_bytecode_context_vm_env_success(
            execution, descriptors, &value, status == MMAY_ENV_OK,
            &registers[target]
        );
        mmay_owned_bytes_dispose(&value);
        return result;
    }
    if (status == MMAY_ENV_INVALID_ARGUMENT) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (status == MMAY_ENV_CAPABILITY_DENIED) {
        return mmay_bytecode_context_vm_error(
            execution, descriptors, 1, name, name_length, -1,
            &registers[target]
        );
    }
    if (status == MMAY_ENV_INVALID_UTF8) return mmay_bytecode_context_vm_error(
        execution, descriptors, 2, NULL, 0, (int64_t)invalid_offset,
        &registers[target]
    );
    if (status == MMAY_ENV_LIMIT_EXCEEDED) return mmay_bytecode_context_vm_error(
        execution, descriptors, 3, NULL, 0, MMAY_ENV_MAX_VALUE_BYTES,
        &registers[target]
    );
    return mmay_bytecode_context_vm_error(
        execution, descriptors,
        status == MMAY_ENV_INVALID_NAME ? 0U : 4U,
        NULL, 0, -1, &registers[target]
    );
}

static MarimayBytecodeRunStatus mmay_bytecode_context_vm_process(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeContextDescriptors *descriptors
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    MarimayOwnedBytes path = {0};
    size_t invalid_offset = 0;
    MarimayProcessContextStatus status = instruction->opcode
            == MMAY_OPCODE_PROCESS_CURRENT_DIRECTORY
        ? mmay_process_context_current_directory(
            &execution->process_context, execution->allocator,
            &path, &invalid_offset
        ) : mmay_process_context_executable_path(
            &execution->process_context, execution->allocator,
            &path, &invalid_offset
        );
    MarimayBytecodeRunStatus result;
    size_t variant = 3;
    if (status == MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (status == MMAY_PROCESS_CONTEXT_OK) {
        result = mmay_bytecode_context_vm_process_success(
            execution, descriptors, &path, &registers[target]
        );
        mmay_owned_bytes_dispose(&path);
        return result;
    }
    if (status == MMAY_PROCESS_CONTEXT_INVALID_ARGUMENT) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (status == MMAY_PROCESS_CONTEXT_INVALID_UTF8) {
        return mmay_bytecode_context_vm_error(
            execution, descriptors, 2, NULL, 0, (int64_t)invalid_offset,
            &registers[target]
        );
    }
    if (status == MMAY_PROCESS_CONTEXT_CURRENT_DIRECTORY_UNAVAILABLE) {
        variant = 0;
    } else if (status == MMAY_PROCESS_CONTEXT_EXECUTABLE_PATH_UNAVAILABLE) {
        variant = 1;
    }
    return mmay_bytecode_context_vm_error(
        execution, descriptors, variant, NULL, 0, -1, &registers[target]
    );
}

MarimayBytecodeRunStatus mmay_bytecode_run_context_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    uint32_t result_id = mmay_bytecode_register_layout(
        function, (uint32_t)target
    );
    bool environment = instruction->opcode == MMAY_OPCODE_ENV_GET;
    MarimayBytecodeContextDescriptors descriptors;
    MarimayBytecodeRunStatus status = mmay_bytecode_context_vm_descriptors(
        execution, function, result_id, environment, &descriptors
    );
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    return environment
        ? mmay_bytecode_context_vm_env(
            instruction, registers, execution, &descriptors
        ) : mmay_bytecode_context_vm_process(
            instruction, registers, execution, &descriptors
        );
}
