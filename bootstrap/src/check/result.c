#include "checker_internal.h"
#include "checker_intrinsics_api.h"
#include "checker_visible_api.h"

#include <stdint.h>
#include <string.h>

MarimayCheckOptions mmay_check_default_options(void) {
    MarimayCheckOptions options;
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
    options.type_options = mmay_type_table_default_options();
    options.mir_options = mmay_mir_function_default_options();
    return options;
}

MarimayCheckInterpretOptions mmay_check_interpret_default_options(void) {
    MarimayCheckInterpretOptions options;
    options.max_steps_per_function = 10000000;
    options.max_total_steps = 100000000;
    options.max_call_depth = 1024;
    options.execution = NULL;
    options.stdout_sink = NULL;
    options.panic = NULL;
    return options;
}

bool mmay_check_result_init(
    MarimayCheckResult *result,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    const MarimaySourceSnapshot *snapshot,
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
    bool intrinsics_initialized = false;
    bool intrinsic_calls_initialized = false;
    bool visible_initialized = false;
    bool function_index_initialized = false;
    bool symbol_index_initialized = false;
    if (result == NULL || snapshot == NULL || package_name == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->allocator = allocator;
    result->snapshot = snapshot;
    if (!mmay_type_table_init(&result->types, allocator, options.type_options)) {
        return false;
    }
    diagnostics_initialized = mmay_vector_init(
        &result->diagnostics,
        allocator,
        sizeof(MarimayCheckDiagnostic)
    );
    functions_initialized = mmay_vector_init(
        &result->functions,
        allocator,
        sizeof(MarimayCheckedFunction)
    );
    constants_initialized = mmay_vector_init(
        &result->constants,
        allocator,
        sizeof(MarimayCheckedConstant)
    );
    type_aliases_initialized = mmay_vector_init(
        &result->type_aliases,
        allocator,
        sizeof(MarimayCheckedTypeAlias)
    );
    records_initialized = mmay_vector_init(
        &result->records,
        allocator,
        sizeof(MarimayCheckedRecord)
    );
    enums_initialized = mmay_vector_init(
        &result->enums,
        allocator,
        sizeof(MarimayCheckedEnum)
    );
    interfaces_initialized = mmay_vector_init(
        &result->interfaces,
        allocator,
        sizeof(MarimayCheckedInterface)
    );
    symbols_initialized = mmay_vector_init(
        &result->symbols,
        allocator,
        sizeof(MarimayCheckedSymbol)
    );
    occurrences_initialized = mmay_vector_init(
        &result->occurrences,
        allocator,
        sizeof(MarimayCheckedOccurrence)
    );
    completions_initialized = mmay_vector_init(
        &result->completions,
        allocator,
        sizeof(MarimayCheckedCompletion)
    );
    function_index_initialized = mmay_check_function_index_init(result);
    intrinsics_initialized = mmay_vector_init(&result->intrinsic_completions,
        allocator, sizeof(MarimayCheckedIntrinsicCompletion));
    intrinsic_calls_initialized = mmay_vector_init(&result->intrinsic_calls,
        allocator, sizeof(MarimayCheckedIntrinsicCall));
    visible_initialized = mmay_vector_init(&result->visible_symbols,
        allocator, sizeof(MarimayCheckedVisibleSymbol));
    symbol_index_initialized = mmay_check_symbol_index_init(result);
    if (!diagnostics_initialized || !functions_initialized
        || !constants_initialized || !type_aliases_initialized
        || !records_initialized || !enums_initialized
        || !interfaces_initialized || !symbols_initialized
        || !occurrences_initialized || !completions_initialized || !intrinsics_initialized
        || !visible_initialized || !intrinsic_calls_initialized
        || !function_index_initialized || !symbol_index_initialized) {
        if (visible_initialized) mmay_vector_dispose(&result->visible_symbols);
        if (intrinsic_calls_initialized) mmay_vector_dispose(&result->intrinsic_calls);
        if (intrinsics_initialized) {
            mmay_vector_dispose(&result->intrinsic_completions);
        }
        if (completions_initialized) {
            mmay_vector_dispose(&result->completions);
        }
        if (occurrences_initialized) {
            mmay_vector_dispose(&result->occurrences);
        }
        if (symbols_initialized) {
            mmay_vector_dispose(&result->symbols);
        }
        if (interfaces_initialized) {
            mmay_vector_dispose(&result->interfaces);
        }
        if (enums_initialized) {
            mmay_vector_dispose(&result->enums);
        }
        if (records_initialized) {
            mmay_vector_dispose(&result->records);
        }
        if (type_aliases_initialized) {
            mmay_vector_dispose(&result->type_aliases);
        }
        if (constants_initialized) {
            mmay_vector_dispose(&result->constants);
        }
        if (functions_initialized) {
            mmay_vector_dispose(&result->functions);
        }
        if (diagnostics_initialized) {
            mmay_vector_dispose(&result->diagnostics);
        }
        if (symbol_index_initialized) {
            mmay_map_dispose(&result->symbol_index);
        }
        if (function_index_initialized) {
            mmay_map_dispose(&result->function_index);
        }
        mmay_type_table_dispose(&result->types);
        memset(result, 0, sizeof(*result));
        return false;
    }
    result->initialized = true;
    if (!mmay_owned_bytes_copy(
            &result->package_name,
            allocator,
            package_name,
            package_name_length
        )) {
        mmay_check_result_dispose(result);
        return false;
    }
    return true;
}

void mmay_checked_record_value_dispose(MarimayCheckedRecord *record) {
    size_t index = 0;
    if (record == NULL) {
        return;
    }
    for (index = 0; index < record->fields.length; index += 1) {
        MarimayCheckedRecordField *field = mmay_vector_at(&record->fields, index);
        if (field != NULL) {
            mmay_owned_bytes_dispose(&field->name);
            memset(field, 0, sizeof(*field));
        }
    }
    mmay_vector_dispose(&record->fields);
    mmay_vector_dispose(&record->type_parameters);
    mmay_owned_bytes_dispose(&record->name);
    memset(record, 0, sizeof(*record));
}
void mmay_checked_enum_variant_value_dispose(
    MarimayCheckedEnumVariant *variant
) {
    size_t payload_index = 0;
    if (variant == NULL) {
        return;
    }
    for (payload_index = 0; payload_index < variant->payload.length;
        payload_index += 1) {
        MarimayCheckedEnumPayloadItem *item = mmay_vector_at(
            &variant->payload,
            payload_index
        );
        if (item != NULL) {
            mmay_owned_bytes_dispose(&item->name);
            memset(item, 0, sizeof(*item));
        }
    }
    mmay_vector_dispose(&variant->payload);
    mmay_owned_bytes_dispose(&variant->name);
    memset(variant, 0, sizeof(*variant));
}

void mmay_checked_enum_value_dispose(MarimayCheckedEnum *enum_value) {
    size_t variant_index = 0;
    if (enum_value == NULL) {
        return;
    }
    for (variant_index = 0; variant_index < enum_value->variants.length;
        variant_index += 1) {
        MarimayCheckedEnumVariant *variant = mmay_vector_at(
            &enum_value->variants,
            variant_index
        );
        mmay_checked_enum_variant_value_dispose(variant);
    }
    mmay_vector_dispose(&enum_value->variants);
    mmay_vector_dispose(&enum_value->type_parameters);
    mmay_owned_bytes_dispose(&enum_value->name);
    memset(enum_value, 0, sizeof(*enum_value));
}
static void mmay_checked_interface_value_dispose(
    MarimayCheckedInterface *interface_value
) {
    size_t index = 0;
    if (interface_value == NULL) {
        return;
    }
    for (index = 0; index < interface_value->methods.length; index += 1) {
        MarimayCheckedInterfaceMethod *method = mmay_vector_at(
            &interface_value->methods, index
        );
        if (method != NULL) {
            mmay_vector_dispose(&method->parameter_types);
            mmay_owned_bytes_dispose(&method->name);
            memset(method, 0, sizeof(*method));
        }
    }
    mmay_vector_dispose(&interface_value->methods);
    mmay_owned_bytes_dispose(&interface_value->name);
    memset(interface_value, 0, sizeof(*interface_value));
}
void mmay_check_dispose_function_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->functions.length; index += 1) {
        MarimayCheckedFunction *function = mmay_vector_at(
            &result->functions,
            index
        );
        if (function != NULL) {
            mmay_owned_bytes_dispose(&function->name);
            mmay_mir_function_dispose(&function->mir);
            memset(function, 0, sizeof(*function));
        }
    }
    result->functions.length = 0;
}

