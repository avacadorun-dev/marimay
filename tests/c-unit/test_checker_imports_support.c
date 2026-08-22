#include "test_checker_imports_support.h"

#include <string.h>

static size_t mmay_checker_import_package(
    MarimayCheckerImportFixture *fixture,
    const char *path
) {
    size_t index = 0;
    for (index = 0; index < fixture->package_count; index += 1) {
        if (strcmp(fixture->packages[index].import_path, path) == 0) {
            return index;
        }
    }
    if (fixture->package_count >= MMAY_TEST_IMPORT_MAX_PACKAGES) {
        return SIZE_MAX;
    }
    index = fixture->package_count;
    fixture->package_count += 1;
    fixture->packages[index].import_path = path;
    fixture->packages[index].import_path_length = strlen(path);
    fixture->packages[index].asts = fixture->package_asts[index];
    return index;
}

bool mmay_checker_import_fixture_init(
    MarimayCheckerImportFixture *fixture,
    const MarimayTestImportSource *sources,
    size_t source_count
) {
    size_t index = 0;
    memset(fixture, 0, sizeof(*fixture));
    if (sources == NULL || source_count == 0
        || source_count > MMAY_TEST_IMPORT_MAX_FILES) {
        return false;
    }
    for (index = 0; index < source_count; index += 1) {
        size_t package_index = mmay_checker_import_package(
            fixture, sources[index].package_path
        );
        MarimayCheckPackageInput *package = NULL;
        fixture->file_count = index + 1;
        if (package_index == SIZE_MAX || !mmay_checker_fixture_init_path(
                &fixture->files[index],
                sources[index].source_path,
                sources[index].source
            )) {
            mmay_checker_import_fixture_dispose(fixture);
            return false;
        }
        package = &fixture->packages[package_index];
        if (package->ast_count >= MMAY_TEST_IMPORT_MAX_FILES) {
            mmay_checker_import_fixture_dispose(fixture);
            return false;
        }
        fixture->package_asts[package_index][package->ast_count] =
            &fixture->files[index].ast;
        package->ast_count += 1;
    }
    return true;
}

void mmay_checker_import_fixture_dispose(
    MarimayCheckerImportFixture *fixture
) {
    size_t index = fixture == NULL ? 0 : fixture->file_count;
    while (index != 0) {
        index -= 1;
        mmay_checker_fixture_dispose(&fixture->files[index]);
    }
    if (fixture != NULL) {
        memset(fixture, 0, sizeof(*fixture));
    }
}

MarimayCheckStatus mmay_checker_import_fixture_check(
    const MarimayCheckerImportFixture *fixture,
    const char *root,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
) {
    return mmay_check_package_graph(
        fixture->packages,
        fixture->package_count,
        root,
        strlen(root),
        allocator,
        options,
        result
    );
}

const MarimayCheckedFunction *mmay_checker_import_function(
    const MarimayCheckResult *result,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            result, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            if (result_index != NULL) {
                *result_index = index;
            }
            return function;
        }
    }
    return NULL;
}

static bool mmay_import_mir_append(
    MarimayVector *bytes,
    const void *value,
    size_t size
) {
    return size == 0 || mmay_vector_append(bytes, value, size);
}

typedef bool (*MarimayImportMirAppendElement)(
    MarimayVector *bytes,
    const void *element
);

static bool mmay_import_mir_append_elements(
    MarimayVector *bytes,
    const MarimayVector *values,
    MarimayImportMirAppendElement append_element
) {
    size_t index = 0;
    if (values == NULL || append_element == NULL
        || !mmay_import_mir_append(
            bytes, &values->length, sizeof(values->length)
        )) {
        return false;
    }
    for (index = 0; index < values->length; index += 1) {
        const void *element = mmay_vector_at_const(values, index);
        if (element == NULL || !append_element(bytes, element)) {
            return false;
        }
    }
    return true;
}

#define MMAY_IMPORT_MIR_FIELD(bytes, value, field) \
    mmay_import_mir_append( \
        (bytes), &(value)->field, sizeof((value)->field) \
    )

static bool mmay_import_mir_append_type(
    MarimayVector *bytes,
    const void *element
) {
    return mmay_import_mir_append(bytes, element, sizeof(MarimayTypeId));
}

