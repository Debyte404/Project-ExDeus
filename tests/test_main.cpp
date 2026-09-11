#include <iostream>

void run_value_tests();
void run_table_tests();

int main() {
    std::cout << "Exdeus tests: running registered suites.\n";
    run_value_tests();
    run_table_tests();
    return 0;
}