void mmay_check_dispose_constant_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->constants.length; index += 1) {
        MarimayCheckedConstant *constant = mmay_vector_at(
            &result->constants,
            index
        );
        if (constant != NULL) {
            mmay_owned_bytes_dispose(&constant->name);
            memset(constant, 0, sizeof(*constant));
        }
    }
    result->constants.length = 0;
}

void mmay_check_dispose_type_alias_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->type_aliases.length; index += 1) {
        MarimayCheckedTypeAlias *alias = mmay_vector_at(
            &result->type_aliases,
            index
        );
        if (alias != NULL) {
            mmay_owned_bytes_dispose(&alias->name);
            memset(alias, 0, sizeof(*alias));
        }
    }
    result->type_aliases.length = 0;
}

void mmay_check_dispose_record_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->records.length; index += 1) {
        MarimayCheckedRecord *record = mmay_vector_at(&result->records, index);
        mmay_checked_record_value_dispose(record);
    }
    result->records.length = 0;
}

void mmay_check_dispose_enum_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->enums.length; index += 1) {
        MarimayCheckedEnum *enum_value = mmay_vector_at(&result->enums, index);
        mmay_checked_enum_value_dispose(enum_value);
    }
    result->enums.length = 0;
}

static void mmay_check_dispose_interface_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->interfaces.length; index += 1) {
        mmay_checked_interface_value_dispose(
            mmay_vector_at(&result->interfaces, index)
        );
    }
    result->interfaces.length = 0;
}

