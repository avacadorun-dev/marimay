#include "test_support.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void mmay_test_allocator(void);
void mmay_test_ast(void);
void mmay_test_base(void);
void mmay_test_checked_integer(void);
void mmay_test_integer_range(void);
void mmay_test_integer_text(void);
void mmay_test_string_interpolation(void);
void mmay_test_bytecode(void);
void mmay_test_cp30_bytecode(void);
void mmay_test_cp30_checker(void);
void mmay_test_cp30_mir(void);
void mmay_test_cp31_program_arguments(void);
void mmay_test_cp31_program_argument_allocation_shard(
    size_t shard_index,
    size_t shard_count
);
void mmay_test_cp32_streams(void);
void mmay_test_cp33_text_bytes(void);
void mmay_test_cp34_fs(void);
void mmay_test_cp34_fs_pipeline(void);
void mmay_test_cp35_environment(void);
void mmay_test_cp35_context_pipeline(void);
void mmay_test_cp36_collections(void);
void mmay_test_cp37_cli_parser(void);
void mmay_test_cp38_json(void);
void mmay_test_bytecode_vm(void);
void mmay_test_checker(void);
void mmay_test_checker_arrays(void);
void mmay_test_checker_array_mutation(void);
void mmay_test_mir_array_mutation(void);
void mmay_test_checker_facts(void);
void mmay_test_checker_imports(void);
void mmay_test_checker_import_determinism(void);
void mmay_test_checker_generics(void);
void mmay_test_checker_generics_imports(void);
void mmay_test_checker_imported_results(void);
void mmay_test_checker_resolution(void);
void mmay_test_receiver_collection_names(void);
void mmay_test_receiver_collection_service(void);
void mmay_test_check_entry(void);
void mmay_test_cli(void);
void mmay_test_cli_release_metadata(void);
void mmay_test_cli_build(void);
void mmay_test_cli_build_profiles(void);
void mmay_test_cli_check(void);
void mmay_test_cli_heap(void);
void mmay_test_cli_run(void);
void mmay_test_cli_test(void);
void mmay_test_cli_cp14_imported_arrays(void);
void mmay_test_cli_cp15_imported_results(void);
void mmay_test_cli_cp16_receiver_names(void);
void mmay_test_cli_cp17_array_mutation(void);
void mmay_test_cli_cp18_array_spelling(void);
void mmay_test_cli_cp20_algorithm_corpus(void);
void mmay_test_compiler_service(void);
void mmay_test_compiler_service_array_types(void);
void mmay_test_compiler_service_incremental(void);
void mmay_test_compiler_service_queries(void);
void mmay_test_cp9_io_pipeline(void);
void mmay_test_cp11_test_pipeline(void);
void mmay_test_diagnostic(void);
void mmay_test_defer_cp7(void);
void mmay_test_directory(void);
void mmay_test_doc_examples(void);
void mmay_test_document(void);
void mmay_test_file(void);
void mmay_test_final_preview(void);
void mmay_test_formatter(void);
void mmay_test_hash_map(void);
void mmay_test_lexer(void);
void mmay_test_manifest(void);
void mmay_test_managed(void);
void mmay_test_managed_resource(void);
void mmay_test_managed_string(void);
void mmay_test_managed_array_access(void);
void mmay_test_managed_array_capacity(void);
void mmay_test_managed_array_nested(void);
void mmay_test_managed_array_storage(void);
void mmay_test_managed_record_view(void);
void mmay_test_managed_mutation(void);
void mmay_test_managed_map_set(void);
void mmay_test_program_arguments(void);
void mmay_test_number_format(void);
void mmay_test_mir(void);
void mmay_test_mir_range(void);
void mmay_test_mir_integer_range(void);
void mmay_test_mir_interface_execution(void);
void mmay_test_mir_interfaces(void);
void mmay_test_mir_bytecode(void);
void mmay_test_native_c_emitter(void);
void mmay_test_native_c_emitter_array_mutation(void);
void mmay_test_native_generics_differential(void);
void mmay_test_native_matrix(void);
void mmay_test_nominal_engine_allocations(void);
void mmay_test_numeric_cp6_differential(void);
void mmay_test_pre1_surface_differential(void);
void mmay_test_path_stream(void);
void mmay_test_parser(void);
void mmay_test_process(void);
void mmay_test_runtime_io(void);
void mmay_test_source_manager(void);
void mmay_test_types(void);
void mmay_test_unicode(void);
void mmay_test_utf8(void);

