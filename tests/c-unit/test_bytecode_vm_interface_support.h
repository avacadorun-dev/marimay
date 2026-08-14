#ifndef RUDA_TEST_BYTECODE_VM_INTERFACE_SUPPORT_H
#define RUDA_TEST_BYTECODE_VM_INTERFACE_SUPPORT_H

#include "test_bytecode_vm_support.h"

bool ruda_test_vm_build_interface_method(
    RudaBytecodeFunction *function,
    RudaSymbolId record_symbol
);

#endif
