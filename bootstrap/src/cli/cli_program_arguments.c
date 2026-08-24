#include "cli_program_arguments.h"

#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

static bool mmay_cli_program_argument_length(
    const char *value,
    size_t *result
) {
    size_t length = 0;
    if (value == NULL || result == NULL) return false;
    while (length <= MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES
        && value[length] != 0) {
        length += 1;
    }
    if (length > MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES) return false;
    *result = length;
    return true;
}

MarimayProgramArgumentsStatus mmay_cli_program_arguments_build(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayCliProgramArguments *arguments
) {
    size_t index = 0;
    if (arguments == NULL) return MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
    memset(arguments, 0, sizeof(*arguments));
    if (session == NULL || options == NULL
        || (options->program_arguments == NULL
            && options->program_argument_count != 0)) {
        return MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
    }
    if (options->program_argument_count > MMAY_PROGRAM_ARGUMENT_MAX_COUNT) {
        return MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT;
    }
    if (!mmay_size_multiply(
            options->program_argument_count,
            sizeof(*arguments->values), &arguments->allocation_size
        )) {
        return MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
    }
    arguments->allocator = session->allocator;
    if (arguments->allocation_size != 0) {
        arguments->values = mmay_allocate(
            &arguments->allocator, arguments->allocation_size
        );
        if (arguments->values == NULL) {
            return MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED;
        }
    }
    for (index = 0; index < options->program_argument_count; index += 1) {
        const char *value = options->program_arguments[index];
        size_t length = 0;
        if (value == NULL) {
            mmay_cli_program_arguments_dispose(arguments);
            return MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
        }
        if (!mmay_cli_program_argument_length(value, &length)) {
            mmay_cli_program_arguments_dispose(arguments);
            return MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
        }
        arguments->values[index].data = (const unsigned char *)value;
        arguments->values[index].length = length;
    }
    arguments->list.values = arguments->values;
    arguments->list.count = options->program_argument_count;
    {
        MarimayProgramArgumentsStatus status =
            mmay_program_arguments_validate(arguments->list);
        if (status != MMAY_PROGRAM_ARGUMENTS_OK) {
            mmay_cli_program_arguments_dispose(arguments);
        }
        return status;
    }
}

void mmay_cli_program_arguments_dispose(
    MarimayCliProgramArguments *arguments
) {
    if (arguments == NULL) return;
    mmay_deallocate(
        &arguments->allocator, arguments->values, arguments->allocation_size
    );
    memset(arguments, 0, sizeof(*arguments));
}

int mmay_cli_program_arguments_error(
    const MarimayCliOptions *options,
    MarimayProgramArgumentsStatus status,
    MarimayWriter standard_error
) {
    const char *message = "program arguments failed validation";
    const char *code = MMAY_DIAGNOSTIC_PROGRAM_ARGUMENTS;
    int exit_code = MMAY_EXIT_SOURCE_FAILURE;
    if (status == MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8) {
        message = "program argument is not valid UTF-8";
    } else if (status == MMAY_PROGRAM_ARGUMENTS_CONTAINS_NUL) {
        message = "program argument contains NUL";
    } else if (status == MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT) {
        message = "program argument count exceeds 4096";
    } else if (status == MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT) {
        message = "program arguments exceed the 1048576-byte total or 262144-byte per-argument limit";
    } else if (status == MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED) {
        code = MMAY_DIAGNOSTIC_RUNTIME_FAILURE;
        message = "runtime host allocation failed";
        exit_code = MMAY_EXIT_INTERNAL;
    }
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        code, message, exit_code
    );
}
