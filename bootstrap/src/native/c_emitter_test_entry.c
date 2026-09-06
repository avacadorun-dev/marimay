#include "c_emitter_test_entry.h"
#include "c_emitter_internal.h"

MarimayNativeCEmitStatus mmay_native_c_validate_test_entries(
    const MarimayMirFunction *entry,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
) {
    size_t index;
    if (entry == NULL || functions == NULL || options == NULL
        || options->test_entries == NULL || options->test_entry_count == 0
        || options->test_entry_count > count || !options->executable_entry
        || options->executable_takes_arguments || options->executable_returns_status
        || options->executable_returns_result) return MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT;
    if (mmay_native_c_symbol_compare(entry->symbol, options->test_entries[0]) != 0) {
        return MMAY_NATIVE_C_EMIT_INVALID_MIR;
    }
    for (index = 0; index < options->test_entry_count; index += 1) {
        MarimayNativeCEmitOptions selected = *options;
        const MarimayMirFunction *function;
        MarimayNativeCEmitStatus status;
        if (index != 0 && mmay_native_c_symbol_compare(
                options->test_entries[index - 1], options->test_entries[index]
            ) >= 0) return MMAY_NATIVE_C_EMIT_INVALID_MIR;
        function = mmay_native_c_find(functions, count, options->test_entries[index]);
        if (function == NULL) return MMAY_NATIVE_C_EMIT_INVALID_MIR;
        selected.executable_takes_arguments = function->parameter_count == 1;
        status = mmay_native_c_validate_executable_entry(function, functions, count, &selected);
        if (status != MMAY_NATIVE_C_EMIT_OK) return status;
    }
    return MMAY_NATIVE_C_EMIT_OK;
}

bool mmay_native_c_emit_test_selector(MarimayNativeCBuffer *buffer, size_t count) {
    if (count == 0) return false;
    return mmay_native_c_write(buffer,
        "size_t mmay_g_test_index = 0, mmay_g_test_digit = 0;\n"
        "if (mmay_g_argc < 2 || mmay_g_argv == NULL || mmay_g_argv[1] == NULL || mmay_g_argv[1][0] == 0 || (mmay_g_argv[1][0] == '0' && mmay_g_argv[1][1] != 0)) return mmay_g_public_message(\"error\", \"I0002\", (const unsigned char *)\"invalid native test selector\", sizeof(\"invalid native test selector\") - 1, 70);\n"
        "while (mmay_g_argv[1][mmay_g_test_digit] != 0) {\n"
        "  size_t mmay_g_digit;\n"
        "  if (mmay_g_argv[1][mmay_g_test_digit] < '0' || mmay_g_argv[1][mmay_g_test_digit] > '9') return mmay_g_public_message(\"error\", \"I0002\", (const unsigned char *)\"invalid native test selector\", sizeof(\"invalid native test selector\") - 1, 70);\n"
        "  mmay_g_digit = (size_t)(mmay_g_argv[1][mmay_g_test_digit] - '0');\n"
        "  if (mmay_g_test_index > (SIZE_MAX - mmay_g_digit) / 10) return mmay_g_public_message(\"error\", \"I0002\", (const unsigned char *)\"invalid native test selector\", sizeof(\"invalid native test selector\") - 1, 70);\n"
        "  mmay_g_test_index = mmay_g_test_index * 10 + mmay_g_digit;\n"
        "  if (mmay_g_test_index >= %zu) return mmay_g_public_message(\"error\", \"I0002\", (const unsigned char *)\"invalid native test selector\", sizeof(\"invalid native test selector\") - 1, 70);\n"
        "  mmay_g_test_digit += 1;\n"
        "}\n"
        "mmay_g_argc -= 1; mmay_g_argv += 1;\n", count);
}

bool mmay_native_c_emit_test_dispatch(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
) {
    size_t index;
    if (!mmay_native_c_write(buffer, "switch (mmay_g_test_index) {\n")) return false;
    for (index = 0; index < options->test_entry_count; index += 1) {
        const MarimayMirFunction *entry = mmay_native_c_find(
            functions, count, options->test_entries[index]);
        MarimayNativeCEmitOptions selected = *options;
        if (entry == NULL) return false;
        selected.executable_takes_arguments = entry->parameter_count == 1;
        if (!mmay_native_c_write(buffer, "case %zu: {\n", index)
            || (selected.executable_takes_arguments
                && !mmay_native_c_emit_executable_argument_value(buffer, entry))
            || !mmay_native_c_emit_executable_entry(buffer, entry, functions, count, &selected)
            || !mmay_native_c_write(buffer, "}\n")) return false;
    }
    return mmay_native_c_write(buffer,
        "default: mmay_g_exit = 70; goto mmay_g_public_cleanup;\n}\n");
}
