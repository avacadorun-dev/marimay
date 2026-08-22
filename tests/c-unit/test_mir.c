#include "test_mir_support.h"

void mmay_test_mir_arrays(MarimaySymbolId symbol);
void mmay_test_mir_scalar_core(MarimaySymbolId symbol);
void mmay_test_mir_verify(MarimaySymbolId symbol);
void mmay_test_mir_calls_arithmetic(MarimaySymbolId symbol);
void mmay_test_mir_records_identity(MarimaySymbolId symbol);
void mmay_test_mir_enums_projection(MarimaySymbolId symbol);
void mmay_test_mir_strings_f64(MarimaySymbolId symbol);
void mmay_test_mir_imported_arrays(void);
void mmay_test_mir_imported_results(void);

void mmay_test_mir(void) {
    MarimaySymbolId symbol;
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "example.test/core",
        "src/calc.my",
        "calc",
        0,
        &symbol
    ));
    mmay_test_mir_scalar_core(symbol);
    mmay_test_mir_verify(symbol);
    mmay_test_mir_calls_arithmetic(symbol);
    mmay_test_mir_records_identity(symbol);
    mmay_test_mir_enums_projection(symbol);
    mmay_test_mir_strings_f64(symbol);
    mmay_test_mir_arrays(symbol);
    mmay_test_mir_imported_arrays();
    mmay_test_mir_imported_results();
}