void mmay_check_dispose_symbol_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->symbols.length; index += 1) {
        MarimayCheckedSymbol *symbol = mmay_vector_at(&result->symbols, index);
        if (symbol != NULL) {
            mmay_owned_bytes_dispose(&symbol->name);
            mmay_owned_bytes_dispose(&symbol->package_name);
            memset(symbol, 0, sizeof(*symbol));
        }
    }
    result->symbols.length = 0;
}

void mmay_check_dispose_completion_values(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->completions.length; index += 1) {
        MarimayCheckedCompletion *completion = mmay_vector_at(
            &result->completions,
            index
        );
        if (completion != NULL) {
            mmay_owned_bytes_dispose(&completion->name);
            mmay_owned_bytes_dispose(&completion->package_name);
            mmay_owned_bytes_dispose(&completion->free_spelling);
            memset(completion, 0, sizeof(*completion));
        }
    }
    result->completions.length = 0;
}

void mmay_check_dispose_diagnostics(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        MarimayCheckDiagnostic *diagnostic = mmay_vector_at(
            &result->diagnostics,
            index
        );
        if (diagnostic != NULL) {
            mmay_owned_bytes_dispose(&diagnostic->owned_message);
            memset(diagnostic, 0, sizeof(*diagnostic));
        }
    }
    result->diagnostics.length = 0;
    result->error_count = 0;
}

