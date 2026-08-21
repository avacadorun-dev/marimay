#include "cli_internal.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "ruda/generated/diagnostic_codes.h"

enum { RUDA_CLI_DEFAULT_MAX_HEAP_BYTES = 64U * 1024U * 1024U };

void ruda_cli_heap_limit_default(RudaCliOptions *options) {
    if (options == NULL) return;
    options->max_heap_bytes = RUDA_CLI_DEFAULT_MAX_HEAP_BYTES;
    options->max_heap_bytes_index = 0;
    options->max_heap_bytes_seen = false;
}

int ruda_cli_heap_limit_parse(
    RudaCliOptions *options,
    const char *value,
    size_t option_index,
    size_t value_index,
    RudaWriter error_writer
) {
    char invalid_message[128];
    uint64_t maximum = (uint64_t)SIZE_MAX < (uint64_t)INT64_MAX
        ? (uint64_t)SIZE_MAX : (uint64_t)INT64_MAX;
    uint64_t parsed = 0;
    size_t index = 0;
    int message_length = 0;
    if (options == NULL || value == NULL) return RUDA_EXIT_INTERNAL;
    message_length = snprintf(
        invalid_message, sizeof(invalid_message),
        "--max-heap-bytes expects a decimal byte count from 1 to %" PRIu64,
        maximum
    );
    if (message_length < 0
        || (size_t)message_length >= sizeof(invalid_message)) {
        return RUDA_EXIT_INTERNAL;
    }
    if (options->max_heap_bytes_seen) {
        return ruda_cli_option_value_error(
            error_writer, options->diagnostic_style,
            RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE, option_index,
            "--max-heap-bytes may be specified only once"
        );
    }
    if (value[0] == '\0') {
        return ruda_cli_option_value_error(
            error_writer, options->diagnostic_style,
            RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE, value_index, invalid_message
        );
    }
    for (index = 0; value[index] != '\0'; index += 1) {
        uint64_t digit = 0;
        if (value[index] < '0' || value[index] > '9') {
            return ruda_cli_option_value_error(
                error_writer, options->diagnostic_style,
                RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE, value_index,
                invalid_message
            );
        }
        digit = (uint64_t)(value[index] - '0');
        if (parsed > (maximum - digit) / UINT64_C(10)) {
            return ruda_cli_option_value_error(
                error_writer, options->diagnostic_style,
                RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE, value_index,
                invalid_message
            );
        }
        parsed = parsed * UINT64_C(10) + digit;
    }
    if (parsed == 0) {
        return ruda_cli_option_value_error(
            error_writer, options->diagnostic_style,
            RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE, value_index, invalid_message
        );
    }
    options->max_heap_bytes = (size_t)parsed;
    options->max_heap_bytes_index = option_index;
    options->max_heap_bytes_seen = true;
    return RUDA_EXIT_SUCCESS;
}
