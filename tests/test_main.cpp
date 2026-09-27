#include <iostream>

void run_value_tests();
void run_table_tests();
void run_engine_tests();
void run_lexer_tests();
void run_parser_tests();
void run_interpreter_tests();

int main() {
    std::cout << "Exdeus tests: running registered suites.\n";
    run_value_tests();
    run_table_tests();
    run_engine_tests();
    run_lexer_tests();
    run_parser_tests();
    run_interpreter_tests();
    return 0;
}
