#include "test_imported_arrays_support.h"

static bool mmay_test_bytes_equal(
    const MarimayOwnedBytes *bytes,
    const char *text
) {
    size_t length = strlen(text);
    return bytes != NULL && bytes->length == length
        && memcmp(bytes->data, text, length) == 0;
}

static void mmay_test_checker_imported_array_metadata(
    const MarimayTestImportedArrays *test
) {
    const MarimayCheckedRecord *box = NULL;
    size_t index = 0;
    bool found_box_i64 = false;
    bool found_array_box_i64 = false;
    bool found_nested_cell_array = false;
    for (index = 0; index < test->checked.records.length; index += 1) {
        const MarimayCheckedRecord *record = mmay_checked_record(
            &test->checked, index
        );
        if (record != NULL && mmay_test_bytes_equal(&record->name, "Box")) {
            box = record;
            break;
        }
    }
    MMAY_EXPECT(box != NULL);
    if (box != NULL) {
        MMAY_EXPECT(box->is_public);
        MMAY_EXPECT(box->type_parameters.length == 1);
    }
    for (index = 0; index < test->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &test->checked, index
        );
        size_t record_index = 0;
        size_t array_index = 0;
        if (function == NULL) continue;
        for (record_index = 0; record_index < function->mir.records.length;
            record_index += 1) {
            const MarimayMirRecord *record = mmay_vector_at_const(
                &function->mir.records, record_index
            );
            const MarimayTypeDescriptor *descriptor = record != NULL
                ? mmay_type_descriptor(&test->checked.types, record->type)
                : NULL;
            if (box != NULL && descriptor != NULL
                && mmay_symbol_id_equal(descriptor->symbol, box->symbol)
                && descriptor->argument_count == 1
                && descriptor->arguments[0] == MMAY_TYPE_I64) {
                const MarimayMirRecordField *field = mmay_vector_at_const(
                    &function->mir.record_fields, record->field_start
                );
                found_box_i64 = field != NULL
                    && record->field_count == 1
                    && field->type == MMAY_TYPE_I64;
            }
        }
        for (array_index = 0; array_index < function->mir.arrays.length;
            array_index += 1) {
            const MarimayMirArray *array = mmay_vector_at_const(
                &function->mir.arrays, array_index
            );
            const MarimayTypeDescriptor *element = array != NULL
                ? mmay_type_descriptor(
                    &test->checked.types, array->element_type
                )
                : NULL;
            if (box != NULL && element != NULL
                && mmay_symbol_id_equal(element->symbol, box->symbol)
                && element->argument_count == 1
                && element->arguments[0] == MMAY_TYPE_I64) {
                found_array_box_i64 = true;
            }
            if (element != NULL && element->kind == MMAY_TYPE_KIND_ARRAY) {
                MarimayTypeId nested_element = MMAY_TYPE_INVALID;
                if (mmay_type_array_element(
                        &test->checked.types,
                        array->element_type,
                        &nested_element
                    ) && mmay_type_kind(
                        &test->checked.types, nested_element
                    ) == MMAY_TYPE_KIND_NOMINAL) {
                    found_nested_cell_array = true;
                }
            }
        }
    }
    MMAY_EXPECT(found_box_i64);
    MMAY_EXPECT(found_array_box_i64);
    MMAY_EXPECT(found_nested_cell_array);
}

static void mmay_test_checker_imported_array_allocations(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayTestImportedArrays test;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (mmay_test_imported_arrays_init(
                &test, mmay_fail_allocator(&state)
            )) {
            reached_success = true;
            mmay_test_imported_arrays_dispose(&test);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_checker_imported_arrays(void) {
    MarimayTestImportedArrays test;
    MMAY_EXPECT(mmay_test_imported_arrays_init(
        &test, mmay_system_allocator()
    ));
    if (test.initialized) {
        MMAY_EXPECT(test.checked.diagnostics.length == 0);
        mmay_test_checker_imported_array_metadata(&test);
        mmay_test_imported_arrays_dispose(&test);
    }
    mmay_test_checker_imported_array_allocations();
}
