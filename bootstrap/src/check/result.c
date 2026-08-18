#include "checker_internal.h"

#include <stdint.h>
#include <string.h>

RudaCheckOptions ruda_check_default_options(void) {
    RudaCheckOptions options;
    options.max_source_files = 100000;
    options.max_functions = 100000;
    options.max_symbols = 100000;
    options.max_occurrences = 1000000;
    options.max_completions = 1000000;
    options.max_package_name_bytes = 4096;
    options.max_constants = 100000;
    options.max_type_aliases = 100000;
    options.max_records = 100000;
    options.max_fields_per_record = 100000;
    options.max_enums = 100000;
    options.max_variants_per_enum = 100000;
    options.max_payload_items_per_variant = 100000;
    options.max_interfaces = 100000;
    options.max_methods_per_interface = 100000;
    options.max_parameters_per_interface_method = 100000;
    options.max_locals_per_function = 100000;
    options.max_diagnostics = 1000;
    options.max_constant_steps = 10000000;
    options.max_constant_depth = 1024;
    options.max_type_alias_depth = 1024;
    options.max_float_literal_digits = 4096;
    options.type_options = ruda_type_table_default_options();
    options.mir_options = ruda_mir_function_default_options();
    return options;
}

RudaCheckInterpretOptions ruda_check_interpret_default_options(void) {
    RudaCheckInterpretOptions options;
    options.max_steps_per_function = 10000000;
    options.max_total_steps = 100000000;
    options.max_call_depth = 1024;
    options.execution = NULL;
    options.stdout_sink = NULL;
    options.panic = NULL;
    return options;
}