static bool mmay_test_parse_size(const char *text, size_t *value) {
    size_t parsed = 0;
    size_t index = 0;
    if (text[0] == '\0') return false;
    for (index = 0; text[index] != '\0'; index += 1) {
        size_t digit;
        if (text[index] < '0' || text[index] > '9') return false;
        digit = (size_t)(text[index] - '0');
        if (parsed > (SIZE_MAX - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    *value = parsed;
    return true;
}

static void mmay_test_run_suite_shard(size_t shard_index) {
    switch (shard_index) {
    case 0:
        mmay_test_allocator();
        mmay_test_ast();
        mmay_test_base();
        mmay_test_checked_integer();
        mmay_test_integer_range();
        mmay_test_integer_text();
        mmay_test_string_interpolation();
        mmay_test_diagnostic();
        mmay_test_doc_examples();
        mmay_test_document();
        mmay_test_formatter();
        mmay_test_hash_map();
        mmay_test_lexer();
        mmay_test_manifest();
        mmay_test_parser();
        mmay_test_source_manager();
        mmay_test_types();
        mmay_test_unicode();
        mmay_test_utf8();
        break;
    case 1:
        mmay_test_cp30_checker();
        mmay_test_checker();
        mmay_test_checker_arrays();
        mmay_test_checker_array_mutation();
        mmay_test_checker_facts();
        mmay_test_checker_imports();
        mmay_test_checker_import_determinism();
        mmay_test_checker_generics();
        mmay_test_checker_generics_imports();
        mmay_test_checker_imported_results();
        mmay_test_checker_resolution();
        mmay_test_receiver_collection_names();
        mmay_test_receiver_collection_service();
        mmay_test_check_entry();
        mmay_test_compiler_service();
        mmay_test_compiler_service_array_types();
        mmay_test_compiler_service_incremental();
        mmay_test_compiler_service_queries();
        break;
    case 2:
        mmay_test_bytecode();
        mmay_test_cp30_bytecode();
        mmay_test_cp33_text_bytes();
        mmay_test_bytecode_vm();
        break;
    case 3:
        mmay_test_numeric_cp6_differential();
        mmay_test_pre1_surface_differential();
        mmay_test_defer_cp7();
        mmay_test_cp30_mir();
        mmay_test_cp32_streams();
        mmay_test_cp34_fs();
        mmay_test_cp34_fs_pipeline();
        mmay_test_cp35_environment();
        mmay_test_cp35_context_pipeline();
        mmay_test_mir_array_mutation();
        mmay_test_cp9_io_pipeline();
        mmay_test_cp11_test_pipeline();
        mmay_test_directory();
        mmay_test_file();
        mmay_test_managed();
        mmay_test_managed_resource();
        mmay_test_managed_string();
        mmay_test_managed_array_access();
        mmay_test_managed_array_capacity();
        mmay_test_managed_array_nested();
        mmay_test_managed_array_storage();
        mmay_test_managed_record_view();
        mmay_test_managed_mutation();
        mmay_test_managed_map_set();
        mmay_test_program_arguments();
        mmay_test_number_format();
        mmay_test_mir();
        mmay_test_mir_range();
        mmay_test_mir_integer_range();
        mmay_test_mir_interface_execution();
        mmay_test_mir_interfaces();
        mmay_test_mir_bytecode();
        mmay_test_nominal_engine_allocations();
        mmay_test_path_stream();
        mmay_test_process();
        mmay_test_runtime_io();
        break;
    case 4:
        mmay_test_native_c_emitter();
        mmay_test_native_c_emitter_array_mutation();
        mmay_test_native_generics_differential();
        mmay_test_native_matrix();
        break;
    case 5:
        mmay_test_cp31_program_arguments();
        break;
    case 6:
        mmay_test_final_preview();
        break;
    case 7:
        mmay_test_cli();
        mmay_test_cli_release_metadata();
        break;
    case 8:
        mmay_test_cli_build();
        break;
    case 9:
        mmay_test_cli_build_profiles();
        mmay_test_cli_check();
        break;
    case 10:
        mmay_test_cli_heap();
        break;
    case 11:
        mmay_test_cli_run();
        break;
    case 12:
        mmay_test_cli_test();
        break;
    case 13:
        mmay_test_cli_cp14_imported_arrays();
        mmay_test_cli_cp15_imported_results();
        break;
    case 14:
        mmay_test_cli_cp16_receiver_names();
        mmay_test_cli_cp17_array_mutation();
        break;
    case 15:
        mmay_test_cli_cp18_array_spelling();
        mmay_test_cli_cp20_algorithm_corpus();
        break;
    }
}

int main(int argument_count, char **arguments) {
    if (argument_count == 5
        && strcmp(arguments[2], "--cp31-allocation-shard") == 0) {
        size_t shard_index = 0;
        size_t shard_count = 0;
        if (!mmay_test_parse_size(arguments[3], &shard_index)
            || !mmay_test_parse_size(arguments[4], &shard_count)
            || shard_count == 0 || shard_index >= shard_count) {
            fprintf(stderr, "invalid CP31 allocation shard\n");
            return 2;
        }
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp31_program_argument_allocation_shard(
            shard_index, shard_count
        );
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp31_allocation_shard: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp31_allocation_shard: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp33") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp33_text_bytes();
        mmay_test_runtime_io();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp33_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp33_unit: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp34") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp34_fs();
        mmay_test_cp34_fs_pipeline();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp34_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp34_unit: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp35") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp35_environment();
        mmay_test_cp35_context_pipeline();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp35_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp35_unit: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp36") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_managed_map_set();
        mmay_test_cp36_collections();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp36_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp36_unit: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp37") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp37_cli_parser();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp37_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp37_unit: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp38") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp38_json();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp38_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp38_unit: ok\n");
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[2], "--cp32") == 0) {
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_cp32_streams();
        mmay_test_process();
        mmay_test_runtime_io();
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_cp32_unit: %d failure(s)\n",
                mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_cp32_unit: ok\n");
        return 0;
    }
    if (argument_count == 4
        && strcmp(arguments[2], "--suite-shard") == 0) {
        size_t shard_index = 0;
        if (!mmay_test_parse_size(arguments[3], &shard_index)
            || shard_index >= MMAY_TEST_C_UNIT_SHARD_COUNT) {
            fprintf(stderr, "invalid C-unit suite shard\n");
            return 2;
        }
        mmay_test_set_toolchain_executable(arguments[1]);
        mmay_test_run_suite_shard(shard_index);
        if (mmay_test_failure_count() != 0) {
            fprintf(
                stderr, "mmay_c_unit_%zu: %d failure(s)\n",
                shard_index, mmay_test_failure_count()
            );
            return 1;
        }
        printf("mmay_c_unit_%zu: ok\n", shard_index);
        return 0;
    }
    if (argument_count != 2) {
        fprintf(stderr, "usage: mmay_c_unit <mmay-executable>\n");
        return 2;
    }
    mmay_test_set_toolchain_executable(arguments[1]);
    {
        size_t shard_index = 0;
        for (shard_index = 0;
            shard_index < MMAY_TEST_C_UNIT_SHARD_COUNT;
            shard_index += 1) {
            mmay_test_run_suite_shard(shard_index);
        }
    }
    if (mmay_test_failure_count() != 0) {
        fprintf(stderr, "mmay_c_unit: %d failure(s)\n", mmay_test_failure_count());
        return 1;
    }
    printf("mmay_c_unit: ok\n");
    return 0;
}
