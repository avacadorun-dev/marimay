#include "ruda/check/entry.h"

#include <string.h>

static bool ruda_entry_bytes_equal(
    const RudaOwnedBytes *bytes,
    const void *expected,
    size_t expected_length
) {
    return bytes != NULL && bytes->length == expected_length
        && (expected_length == 0
            || memcmp(bytes->data, expected, expected_length) == 0);
}

static bool ruda_entry_result_symbol(RudaSymbolId *symbol) {
    static const char package_name[] = "std";
    static const char source_path[] = "prelude";
    static const char declaration_name[] = "Result";
    return ruda_symbol_id_from_bytes(
        package_name, sizeof(package_name) - 1,
        source_path, sizeof(source_path) - 1,
        declaration_name, sizeof(declaration_name) - 1,
        0, symbol
    );
}

static const RudaCheckedFunction *ruda_entry_function_by_symbol(
    const RudaCheckResult *result,
    RudaSymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < result->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            result, index
        );
        if (function != NULL && ruda_symbol_id_equal(
                function->mir.symbol, symbol
            )) {
            return function;
        }
    }
    return NULL;
}

static const RudaCheckedFunction *ruda_entry_error_message(
    const RudaCheckResult *result,
    RudaTypeId error_type,
    const char *root_package,
    size_t root_package_length
) {
    static const char message_name[] = "message";
    size_t index = 0;
    for (index = 0; index < result->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            result, index
        );
        const RudaTypeDescriptor *signature = function == NULL ? NULL
            : ruda_type_descriptor(&result->types, function->signature_type);
        const RudaCheckedSymbol *symbol = function == NULL ? NULL
            : ruda_checked_symbol_by_id(result, function->mir.symbol);
        if (function != NULL && function->dot_eligible
            && !function->is_external
            && symbol != NULL
            && (function->is_public || ruda_entry_bytes_equal(
                &symbol->package_name, root_package, root_package_length
            ))
            && ruda_entry_bytes_equal(
                &function->name, message_name, sizeof(message_name) - 1
            ) && signature != NULL
            && signature->kind == RUDA_TYPE_KIND_FUNCTION
            && signature->parameter_count == 1
            && signature->argument_count == 2
            && signature->arguments[0] == error_type
            && signature->arguments[1] == RUDA_TYPE_STRING) {
            return function;
        }
    }
    return NULL;
}

RudaCheckEntryStatus ruda_check_select_entry(
    const RudaCheckResult *result,
    const char *root_package,
    size_t root_package_length,
    RudaCheckedEntry *entry
) {
    static const char main_name[] = "main";
    const RudaCheckedSymbol *main_symbol = NULL;
    const RudaCheckedFunction *main_function = NULL;
    const RudaTypeDescriptor *signature = NULL;
    const RudaTypeDescriptor *return_descriptor = NULL;
    RudaSymbolId result_symbol;
    RudaTypeId return_type = RUDA_TYPE_INVALID;
    size_t index = 0;
    if (entry != NULL) {
        memset(entry, 0, sizeof(*entry));
        entry->error_type = RUDA_TYPE_INVALID;
    }
    if (result == NULL || !result->initialized || root_package == NULL
        || root_package_length == 0 || entry == NULL) {
        return RUDA_CHECK_ENTRY_INVALID_ARGUMENT;
    }
    for (index = 0; index < result->symbols.length; index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(result, index);
        if (symbol != NULL && symbol->kind == RUDA_CHECKED_SYMBOL_FUNCTION
            && ruda_entry_bytes_equal(
                &symbol->package_name, root_package, root_package_length
            ) && ruda_entry_bytes_equal(
                &symbol->name, main_name, sizeof(main_name) - 1
            )) {
            main_symbol = symbol;
            break;
        }
    }
    if (main_symbol == NULL) {
        return RUDA_CHECK_ENTRY_NOT_FOUND;
    }
    main_function = ruda_entry_function_by_symbol(result, main_symbol->symbol);
    signature = main_function == NULL ? NULL
        : ruda_type_descriptor(&result->types, main_function->signature_type);
    if (main_function == NULL || main_function->is_external
        || main_function->is_test || main_function->is_fuzz
        || signature == NULL || signature->kind != RUDA_TYPE_KIND_FUNCTION
        || signature->parameter_count != 0 || signature->argument_count != 1) {
        return RUDA_CHECK_ENTRY_INVALID_SIGNATURE;
    }
    return_type = signature->arguments[0];
    if (return_type == RUDA_TYPE_VOID) {
        entry->function = main_function;
        entry->symbol = main_symbol;
        return RUDA_CHECK_ENTRY_OK;
    }
    return_descriptor = ruda_type_descriptor(&result->types, return_type);
    if (!ruda_entry_result_symbol(&result_symbol)
        || return_descriptor == NULL
        || return_descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || return_descriptor->argument_count != 2
        || !ruda_symbol_id_equal(return_descriptor->symbol, result_symbol)
        || return_descriptor->arguments[0] != RUDA_TYPE_VOID) {
        return RUDA_CHECK_ENTRY_INVALID_SIGNATURE;
    }
    entry->error_message = ruda_entry_error_message(
        result, return_descriptor->arguments[1],
        root_package, root_package_length
    );
    if (entry->error_message == NULL) {
        return RUDA_CHECK_ENTRY_ERROR_CONTRACT_MISSING;
    }
    entry->function = main_function;
    entry->symbol = main_symbol;
    entry->error_type = return_descriptor->arguments[1];
    entry->returns_result = true;
    return RUDA_CHECK_ENTRY_OK;
}