static bool mmay_import_mir_append_symbol(
    MarimayVector *bytes,
    const void *element
) {
    return mmay_import_mir_append(bytes, element, sizeof(MarimaySymbolId));
}

static bool mmay_import_mir_append_call(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirCall *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, callee)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, result_type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, interface_slot)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool mmay_import_mir_append_call_argument(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirCallArgument *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, register_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool mmay_import_mir_append_record(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirRecord *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, field_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, field_count);
}

static bool mmay_import_mir_append_record_field(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirRecordField *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, is_mutable);
}

static bool mmay_import_mir_append_record_construction(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirRecordConstruction *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, record)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool mmay_import_mir_append_record_argument(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirRecordArgument *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, register_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool mmay_import_mir_append_enum(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirEnum *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, variant_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, variant_count)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, payload_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, payload_count);
}

static bool mmay_import_mir_append_enum_variant(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirEnumVariant *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, kind)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, payload_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, payload_count);
}

static bool mmay_import_mir_append_enum_payload(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirEnumPayload *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool mmay_import_mir_append_enum_construction(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirEnumConstruction *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, enum_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, variant)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool mmay_import_mir_append_enum_argument(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirEnumArgument *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, register_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool mmay_import_mir_append_array(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirArray *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, element_type);
}

static bool mmay_import_mir_append_array_construction(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirArrayConstruction *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, array)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool mmay_import_mir_append_array_argument(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirArrayArgument *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, register_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool mmay_import_mir_append_interface(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirInterface *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, method_count);
}

static bool mmay_import_mir_append_interface_construction(
    MarimayVector *bytes,
    const void *element
) {
    const MarimayMirInterfaceConstruction *value = element;
    return MMAY_IMPORT_MIR_FIELD(bytes, value, interface_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, payload)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, payload_type)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, method_start)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, method_count);
}

static bool mmay_import_mir_append_instruction(
    MarimayVector *bytes,
    const MarimayMirInstruction *value
) {
    return MMAY_IMPORT_MIR_FIELD(bytes, value, kind)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, target)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, first)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, second)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, third)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, call)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, record)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, record_construction)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, enum_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, enum_construction)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, string_constant)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, array)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, array_construction)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, interface_id)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, interface_construction)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, variant)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, field)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, integer)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, f64_bits)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, boolean);
}

static bool mmay_import_mir_append_terminator(
    MarimayVector *bytes,
    const MarimayMirTerminator *value
) {
    return MMAY_IMPORT_MIR_FIELD(bytes, value, kind)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, value)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, call)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, target)
        && MMAY_IMPORT_MIR_FIELD(bytes, value, otherwise);
}

static bool mmay_import_mir_append_blocks(
    MarimayVector *bytes,
    const MarimayMirFunction *function
) {
    size_t block_index = 0;
    if (!mmay_import_mir_append(
            bytes, &function->blocks.length,
            sizeof(function->blocks.length)
        )) {
        return false;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL || !mmay_import_mir_append(
                bytes, &block->instructions.length,
                sizeof(block->instructions.length)
            )) {
            return false;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *source = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (source == NULL) {
                return false;
            }
            if (!mmay_import_mir_append_instruction(bytes, source)) {
                return false;
            }
        }
        if (!mmay_import_mir_append_terminator(
                bytes, &block->terminator
            )) {
            return false;
        }
    }
    return true;
}

static bool mmay_import_mir_append_strings(
    MarimayVector *bytes,
    const MarimayMirFunction *function
) {
    size_t index = 0;
    if (!mmay_import_mir_append(
            bytes, &function->string_constants.length,
            sizeof(function->string_constants.length)
        )) {
        return false;
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        const MarimayMirStringConstant *string_value = mmay_vector_at_const(
            &function->string_constants, index
        );
        if (string_value == NULL || !mmay_import_mir_append(
                bytes, &string_value->value.length,
                sizeof(string_value->value.length)
            ) || !mmay_import_mir_append(
                bytes, string_value->value.data, string_value->value.length
            )) {
            return false;
        }
    }
    return true;
}

