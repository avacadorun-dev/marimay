#include "test_imported_arrays_support.h"

static bool ruda_test_bytes_equal(
    const RudaOwnedBytes *bytes,
    const char *text
) {
    size_t length = strlen(text);
    return bytes != NULL && bytes->length == length
        && memcmp(bytes->data, text, length) == 0;
}

static void ruda_test_checker_imported_array_metadata(
    const RudaTestImportedArrays *test
) {
    const RudaCheckedRecord *box = NULL;
    size_t index = 0;
    bool found_box_i64 = false;
    bool found_array_box_i64 = false;
    bool found_nested_cell_array = false;
    for (index = 0; index < test->checked.records.length; index += 1) {
        const RudaCheckedRecord *record = ruda_checked_record(
            &test->checked, index
        );
        if (record != NULL && ruda_test_bytes_equal(&record->name, "Box")) {
            box = record;
            break;
        }
    }
    RUDA_EXPECT(box != NULL);
    if (box != NULL) {
        RUDA_EXPECT(box->is_public);
        RUDA_EXPECT(box->type_parameters.length == 1);
    }
    for (index = 0; index < test->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &test->checked, index
        );
        size_t record_index = 0;
        size_t array_index = 0;
        if (function == NULL) continue;
        for (record_index = 0; record_index < function->mir.records.length;
            record_index += 1) {
            const RudaMirRecord *record = ruda_vector_at_const(
                &function->mir.records, record_index
            );
            const RudaTypeDescriptor *descriptor = record != NULL
                ? ruda_type_descriptor(&test->checked.types, record->type)
                : NULL;
            if (box != NULL && descriptor != NULL
                && ruda_symbol_id_equal(descriptor->symbol, box->symbol)
                && descriptor->argument_count == 1
                && descriptor->arguments[0] == RUDA_TYPE_I64) {
                const RudaMirRecordField *field = ruda_vector_at_const(
                    &function->mir.record_fields, record->field_start
                );
                found_box_i64 = field != NULL
                    && record->field_count == 1
                    && field->type == RUDA_TYPE_I64;
            }
        }
        for (array_index = 0; array_index < function->mir.arrays.length;
            array_index += 1) {
            const RudaMirArray *array = ruda_vector_at_const(
                &function->mir.arrays, array_index
            );
            const RudaTypeDescriptor *element = array != NULL
                ? ruda_type_descriptor(
                    &test->checked.types, array->element_type
                )
                : NULL;
            if (box != NULL && element != NULL
                && ruda_symbol_id_equal(element->symbol, box->symbol)
                && element->argument_count == 1
                && element->arguments[0] == RUDA_TYPE_I64) {
                found_array_box_i64 = true;
            }
            if (element != NULL && element->kind == RUDA_TYPE_KIND_ARRAY) {
                RudaTypeId nested_element = RUDA_TYPE_INVALID;
                if (ruda_type_array_element(
                        &test->checked.types,
                        array->element_type,
                        &nested_element
                    ) && ruda_type_kind(
                        &test->checked.types, nested_element
                    ) == RUDA_TYPE_KIND_NOMINAL) {
                    found_nested_cell_array = true;
                }
            }
        }
    }
    RUDA_EXPECT(found_box_i64);
    RUDA_EXPECT(found_array_box_i64);
    RUDA_EXPECT(found_nested_cell_array);
}

static void ruda_test_checker_imported_array_allocations(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaTestImportedArrays test;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        if (ruda_test_imported_arrays_init(
                &test, ruda_fail_allocator(&state)
            )) {
            reached_success = true;
            ruda_test_imported_arrays_dispose(&test);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_checker_imported_arrays(void) {
    RudaTestImportedArrays test;
    RUDA_EXPECT(ruda_test_imported_arrays_init(
        &test, ruda_system_allocator()
    ));
    if (test.initialized) {
        RUDA_EXPECT(test.checked.diagnostics.length == 0);
        ruda_test_checker_imported_array_metadata(&test);
        ruda_test_imported_arrays_dispose(&test);
    }
    ruda_test_checker_imported_array_allocations();
}
