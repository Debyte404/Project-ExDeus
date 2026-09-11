cmake_minimum_required(VERSION 3.20)

project(Exdeus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(exdeus_core
    src/core/value.cpp
    src/core/schema.cpp
    src/core/table.cpp
    src/core/catalog.cpp
    src/core/engine.cpp
    src/language/lexer.cpp
    src/language/parser.cpp
    src/io/snapshot_store.cpp
    src/io/csv_exporter.cpp
)

target_include_directories(exdeus_core PUBLIC include)

if (MSVC)
    target_compile_options(exdeus_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(exdeus_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(exdeus src/repl/main.cpp)
target_link_libraries(exdeus PRIVATE exdeus_core)

add_executable(exdeus_tests
    tests/test_main.cpp
    tests/value_tests.cpp
    tests/lexer_tests.cpp
    tests/parser_tests.cpp
    tests/table_tests.cpp
    tests/engine_tests.cpp
    tests/snapshot_tests.cpp
    tests/end_to_end_tests.cpp
)
target_link_libraries(exdeus_tests PRIVATE exdeus_core)

add_test(NAME exdeus_tests COMMAND exdeus_tests)
