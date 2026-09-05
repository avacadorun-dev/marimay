#include "test_formatter_cases.h"

void mmay_test_formatter(void) {
    mmay_test_formatter_round_trip();
    mmay_test_formatter_type_closer_regression();
    mmay_test_formatter_fixture_file();
    mmay_test_formatter_long_list();
    mmay_test_formatter_example_corpus();
    mmay_test_formatter_trivia_property();
    mmay_test_formatter_rejection_limits_and_allocation();
    mmay_test_formatter_rejects_invalid_types();
    mmay_test_formatter_array_type_spelling();
    mmay_test_formatter_boolean_groups();
}
