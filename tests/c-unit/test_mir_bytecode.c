#include "test_mir_bytecode_support.h"

void mmay_test_mir_program_preparation(void);
void mmay_test_mir_bytecode_model(void);
void mmay_test_mir_bytecode_programs(void);

void mmay_test_mir_bytecode(void) {
    mmay_test_mir_bytecode_model();
    mmay_test_mir_bytecode_programs();
    mmay_test_mir_program_preparation();
}