static bool mmay_import_mir_append_function(
    MarimayVector *bytes,
    const MarimayMirFunction *function
) {
    return function != NULL && function->initialized
        && mmay_import_mir_append(
            bytes, &function->symbol, sizeof(function->symbol)
        )
        && mmay_import_mir_append(
            bytes, &function->result_type, sizeof(function->result_type)
        )
        && mmay_import_mir_append(
            bytes, &function->parameter_count,
            sizeof(function->parameter_count)
        )
        && mmay_import_mir_append(
            bytes, &function->instruction_count,
            sizeof(function->instruction_count)
        )
        && mmay_import_mir_append(
            bytes, &function->string_bytes, sizeof(function->string_bytes)
        )
        && mmay_import_mir_append_elements(
            bytes, &function->register_types, mmay_import_mir_append_type
        )
        && mmay_import_mir_append_blocks(bytes, function)
        && mmay_import_mir_append_elements(
            bytes, &function->calls, mmay_import_mir_append_call
        )
        && mmay_import_mir_append_elements(
            bytes, &function->call_arguments,
            mmay_import_mir_append_call_argument
        )
        && mmay_import_mir_append_elements(
            bytes, &function->records, mmay_import_mir_append_record
        )
        && mmay_import_mir_append_elements(
            bytes, &function->record_fields,
            mmay_import_mir_append_record_field
        )
        && mmay_import_mir_append_elements(
            bytes, &function->record_constructions,
            mmay_import_mir_append_record_construction
        )
        && mmay_import_mir_append_elements(
            bytes, &function->record_arguments,
            mmay_import_mir_append_record_argument
        )
        && mmay_import_mir_append_elements(
            bytes, &function->enums, mmay_import_mir_append_enum
        )
        && mmay_import_mir_append_elements(
            bytes, &function->enum_variants,
            mmay_import_mir_append_enum_variant
        )
        && mmay_import_mir_append_elements(
            bytes, &function->enum_payloads,
            mmay_import_mir_append_enum_payload
        )
        && mmay_import_mir_append_elements(
            bytes, &function->enum_constructions,
            mmay_import_mir_append_enum_construction
        )
        && mmay_import_mir_append_elements(
            bytes, &function->enum_arguments,
            mmay_import_mir_append_enum_argument
        )
        && mmay_import_mir_append_strings(bytes, function)
        && mmay_import_mir_append_elements(
            bytes, &function->arrays, mmay_import_mir_append_array
        )
        && mmay_import_mir_append_elements(
            bytes, &function->array_constructions,
            mmay_import_mir_append_array_construction
        )
        && mmay_import_mir_append_elements(
            bytes, &function->array_arguments,
            mmay_import_mir_append_array_argument
        )
        && mmay_import_mir_append_elements(
            bytes, &function->interfaces,
            mmay_import_mir_append_interface
        )
        && mmay_import_mir_append_elements(
            bytes, &function->interface_constructions,
            mmay_import_mir_append_interface_construction
        )
        && mmay_import_mir_append_elements(
            bytes, &function->interface_methods,
            mmay_import_mir_append_symbol
        );
}

#undef MMAY_IMPORT_MIR_FIELD

bool mmay_checker_import_mir_bytes(
    const MarimayCheckResult *result,
    MarimayAllocator allocator,
    MarimayOwnedBytes *bytes
) {
    MarimayVector buffer;
    size_t index = 0;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (result == NULL || !result->initialized || bytes == NULL) {
        return false;
    }
    memset(bytes, 0, sizeof(*bytes));
    if (!mmay_vector_init(&buffer, allocator, sizeof(unsigned char))
        || !mmay_import_mir_append(
            &buffer, &result->functions.length,
            sizeof(result->functions.length)
        )) {
        goto cleanup;
    }
    for (index = 0; index < result->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            result, index
        );
        if (function == NULL || !mmay_import_mir_append_function(
                &buffer, &function->mir
            )) {
            goto cleanup;
        }
    }
    accepted = mmay_owned_bytes_copy(
        bytes, allocator, buffer.data, buffer.length
    );
cleanup:
    mmay_vector_dispose(&buffer);
    return accepted;
}
