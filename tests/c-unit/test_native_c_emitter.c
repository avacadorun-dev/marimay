#include "test_native_c_emitter_support.h"

void ruda_test_native_scalar_f64(void);
void ruda_test_native_source_matrix(void);
void ruda_test_native_strings_panic(void);
void ruda_test_native_arrays(void);
void ruda_test_native_baseline(void);
void ruda_test_native_nominal_boundary(void);
void ruda_test_native_nominal_differential(void);
void ruda_test_native_import_differential(void);
void ruda_test_native_imported_arrays(void);
void ruda_test_native_imported_results(void);

void ruda_test_native_c_emitter(void) {
    ruda_test_native_scalar_f64();
    ruda_test_native_source_matrix();
    ruda_test_native_strings_panic();
    ruda_test_native_arrays();
    ruda_test_native_baseline();
    ruda_test_native_nominal_boundary();
    ruda_test_native_nominal_differential();
    ruda_test_native_import_differential();
    ruda_test_native_imported_arrays();
    ruda_test_native_imported_results();
}
