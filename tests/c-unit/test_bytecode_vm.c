#include "test_bytecode_vm_support.h"

void mmay_test_vm_managed_strings(void);

void mmay_test_bytecode_vm(void) {
    mmay_test_vm_all_opcodes_and_limits();
    mmay_test_vm_checked_arithmetic();
    mmay_test_vm_scalar_results();
    mmay_test_vm_bitwise_and_shifts();
    mmay_test_vm_f64();
    mmay_test_vm_strings_and_panic();
    mmay_test_vm_managed_strings();
    mmay_test_vm_direct_calls();
    mmay_test_vm_managed_arrays();
    mmay_test_vm_array_storage_layouts();
    mmay_test_vm_array_feature_call_metadata();
    mmay_test_vm_checked_index_cfg();
    mmay_test_vm_array_rhs_ordering();
    mmay_test_vm_array_mutation();
    mmay_test_vm_managed_array_calls();
    mmay_test_vm_managed_array_provenance();
    mmay_test_vm_managed_array_panic_frames();
    mmay_test_vm_interfaces();
    mmay_test_vm_interface_values();
    mmay_test_vm_nominal_values_and_verifier();
    mmay_test_vm_nominal_host_boundary();
    mmay_test_vm_nominal_identity_and_strings();
    mmay_test_vm_nominal_codec();
    mmay_test_vm_rejection_round_trip_and_allocation();
}
