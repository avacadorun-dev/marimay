#include "mir_internal.h"
#include "context_internal.h"

#include <string.h>

static const MarimayMirEnum *mmay_mir_context_enum(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirEnumPayload *mmay_mir_context_payload(
    const MarimayMirFunction *function,
    const MarimayMirEnum *layout,
    size_t variant
) {
    const MarimayMirEnumVariant *item = mmay_mir_enum_variant(
        function, layout, variant
    );
    return item != NULL && item->payload_count == 1
        ? mmay_mir_enum_payload(function, item, 0) : NULL;
}

static MarimayMirInterpretStatus mmay_mir_context_error(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    size_t variant,
    const MarimayMirValue *payload,
    MarimayMirValue *target
) {
    const MarimayMirEnumPayload *error_payload = mmay_mir_context_payload(
        function, result, 1
    );
    const MarimayMirEnum *error = error_payload != NULL
        ? mmay_mir_context_enum(function, error_payload->type) : NULL;
    MarimayMirValue error_value;
    MarimayMirInterpretStatus status;
    if (error == NULL) return MMAY_MIR_INTERPRET_INVALID_MIR;
    memset(&error_value, 0, sizeof(error_value));
    error_value.type = error->type;
    status = mmay_mir_execution_new_enum(
        options.execution, function, error, variant,
        payload, payload != NULL ? 1U : 0U, &error_value.as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    return mmay_mir_execution_new_enum(
        options.execution, function, result, 1,
        &error_value, 1, &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_context_ok(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    const MarimayMirValue *value,
    MarimayMirValue *target
) {
    return mmay_mir_execution_new_enum(
        options.execution, function, result, 0,
        value, 1, &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_context_env(
    const MarimayMirFunction *function,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    const MarimayMirEnum *result = mmay_mir_context_enum(
        function, target->type
    );
    const MarimayMirEnumPayload *ok_payload = result != NULL
        ? mmay_mir_context_payload(function, result, 0) : NULL;
    const MarimayMirEnum *option = ok_payload != NULL
        ? mmay_mir_context_enum(function, ok_payload->type) : NULL;
    const unsigned char *name = NULL;
    size_t name_length = 0;
    size_t invalid_offset = 0;
    MarimayOwnedBytes value;
    MarimayEnvStatus env_status;
    MarimayMirValue payload;
    MarimayMirValue option_value;
    MarimayMirInterpretStatus status;
    memset(&value, 0, sizeof(value));
    memset(&payload, 0, sizeof(payload));
    memset(&option_value, 0, sizeof(option_value));
    if (result == NULL || option == NULL || source == NULL
        || !mmay_mir_execution_string_bytes(
            options.execution, *source, &name, &name_length
        )) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    env_status = mmay_env_read(
        &options.execution->env, (const char *)name, name_length,
        &value, &invalid_offset
    );
    if (env_status == MMAY_ENV_ALLOCATION_FAILED) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (env_status == MMAY_ENV_OK || env_status == MMAY_ENV_MISSING) {
        option_value.type = option->type;
        if (env_status == MMAY_ENV_OK) {
            payload.type = MMAY_TYPE_STRING;
            status = mmay_mir_execution_new_string(
                options.execution, value.data, value.length,
                &payload.as.reference
            );
            mmay_owned_bytes_dispose(&value);
            if (status != MMAY_MIR_INTERPRET_OK) return status;
            status = mmay_mir_execution_new_enum(
                options.execution, function, option, 0,
                &payload, 1, &option_value.as.reference
            );
        } else {
            status = mmay_mir_execution_new_enum(
                options.execution, function, option, 1,
                NULL, 0, &option_value.as.reference
            );
        }
        return status == MMAY_MIR_INTERPRET_OK
            ? mmay_mir_context_ok(
                function, options, result, &option_value, target
            ) : status;
    }
    if (env_status == MMAY_ENV_INVALID_ARGUMENT) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (env_status == MMAY_ENV_CAPABILITY_DENIED) {
        return mmay_mir_context_error(
            function, options, result, 1, source, target
        );
    }
    if (env_status == MMAY_ENV_INVALID_UTF8
        || env_status == MMAY_ENV_LIMIT_EXCEEDED) {
        payload.type = MMAY_TYPE_I64;
        payload.as.i64 = env_status == MMAY_ENV_INVALID_UTF8
            ? (int64_t)invalid_offset : (int64_t)MMAY_ENV_MAX_VALUE_BYTES;
        return mmay_mir_context_error(
            function, options, result,
            env_status == MMAY_ENV_INVALID_UTF8 ? 2U : 3U,
            &payload, target
        );
    }
    return mmay_mir_context_error(
        function, options, result,
        env_status == MMAY_ENV_INVALID_NAME ? 0U : 4U,
        NULL, target
    );
}

static MarimayMirInterpretStatus mmay_mir_context_process(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    const MarimayMirEnum *result = mmay_mir_context_enum(
        function, target->type
    );
    MarimayOwnedBytes path;
    size_t invalid_offset = 0;
    MarimayProcessContextStatus context_status;
    MarimayMirValue payload;
    MarimayMirInterpretStatus status;
    memset(&path, 0, sizeof(path));
    memset(&payload, 0, sizeof(payload));
    if (result == NULL) return MMAY_MIR_INTERPRET_INVALID_MIR;
    context_status = instruction->context_operation
            == MMAY_MIR_CONTEXT_CURRENT_DIRECTORY
        ? mmay_process_context_current_directory(
            &options.execution->process_context,
            options.execution->allocator, &path, &invalid_offset
        ) : mmay_process_context_executable_path(
            &options.execution->process_context,
            options.execution->allocator, &path, &invalid_offset
        );
    if (context_status == MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (context_status == MMAY_PROCESS_CONTEXT_OK) {
        payload.type = MMAY_TYPE_STRING;
        status = mmay_mir_execution_new_string(
            options.execution, path.data, path.length,
            &payload.as.reference
        );
        mmay_owned_bytes_dispose(&path);
        return status == MMAY_MIR_INTERPRET_OK
            ? mmay_mir_context_ok(
                function, options, result, &payload, target
            ) : status;
    }
    if (context_status == MMAY_PROCESS_CONTEXT_INVALID_ARGUMENT) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (context_status == MMAY_PROCESS_CONTEXT_INVALID_UTF8) {
        payload.type = MMAY_TYPE_I64;
        payload.as.i64 = (int64_t)invalid_offset;
        return mmay_mir_context_error(
            function, options, result, 2, &payload, target
        );
    }
    return mmay_mir_context_error(
        function, options, result,
        context_status == MMAY_PROCESS_CONTEXT_CURRENT_DIRECTORY_UNAVAILABLE
            ? 0U : context_status
                == MMAY_PROCESS_CONTEXT_EXECUTABLE_PATH_UNAVAILABLE
                ? 1U : 3U,
        NULL, target
    );
}

MarimayMirInterpretStatus mmay_mir_execute_context_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    if (function == NULL || instruction == NULL || target == NULL
        || options.execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    return instruction->kind == MMAY_MIR_ENV_READ
        ? mmay_mir_context_env(function, source, target, options)
        : mmay_mir_context_process(function, instruction, target, options);
}
