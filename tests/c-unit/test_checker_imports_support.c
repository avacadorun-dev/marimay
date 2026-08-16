#include "test_checker_imports_support.h"

#include <string.h>

static size_t ruda_checker_import_package(
    RudaCheckerImportFixture *fixture,
    const char *path
) {
    size_t index = 0;
    for (index = 0; index < fixture->package_count; index += 1) {
        if (strcmp(fixture->packages[index].import_path, path) == 0) {
            return index;
        }
    }
    if (fixture->package_count >= RUDA_TEST_IMPORT_MAX_PACKAGES) {
        return SIZE_MAX;
    }
    index = fixture->package_count;
    fixture->package_count += 1;
    fixture->packages[index].import_path = path;
    fixture->packages[index].import_path_length = strlen(path);
    fixture->packages[index].asts = fixture->package_asts[index];
    return index;
}

bool ruda_checker_import_fixture_init(
    RudaCheckerImportFixture *fixture,
    const RudaTestImportSource *sources,
    size_t source_count
) {
    size_t index = 0;
    memset(fixture, 0, sizeof(*fixture));
    if (sources == NULL || source_count == 0
        || source_count > RUDA_TEST_IMPORT_MAX_FILES) {
        return false;
    }
    for (index = 0; index < source_count; index += 1) {
        size_t package_index = ruda_checker_import_package(
            fixture, sources[index].package_path
        );
        RudaCheckPackageInput *package = NULL;
        fixture->file_count = index + 1;
        if (package_index == SIZE_MAX || !ruda_checker_fixture_init_path(
                &fixture->files[index],
                sources[index].source_path,
                sources[index].source
            )) {
            ruda_checker_import_fixture_dispose(fixture);
            return false;
        }
        package = &fixture->packages[package_index];
        if (package->ast_count >= RUDA_TEST_IMPORT_MAX_FILES) {
            ruda_checker_import_fixture_dispose(fixture);
            return false;
        }
        fixture->package_asts[package_index][package->ast_count] =
            &fixture->files[index].ast;
        package->ast_count += 1;
    }
    return true;
}

void ruda_checker_import_fixture_dispose(
    RudaCheckerImportFixture *fixture
) {
    size_t index = fixture == NULL ? 0 : fixture->file_count;
    while (index != 0) {
        index -= 1;
        ruda_checker_fixture_dispose(&fixture->files[index]);
    }
    if (fixture != NULL) {
        memset(fixture, 0, sizeof(*fixture));
    }
}

RudaCheckStatus ruda_checker_import_fixture_check(
    const RudaCheckerImportFixture *fixture,
    const char *root,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
) {
    return ruda_check_package_graph(
        fixture->packages,
        fixture->package_count,
        root,
        strlen(root),
        allocator,
        options,
        result
    );
}

const RudaCheckedFunction *ruda_checker_import_function(
    const RudaCheckResult *result,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; result != NULL && index < result->functions.length;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
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

static bool ruda_import_mir_append(
    RudaVector *bytes,
    const void *value,
    size_t size
) {
    return size == 0 || ruda_vector_append(bytes, value, size);
}

typedef bool (*RudaImportMirAppendElement)(
    RudaVector *bytes,
    const void *element
);

static bool ruda_import_mir_append_elements(
    RudaVector *bytes,
    const RudaVector *values,
    RudaImportMirAppendElement append_element
) {
    size_t index = 0;
    if (values == NULL || append_element == NULL
        || !ruda_import_mir_append(
            bytes, &values->length, sizeof(values->length)
        )) {
        return false;
    }
    for (index = 0; index < values->length; index += 1) {
        const void *element = ruda_vector_at_const(values, index);
        if (element == NULL || !append_element(bytes, element)) {
            return false;
        }
    }
    return true;
}

#define RUDA_IMPORT_MIR_FIELD(bytes, value, field) \
    ruda_import_mir_append( \
        (bytes), &(value)->field, sizeof((value)->field) \
    )

static bool ruda_import_mir_append_type(
    RudaVector *bytes,
    const void *element
) {
    return ruda_import_mir_append(bytes, element, sizeof(RudaTypeId));
}

static bool ruda_import_mir_append_symbol(
    RudaVector *bytes,
    const void *element
) {
    return ruda_import_mir_append(bytes, element, sizeof(RudaSymbolId));
}

static bool ruda_import_mir_append_call(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirCall *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, callee)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, result_type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, interface_slot)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool ruda_import_mir_append_call_argument(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirCallArgument *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, register_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool ruda_import_mir_append_record(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirRecord *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, field_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, field_count);
}

