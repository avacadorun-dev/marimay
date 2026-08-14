#include "test_bytecode_vm_support.h"

void ruda_test_bytecode_vm(void) {
    ruda_test_vm_all_opcodes_and_limits();
    ruda_test_vm_checked_arithmetic();
    ruda_test_vm_scalar_results();
    ruda_test_vm_bitwise_and_shifts();
    ruda_test_vm_f64();
    ruda_test_vm_strings_and_panic();
    ruda_test_vm_direct_calls();
    ruda_test_vm_managed_arrays();
    ruda_test_vm_array_feature_call_metadata();
    ruda_test_vm_checked_index_cfg();
    ruda_test_vm_array_rhs_ordering();
    ruda_test_vm_managed_array_calls();
    ruda_test_vm_managed_array_provenance();
    ruda_test_vm_managed_array_panic_frames();
    ruda_test_vm_rejection_round_trip_and_allocation();
}
