#include "test_bytecode_support.h"

void mmay_test_bytecode_preparation(void);
void mmay_test_bytecode_imported_arrays(void);
void mmay_test_bytecode_imported_results(void);
void mmay_test_bytecode_layout_features(void);

void mmay_test_bytecode(void) {
    mmay_test_bytecode_metadata();
    mmay_test_bytecode_valid_and_disassembly();
    mmay_test_bytecode_codec();
    mmay_test_bytecode_f64_codec_and_verifier();
    mmay_test_bytecode_decode_rejections();
    mmay_test_bytecode_verifier_rejections();
    mmay_test_bytecode_integer_widths();
    mmay_test_bytecode_calls_and_program();
    mmay_test_bytecode_preparation();
    mmay_test_bytecode_string_pool();
    mmay_test_bytecode_string_verifier_rejections();
    mmay_test_bytecode_allocation_failures();
    mmay_test_bytecode_verifier_provenance();
    mmay_test_bytecode_nominal_verifier_matrix();
    mmay_test_bytecode_nominal_program_schemas();
    mmay_test_bytecode_imported_arrays();
    mmay_test_bytecode_imported_results();
    mmay_test_bytecode_layout_features();
}