bool ruda_check_result_init(
    RudaCheckResult *result,
    RudaAllocator allocator,
    RudaCheckOptions options,
    const RudaSourceSnapshot *snapshot,
    const char *package_name,
    size_t package_name_length
) {
    bool diagnostics_initialized = false;
    bool functions_initialized = false;
    bool constants_initialized = false;
    bool type_aliases_initialized = false;
    bool records_initialized = false;
    bool enums_initialized = false;
    bool interfaces_initialized = false;
    bool symbols_initialized = false;
    bool occurrences_initialized = false;
    bool completions_initialized = false;
    bool function_index_initialized = false;
    bool symbol_index_initialized = false;
    if (result == NULL || snapshot == NULL || package_name == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->allocator = allocator;
    result->snapshot = snapshot;
    if (!ruda_type_table_init(&result->types, allocator, options.type_options)) {
        return false;
    }
    diagnostics_initialized = ruda_vector_init(
        &result->diagnostics,
        allocator,
        sizeof(RudaCheckDiagnostic)
    );
    functions_initialized = ruda_vector_init(
        &result->functions,
        allocator,
        sizeof(RudaCheckedFunction)
    );
    constants_initialized = ruda_vector_init(
        &result->constants,
        allocator,
        sizeof(RudaCheckedConstant)
    );
    type_aliases_initialized = ruda_vector_init(
        &result->type_aliases,
        allocator,
        sizeof(RudaCheckedTypeAlias)
    );
    records_initialized = ruda_vector_init(
        &result->records,
        allocator,
        sizeof(RudaCheckedRecord)
    );
    enums_initialized = ruda_vector_init(
        &result->enums,
        allocator,
        sizeof(RudaCheckedEnum)
    );
    interfaces_initialized = ruda_vector_init(
        &result->interfaces,
        allocator,
        sizeof(RudaCheckedInterface)
    );
    symbols_initialized = ruda_vector_init(
        &result->symbols,
        allocator,
        sizeof(RudaCheckedSymbol)
    );
    occurrences_initialized = ruda_vector_init(
        &result->occurrences,
        allocator,
        sizeof(RudaCheckedOccurrence)
    );
    completions_initialized = ruda_vector_init(
        &result->completions,
        allocator,
        sizeof(RudaCheckedCompletion)
    );
    function_index_initialized = ruda_check_function_index_init(result);
    symbol_index_initialized = ruda_check_symbol_index_init(result);
    if (!diagnostics_initialized || !functions_initialized
        || !constants_initialized || !type_aliases_initialized
        || !records_initialized || !enums_initialized
        || !interfaces_initialized || !symbols_initialized
        || !occurrences_initialized || !completions_initialized
        || !function_index_initialized || !symbol_index_initialized) {
        if (completions_initialized) {
            ruda_vector_dispose(&result->completions);
        }
        if (occurrences_initialized) {
            ruda_vector_dispose(&result->occurrences);
        }
        if (symbols_initialized) {
            ruda_vector_dispose(&result->symbols);
        }
        if (interfaces_initialized) {
            ruda_vector_dispose(&result->interfaces);
        }
        if (enums_initialized) {
            ruda_vector_dispose(&result->enums);
        }
        if (records_initialized) {
            ruda_vector_dispose(&result->records);
        }
        if (type_aliases_initialized) {
            ruda_vector_dispose(&result->type_aliases);
        }
        if (constants_initialized) {
            ruda_vector_dispose(&result->constants);
        }
        if (functions_initialized) {
            ruda_vector_dispose(&result->functions);
        }
        if (diagnostics_initialized) {
            ruda_vector_dispose(&result->diagnostics);
        }
        if (symbol_index_initialized) {
            ruda_map_dispose(&result->symbol_index);
        }
        if (function_index_initialized) {
            ruda_map_dispose(&result->function_index);
        }
        ruda_type_table_dispose(&result->types);
        memset(result, 0, sizeof(*result));
        return false;
    }
    result->initialized = true;
    if (!ruda_owned_bytes_copy(
            &result->package_name,
            allocator,
            package_name,
            package_name_length
        )) {
        ruda_check_result_dispose(result);
        return false;
    }
    return true;
}

void ruda_checked_record_value_dispose(RudaCheckedRecord *record) {
    size_t index = 0;
    if (record == NULL) {
        return;
    }
    for (index = 0; index < record->fields.length; index += 1) {
        RudaCheckedRecordField *field = ruda_vector_at(&record->fields, index);
        if (field != NULL) {
            ruda_owned_bytes_dispose(&field->name);
            memset(field, 0, sizeof(*field));
        }
    }
    ruda_vector_dispose(&record->fields);
    ruda_owned_bytes_dispose(&record->name);
    memset(record, 0, sizeof(*record));
}
void ruda_checked_enum_variant_value_dispose(
    RudaCheckedEnumVariant *variant
) {
    size_t payload_index = 0;
    if (variant == NULL) {
        return;
    }
    for (payload_index = 0; payload_index < variant->payload.length;
        payload_index += 1) {
        RudaCheckedEnumPayloadItem *item = ruda_vector_at(
            &variant->payload,
            payload_index
        );
        if (item != NULL) {
            ruda_owned_bytes_dispose(&item->name);
            memset(item, 0, sizeof(*item));
        }
    }
    ruda_vector_dispose(&variant->payload);
    ruda_owned_bytes_dispose(&variant->name);
    memset(variant, 0, sizeof(*variant));
}

void ruda_checked_enum_value_dispose(RudaCheckedEnum *enum_value) {
    size_t variant_index = 0;
    if (enum_value == NULL) {
        return;
    }
    for (variant_index = 0; variant_index < enum_value->variants.length;
        variant_index += 1) {
        RudaCheckedEnumVariant *variant = ruda_vector_at(
            &enum_value->variants,
            variant_index
        );
        ruda_checked_enum_variant_value_dispose(variant);
    }
    ruda_vector_dispose(&enum_value->variants);
    ruda_owned_bytes_dispose(&enum_value->name);
    memset(enum_value, 0, sizeof(*enum_value));
}
static void ruda_checked_interface_value_dispose(
    RudaCheckedInterface *interface_value
) {
    size_t index = 0;
    if (interface_value == NULL) {
        return;
    }
    for (index = 0; index < interface_value->methods.length; index += 1) {
        RudaCheckedInterfaceMethod *method = ruda_vector_at(
            &interface_value->methods, index
        );
        if (method != NULL) {
            ruda_vector_dispose(&method->parameter_types);
            ruda_owned_bytes_dispose(&method->name);
            memset(method, 0, sizeof(*method));
        }
    }
    ruda_vector_dispose(&interface_value->methods);
    ruda_owned_bytes_dispose(&interface_value->name);
    memset(interface_value, 0, sizeof(*interface_value));
}
void ruda_check_dispose_function_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->functions.length; index += 1) {
        RudaCheckedFunction *function = ruda_vector_at(
            &result->functions,
            index
        );
        if (function != NULL) {
            ruda_owned_bytes_dispose(&function->name);
            ruda_mir_function_dispose(&function->mir);
            memset(function, 0, sizeof(*function));
        }
    }
    result->functions.length = 0;
}

