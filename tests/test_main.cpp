#include <iostream>

void run_value_tests();
void run_table_tests();
void run_engine_tests();
void run_lexer_tests();

int main() {
    std::cout << "Exdeus tests: running registered suites.\n";
    run_value_tests();
    run_table_tests();
    run_engine_tests();
    run_lexer_tests();
    return 0;
}
