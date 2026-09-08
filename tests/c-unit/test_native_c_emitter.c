#include "test_native_c_emitter_support.h"

void mmay_test_native_scalar_f64(void);
void mmay_test_native_source_matrix(void);
void mmay_test_native_strings_panic(void);
void mmay_test_native_arrays(void);
void mmay_test_array_edit_engines(void);
void mmay_test_function_value_engines(void);
void mmay_test_native_c_emitter_array_access(void);
void mmay_test_native_c_emitter_array_storage(void);
void mmay_test_native_baseline(void);
void mmay_test_native_nominal_boundary(void);
void mmay_test_native_nominal_differential(void);
void mmay_test_native_import_differential(void);
void mmay_test_native_imported_arrays(void);
void mmay_test_native_imported_results(void);
void mmay_test_native_c_emitter_test_entry(void);

void mmay_test_native_c_emitter(void) {
    mmay_test_native_scalar_f64();
    mmay_test_native_source_matrix();
    mmay_test_native_strings_panic();
    mmay_test_native_arrays();
    mmay_test_function_value_engines();
    mmay_test_array_edit_engines();
    mmay_test_native_c_emitter_array_access();
    mmay_test_native_c_emitter_array_storage();
    mmay_test_native_baseline();
    mmay_test_native_nominal_boundary();
    mmay_test_native_nominal_differential();
    mmay_test_native_import_differential();
    mmay_test_native_imported_arrays();
    mmay_test_native_imported_results();
    mmay_test_native_c_emitter_test_entry();
}