void ruda_check_dispose_constant_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->constants.length; index += 1) {
        RudaCheckedConstant *constant = ruda_vector_at(
            &result->constants,
            index
        );
        if (constant != NULL) {
            ruda_owned_bytes_dispose(&constant->name);
            memset(constant, 0, sizeof(*constant));
        }
    }
    result->constants.length = 0;
}

void ruda_check_dispose_type_alias_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->type_aliases.length; index += 1) {
        RudaCheckedTypeAlias *alias = ruda_vector_at(
            &result->type_aliases,
            index
        );
        if (alias != NULL) {
            ruda_owned_bytes_dispose(&alias->name);
            memset(alias, 0, sizeof(*alias));
        }
    }
    result->type_aliases.length = 0;
}

void ruda_check_dispose_record_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->records.length; index += 1) {
        RudaCheckedRecord *record = ruda_vector_at(&result->records, index);
        ruda_checked_record_value_dispose(record);
    }
    result->records.length = 0;
}

void ruda_check_dispose_enum_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->enums.length; index += 1) {
        RudaCheckedEnum *enum_value = ruda_vector_at(&result->enums, index);
        ruda_checked_enum_value_dispose(enum_value);
    }
    result->enums.length = 0;
}

static void ruda_check_dispose_interface_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->interfaces.length; index += 1) {
        ruda_checked_interface_value_dispose(
            ruda_vector_at(&result->interfaces, index)
        );
    }
    result->interfaces.length = 0;
}

void ruda_check_dispose_symbol_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->symbols.length; index += 1) {
        RudaCheckedSymbol *symbol = ruda_vector_at(&result->symbols, index);
        if (symbol != NULL) {
            ruda_owned_bytes_dispose(&symbol->name);
            ruda_owned_bytes_dispose(&symbol->package_name);
            memset(symbol, 0, sizeof(*symbol));
        }
    }
    result->symbols.length = 0;
}

void ruda_check_dispose_completion_values(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->completions.length; index += 1) {
        RudaCheckedCompletion *completion = ruda_vector_at(
            &result->completions,
            index
        );
        if (completion != NULL) {
            ruda_owned_bytes_dispose(&completion->name);
            ruda_owned_bytes_dispose(&completion->package_name);
            ruda_owned_bytes_dispose(&completion->free_spelling);
            memset(completion, 0, sizeof(*completion));
        }
    }
    result->completions.length = 0;
}

void ruda_check_dispose_diagnostics(RudaCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        RudaCheckDiagnostic *diagnostic = ruda_vector_at(
            &result->diagnostics,
            index
        );
        if (diagnostic != NULL) {
            ruda_owned_bytes_dispose(&diagnostic->owned_message);
            memset(diagnostic, 0, sizeof(*diagnostic));
        }
    }
    result->diagnostics.length = 0;
}

