cmake_minimum_required(VERSION 3.16)
project(InventoryApp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

add_library(inventory_core
    src/Product.cpp
    src/Supplier.cpp
    src/Transaction.cpp
    src/Persistence.cpp
    src/Inventory.cpp
)
target_include_directories(inventory_core PUBLIC include)

add_executable(inventory src/main.cpp)
target_link_libraries(inventory PRIVATE inventory_core)

# Test binary: plain assertions, no external framework, linked against the
# exact same inventory_core the CLI uses.
add_executable(inventory_tests tests/test_inventory.cpp)
target_link_libraries(inventory_tests PRIVATE inventory_core)

enable_testing()
add_test(NAME inventory_tests COMMAND inventory_tests)