bool mmay_check_function_index_init(MarimayCheckResult *result) {
    static const uint64_t key0 = UINT64_C(0x727564612d66756e);
    static const uint64_t key1 = UINT64_C(0x6374696f6e2d6964);
    return mmay_map_init(
        &result->function_index,
        result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

bool mmay_check_symbol_index_init(MarimayCheckResult *result) {
    static const uint64_t key0 = UINT64_C(0x727564612d73796d);
    static const uint64_t key1 = UINT64_C(0x626f6c2d69647821);
    return mmay_map_init(
        &result->symbol_index,
        result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

void mmay_check_clear_functions(MarimayCheckResult *result) {
    mmay_check_dispose_function_values(result);
    mmay_map_dispose(&result->function_index);
    (void)mmay_check_function_index_init(result);
}

bool mmay_check_clear_semantics(MarimayCheckResult *result) {
    if (result == NULL || !result->initialized) {
        return false;
    }
    mmay_check_dispose_function_values(result);
    mmay_check_dispose_constant_values(result);
    mmay_check_dispose_type_alias_values(result);
    mmay_check_dispose_record_values(result);
    mmay_check_dispose_enum_values(result);
    mmay_check_dispose_interface_values(result);
    mmay_check_dispose_symbol_values(result);
    mmay_check_dispose_completion_values(result);
    mmay_check_dispose_intrinsic_values(result);
    mmay_check_dispose_visible_symbols(result);
    result->occurrences.length = 0;
    result->intrinsic_calls.length = 0;
    mmay_map_dispose(&result->function_index);
    mmay_map_dispose(&result->symbol_index);
    if (!mmay_check_function_index_init(result)
        || !mmay_check_symbol_index_init(result)) {
        return false;
    }
    return true;
}

void mmay_check_result_dispose(MarimayCheckResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    mmay_check_dispose_function_values(result);
    mmay_check_dispose_constant_values(result);
    mmay_check_dispose_type_alias_values(result);
    mmay_check_dispose_record_values(result);
    mmay_check_dispose_enum_values(result);
    mmay_check_dispose_interface_values(result);
    mmay_check_dispose_symbol_values(result);
    mmay_check_dispose_completion_values(result);
    mmay_check_dispose_intrinsic_values(result);
    mmay_check_dispose_visible_symbols(result);
    mmay_check_dispose_diagnostics(result);
    mmay_map_dispose(&result->symbol_index);
    mmay_map_dispose(&result->function_index);
    mmay_vector_dispose(&result->constants);
    mmay_vector_dispose(&result->type_aliases);
    mmay_vector_dispose(&result->records);
    mmay_vector_dispose(&result->enums);
    mmay_vector_dispose(&result->interfaces);
    mmay_vector_dispose(&result->symbols);
    mmay_vector_dispose(&result->occurrences);
    mmay_vector_dispose(&result->completions);
    mmay_vector_dispose(&result->intrinsic_completions);
    mmay_vector_dispose(&result->intrinsic_calls);
    mmay_vector_dispose(&result->visible_symbols);
    mmay_vector_dispose(&result->functions);
    mmay_vector_dispose(&result->diagnostics);
    mmay_type_table_dispose(&result->types);
    mmay_owned_bytes_dispose(&result->package_name);
    memset(result, 0, sizeof(*result));
}
const MarimayCheckDiagnostic *mmay_check_diagnostic(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->diagnostics, index)
        : NULL;
}

const MarimayCheckedFunction *mmay_checked_function(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->functions, index)
        : NULL;
}

const MarimayCheckedConstant *mmay_checked_constant(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->constants, index)
        : NULL;
}

const MarimayCheckedTypeAlias *mmay_checked_type_alias(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->type_aliases, index)
        : NULL;
}

const MarimayCheckedRecord *mmay_checked_record(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->records, index)
        : NULL;
}

const MarimayCheckedEnum *mmay_checked_enum(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->enums, index)
        : NULL;
}

const MarimayCheckedInterface *mmay_checked_interface(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->interfaces, index)
        : NULL;
}

const MarimayCheckedSymbol *mmay_checked_symbol(
    const MarimayCheckResult *result,
    size_t index
) {
    return result != NULL && result->initialized
        ? mmay_vector_at_const(&result->symbols, index)
        : NULL;
}

const MarimayCheckedSymbol *mmay_checked_symbol_by_id(
    const MarimayCheckResult *result,
    MarimaySymbolId symbol
) {
    const size_t *index = NULL;
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    index = mmay_map_get_const(
        &result->symbol_index,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    return index == NULL ? NULL : mmay_checked_symbol(result, *index);
}

const MarimayCheckedSymbol *mmay_checked_symbol_defined_at(
    const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset
) {
    size_t index = 0;
    if (result == NULL || !result->initialized || snapshot == NULL) {
        return NULL;
    }
    for (index = 0; index < result->symbols.length; index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_vector_at_const(
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