static bool ruda_import_mir_append_record_field(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirRecordField *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, is_mutable);
}

static bool ruda_import_mir_append_record_construction(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirRecordConstruction *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, record)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool ruda_import_mir_append_record_argument(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirRecordArgument *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, register_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool ruda_import_mir_append_enum(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirEnum *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, variant_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, variant_count)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, payload_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, payload_count);
}

static bool ruda_import_mir_append_enum_variant(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirEnumVariant *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, kind)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, payload_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, payload_count);
}

static bool ruda_import_mir_append_enum_payload(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirEnumPayload *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool ruda_import_mir_append_enum_construction(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirEnumConstruction *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, enum_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, variant)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool ruda_import_mir_append_enum_argument(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirEnumArgument *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, register_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool ruda_import_mir_append_array(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirArray *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, element_type);
}

static bool ruda_import_mir_append_array_construction(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirArrayConstruction *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, array)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, argument_count);
}

static bool ruda_import_mir_append_array_argument(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirArrayArgument *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, register_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, type);
}

static bool ruda_import_mir_append_interface(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirInterface *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, method_count);
}

static bool ruda_import_mir_append_interface_construction(
    RudaVector *bytes,
    const void *element
) {
    const RudaMirInterfaceConstruction *value = element;
    return RUDA_IMPORT_MIR_FIELD(bytes, value, interface_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, payload)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, payload_type)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, method_start)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, method_count);
}

static bool ruda_import_mir_append_instruction(
    RudaVector *bytes,
    const RudaMirInstruction *value
) {
    return RUDA_IMPORT_MIR_FIELD(bytes, value, kind)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, target)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, first)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, second)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, third)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, call)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, record)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, record_construction)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, enum_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, enum_construction)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, string_constant)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, array)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, array_construction)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, interface_id)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, interface_construction)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, variant)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, field)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, integer)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, f64_bits)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, boolean);
}

static bool ruda_import_mir_append_terminator(
    RudaVector *bytes,
    const RudaMirTerminator *value
) {
    return RUDA_IMPORT_MIR_FIELD(bytes, value, kind)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, value)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, call)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, target)
        && RUDA_IMPORT_MIR_FIELD(bytes, value, otherwise);
}

