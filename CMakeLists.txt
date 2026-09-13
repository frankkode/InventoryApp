cmake_minimum_required(VERSION 3.14)
project(InventoryApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable stricter warnings so mistakes surface at compile time, not runtime.
add_compile_options(-Wall -Wextra)

# --- CLI application -------------------------------------------------------
# For now this only builds main.cpp. As the data/logic layers are added in
# later commits, they will move into a shared "inventory_lib" target that
# both the CLI and the test suite link against, so logic is never
# duplicated between interfaces.
add_executable(inventory src/main.cpp)
target_include_directories(inventory PRIVATE include)