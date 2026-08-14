#include "test_bytecode_support.h"

void ruda_test_bytecode(void) {
    ruda_test_bytecode_metadata();
    ruda_test_bytecode_valid_and_disassembly();
    ruda_test_bytecode_codec();
    ruda_test_bytecode_f64_codec_and_verifier();
    ruda_test_bytecode_decode_rejections();
    ruda_test_bytecode_verifier_rejections();
    ruda_test_bytecode_calls_and_program();
    ruda_test_bytecode_string_pool();
    ruda_test_bytecode_string_verifier_rejections();
    ruda_test_bytecode_allocation_failures();
}