static bool ruda_import_mir_append_blocks(
    RudaVector *bytes,
    const RudaMirFunction *function
) {
    size_t block_index = 0;
    if (!ruda_import_mir_append(
            bytes, &function->blocks.length,
            sizeof(function->blocks.length)
        )) {
        return false;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL || !ruda_import_mir_append(
                bytes, &block->instructions.length,
                sizeof(block->instructions.length)
            )) {
            return false;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *source = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (source == NULL) {
                return false;
            }
            if (!ruda_import_mir_append_instruction(bytes, source)) {
                return false;
            }
        }
        if (!ruda_import_mir_append_terminator(
                bytes, &block->terminator
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_import_mir_append_strings(
    RudaVector *bytes,
    const RudaMirFunction *function
) {
    size_t index = 0;
    if (!ruda_import_mir_append(
            bytes, &function->string_constants.length,
            sizeof(function->string_constants.length)
        )) {
        return false;
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        const RudaMirStringConstant *string_value = ruda_vector_at_const(
            &function->string_constants, index
        );
        if (string_value == NULL || !ruda_import_mir_append(
                bytes, &string_value->value.length,
                sizeof(string_value->value.length)
            ) || !ruda_import_mir_append(
                bytes, string_value->value.data, string_value->value.length
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_import_mir_append_function(
    RudaVector *bytes,
    const RudaMirFunction *function
) {
    return function != NULL && function->initialized
        && ruda_import_mir_append(
            bytes, &function->symbol, sizeof(function->symbol)
        )
        && ruda_import_mir_append(
            bytes, &function->result_type, sizeof(function->result_type)
        )
        && ruda_import_mir_append(
            bytes, &function->parameter_count,
            sizeof(function->parameter_count)
        )
        && ruda_import_mir_append(
            bytes, &function->instruction_count,
            sizeof(function->instruction_count)
        )
        && ruda_import_mir_append(
            bytes, &function->string_bytes, sizeof(function->string_bytes)
        )
        && ruda_import_mir_append_elements(
            bytes, &function->register_types, ruda_import_mir_append_type
        )
        && ruda_import_mir_append_blocks(bytes, function)
        && ruda_import_mir_append_elements(
            bytes, &function->calls, ruda_import_mir_append_call
        )
        && ruda_import_mir_append_elements(
            bytes, &function->call_arguments,
            ruda_import_mir_append_call_argument
        )
        && ruda_import_mir_append_elements(
            bytes, &function->records, ruda_import_mir_append_record
        )
        && ruda_import_mir_append_elements(
            bytes, &function->record_fields,
            ruda_import_mir_append_record_field
        )
        && ruda_import_mir_append_elements(
            bytes, &function->record_constructions,
            ruda_import_mir_append_record_construction
        )
        && ruda_import_mir_append_elements(
            bytes, &function->record_arguments,
            ruda_import_mir_append_record_argument
        )
        && ruda_import_mir_append_elements(
            bytes, &function->enums, ruda_import_mir_append_enum
        )
        && ruda_import_mir_append_elements(
            bytes, &function->enum_variants,
            ruda_import_mir_append_enum_variant
        )
        && ruda_import_mir_append_elements(
            bytes, &function->enum_payloads,
            ruda_import_mir_append_enum_payload
        )
        && ruda_import_mir_append_elements(
            bytes, &function->enum_constructions,
            ruda_import_mir_append_enum_construction
        )
        && ruda_import_mir_append_elements(
            bytes, &function->enum_arguments,
            ruda_import_mir_append_enum_argument
        )
        && ruda_import_mir_append_strings(bytes, function)
        && ruda_import_mir_append_elements(
            bytes, &function->arrays, ruda_import_mir_append_array
        )
        && ruda_import_mir_append_elements(
            bytes, &function->array_constructions,
            ruda_import_mir_append_array_construction
        )
        && ruda_import_mir_append_elements(
            bytes, &function->array_arguments,
            ruda_import_mir_append_array_argument
        )
        && ruda_import_mir_append_elements(
            bytes, &function->interfaces,
            ruda_import_mir_append_interface
        )
        && ruda_import_mir_append_elements(
            bytes, &function->interface_constructions,
            ruda_import_mir_append_interface_construction
        )
        && ruda_import_mir_append_elements(
            bytes, &function->interface_methods,
            ruda_import_mir_append_symbol
        );
}

#undef RUDA_IMPORT_MIR_FIELD

bool ruda_checker_import_mir_bytes(
    const RudaCheckResult *result,
    RudaAllocator allocator,
    RudaOwnedBytes *bytes
) {
    RudaVector buffer;
    size_t index = 0;
    bool accepted = false;
    memset(&buffer, 0, sizeof(buffer));
    if (result == NULL || !result->initialized || bytes == NULL) {
        return false;
    }
    memset(bytes, 0, sizeof(*bytes));
    if (!ruda_vector_init(&buffer, allocator, sizeof(unsigned char))
        || !ruda_import_mir_append(
            &buffer, &result->functions.length,
            sizeof(result->functions.length)
        )) {
        goto cleanup;
    }
    for (index = 0; index < result->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            result, index
        );
        if (function == NULL || !ruda_import_mir_append_function(
                &buffer, &function->mir
            )) {
            goto cleanup;
        }
    }
    accepted = ruda_owned_bytes_copy(
        bytes, allocator, buffer.data, buffer.length
    );
cleanup:
    ruda_vector_dispose(&buffer);
    return accepted;
}
