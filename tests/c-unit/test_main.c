#include "test_support.h"

#include <stdio.h>

void ruda_test_allocator(void);
void ruda_test_ast(void);
void ruda_test_base(void);
void ruda_test_bytecode(void);
void ruda_test_bytecode_vm(void);
void ruda_test_checker(void);
void ruda_test_cli(void);
void ruda_test_diagnostic(void);
void ruda_test_directory(void);
void ruda_test_doc_examples(void);
void ruda_test_document(void);
void ruda_test_file(void);
void ruda_test_formatter(void);
void ruda_test_hash_map(void);
void ruda_test_lexer(void);
void ruda_test_manifest(void);
void ruda_test_managed(void);
void ruda_test_mir(void);
void ruda_test_mir_bytecode(void);
void ruda_test_native_c_emitter(void);
void ruda_test_path_stream(void);
void ruda_test_parser(void);
void ruda_test_process(void);
void ruda_test_source_manager(void);
void ruda_test_types(void);
void ruda_test_unicode(void);
void ruda_test_utf8(void);

int main(int argument_count, char **arguments) {
    if (argument_count != 2) {
        fprintf(stderr, "usage: ruda_c_unit <ruda-executable>\n");
        return 2;
    }
    ruda_test_set_toolchain_executable(arguments[1]);
    ruda_test_allocator();
    ruda_test_ast();
    ruda_test_base();
    ruda_test_bytecode();
    ruda_test_bytecode_vm();
    ruda_test_checker();
    ruda_test_diagnostic();
    ruda_test_directory();
    ruda_test_doc_examples();
    ruda_test_document();
    ruda_test_file();
    ruda_test_formatter();
    ruda_test_hash_map();
    ruda_test_lexer();
    ruda_test_manifest();
    ruda_test_managed();
    ruda_test_mir();
    ruda_test_mir_bytecode();
    ruda_test_native_c_emitter();
    ruda_test_path_stream();
    ruda_test_parser();
    ruda_test_process();
    ruda_test_unicode();
    ruda_test_utf8();
    ruda_test_source_manager();
    ruda_test_types();
    ruda_test_cli();
    if (ruda_test_failure_count() != 0) {
        fprintf(stderr, "ruda_c_unit: %d failure(s)\n", ruda_test_failure_count());
        return 1;
    }
    printf("ruda_c_unit: ok\n");
    return 0;
}
