#include "test_native_c_emitter_support.h"

void ruda_test_native_scalar_f64(void);
void ruda_test_native_source_matrix(void);
void ruda_test_native_strings_panic(void);
void ruda_test_native_arrays(void);
void ruda_test_native_baseline(void);

void ruda_test_native_c_emitter(void) {
    ruda_test_native_scalar_f64();
    ruda_test_native_source_matrix();
    ruda_test_native_strings_panic();
    ruda_test_native_arrays();
    ruda_test_native_baseline();
}