bool ruda_check_function_index_init(RudaCheckResult *result) {
    static const uint64_t key0 = UINT64_C(0x727564612d66756e);
    static const uint64_t key1 = UINT64_C(0x6374696f6e2d6964);
    return ruda_map_init(
        &result->function_index,
        result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

bool ruda_check_symbol_index_init(RudaCheckResult *result) {
    static const uint64_t key0 = UINT64_C(0x727564612d73796d);
    static const uint64_t key1 = UINT64_C(0x626f6c2d69647821);
    return ruda_map_init(
        &result->symbol_index,
        result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

void ruda_check_clear_functions(RudaCheckResult *result) {
    ruda_check_dispose_function_values(result);
    ruda_map_dispose(&result->function_index);
    (void)ruda_check_function_index_init(result);
}

bool ruda_check_clear_semantics(RudaCheckResult *result) {
    if (result == NULL || !result->initialized) {
        return false;
    }
    ruda_check_dispose_function_values(result);
    ruda_check_dispose_constant_values(result);
    ruda_check_dispose_type_alias_values(result);
    ruda_check_dispose_record_values(result);
    ruda_check_dispose_enum_values(result);
    ruda_check_dispose_interface_values(result);
    ruda_check_dispose_symbol_values(result);
    ruda_check_dispose_completion_values(result);
    result->occurrences.length = 0;
    ruda_map_dispose(&result->function_index);
    ruda_map_dispose(&result->symbol_index);
    if (!ruda_check_function_index_init(result)
        || !ruda_check_symbol_index_init(result)) {
        return false;
    }
    return true;
}

void ruda_check_result_dispose(RudaCheckResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_check_dispose_function_values(result);
    ruda_check_dispose_constant_values(result);
    ruda_check_dispose_type_alias_values(result);
    ruda_check_dispose_record_values(result);
    ruda_check_dispose_enum_values(result);
    ruda_check_dispose_interface_values(result);
    ruda_check_dispose_symbol_values(result);
    ruda_check_dispose_completion_values(result);
    ruda_check_dispose_diagnostics(result);
    ruda_map_dispose(&result->symbol_index);
    ruda_map_dispose(&result->function_index);
    ruda_vector_dispose(&result->constants);
    ruda_vector_dispose(&result->type_aliases);
    ruda_vector_dispose(&result->records);
    ruda_vector_dispose(&result->enums);
    ruda_vector_dispose(&result->interfaces);
    ruda_vector_dispose(&result->symbols);
    ruda_vector_dispose(&result->occurrences);
    ruda_vector_dispose(&result->completions);
    ruda_vector_dispose(&result->functions);
    ruda_vector_dispose(&result->diagnostics);
    ruda_type_table_dispose(&result->types);
    ruda_owned_bytes_dispose(&result->package_name);
    memset(result, 0, sizeof(*result));
}
const RudaCheckDiagnostic *ruda_check_diagnostic(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->diagnostics, index)
        : NULL;
}

const RudaCheckedFunction *ruda_checked_function(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->functions, index)
        : NULL;
}

const RudaCheckedConstant *ruda_checked_constant(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->constants, index)
        : NULL;
}

const RudaCheckedTypeAlias *ruda_checked_type_alias(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->type_aliases, index)
        : NULL;
}

const RudaCheckedRecord *ruda_checked_record(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->records, index)
        : NULL;
}

const RudaCheckedEnum *ruda_checked_enum(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->enums, index)
        : NULL;
}

const RudaCheckedInterface *ruda_checked_interface(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->interfaces, index)
        : NULL;
}

const RudaCheckedSymbol *ruda_checked_symbol(
    const RudaCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? ruda_vector_at_const(&result->symbols, index)
        : NULL;
}

const RudaCheckedSymbol *ruda_checked_symbol_by_id(
    const RudaCheckResult *result,
    RudaSymbolId symbol
) {
    const size_t *index = NULL;
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    index = ruda_map_get_const(
        &result->symbol_index,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    return index == NULL ? NULL : ruda_checked_symbol(result, *index);
}

const RudaCheckedSymbol *ruda_checked_symbol_defined_at(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
) {
    size_t index = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return NULL;
    }
    for (index = 0; index < result->symbols.length; index += 1) {
        const RudaCheckedSymbol *symbol = ruda_vector_at_const(
            &result->symbols,
            index
        );
        if (symbol != NULL && symbol->snapshot == snapshot
            && byte_offset >= symbol->start && byte_offset < symbol->end) {
            return symbol;
        }
    }
    return NULL;
}
