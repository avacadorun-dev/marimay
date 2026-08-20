#include "test_parser_cases.h"

void ruda_test_parser(void) {
    ruda_test_parser_snapshot();
    ruda_test_parser_empty_source();
    ruda_test_parser_examples();
    ruda_test_parser_all_productions();
    ruda_test_parser_recovery();
    ruda_test_parser_delimiter_recovery();
    ruda_test_parser_declarations();
    ruda_test_parser_declaration_grammar();
    ruda_test_parser_keyword_boundaries();
    ruda_test_parser_declaration_recovery();
    ruda_test_parser_pratt_expressions();
    ruda_test_parser_expression_grammar();
    ruda_test_parser_expression_recovery();
    ruda_test_parser_pattern_grammar();
    ruda_test_parser_record_field_grammar();
    ruda_test_parser_exact_delimited_grammar();
    ruda_test_parser_statement_forms();
    ruda_test_parser_statement_recovery();
    ruda_test_parser_type_grammar();
    ruda_test_parser_nested_generic_boundaries();
    ruda_test_parser_incremental_reuse();
    ruda_test_parser_limits_and_allocation();
    ruda_test_parser_all_single_bytes();
}
