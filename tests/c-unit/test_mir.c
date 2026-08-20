#include "test_mir_support.h"

void ruda_test_mir_arrays(RudaSymbolId symbol);
void ruda_test_mir_scalar_core(RudaSymbolId symbol);
void ruda_test_mir_verify(RudaSymbolId symbol);
void ruda_test_mir_calls_arithmetic(RudaSymbolId symbol);
void ruda_test_mir_records_identity(RudaSymbolId symbol);
void ruda_test_mir_enums_projection(RudaSymbolId symbol);
void ruda_test_mir_strings_f64(RudaSymbolId symbol);
void ruda_test_mir_imported_arrays(void);
void ruda_test_mir_imported_results(void);

void ruda_test_mir(void) {
    RudaSymbolId symbol;
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "example.test/core",
        "src/calc.ruda",
        "calc",
        0,
        &symbol
    ));
    ruda_test_mir_scalar_core(symbol);
    ruda_test_mir_verify(symbol);
    ruda_test_mir_calls_arithmetic(symbol);
    ruda_test_mir_records_identity(symbol);
    ruda_test_mir_enums_projection(symbol);
    ruda_test_mir_strings_f64(symbol);
    ruda_test_mir_arrays(symbol);
    ruda_test_mir_imported_arrays();
    ruda_test_mir_imported_results();
}
