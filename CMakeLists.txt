cmake_minimum_required(VERSION 3.16)
project(InventoryApp CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra>)

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

# ---------------------------------------------------------------------------
# Optional desktop GUI (Dear ImGui + GLFW + OpenGL3, with a native file
# picker for product photos via tinyfiledialogs).
#
# Built on the exact same inventory_core the CLI uses, so it reads and
# writes the same three data files and either interface can be used
# interchangeably on one inventory folder. On by default, but can be
# turned off with -DINVENTORY_BUILD_GUI=OFF, and auto-disables itself
# with a warning if OpenGL isn't found, so a missing OpenGL install never
# breaks the CLI/tests build.
# ---------------------------------------------------------------------------
option(INVENTORY_BUILD_GUI "Build the optional Dear ImGui desktop interface" ON)

if(INVENTORY_BUILD_GUI)
    find_package(OpenGL)
    if(NOT OPENGL_FOUND)
        message(WARNING "OpenGL not found, skipping inventory_gui. "
                         "Configure with -DINVENTORY_BUILD_GUI=OFF to silence this warning.")
        set(INVENTORY_BUILD_GUI OFF)
    endif()
endif()

if(INVENTORY_BUILD_GUI)
    include(FetchContent)

    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.9
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    # stb_image.h, for loading a product's photo into a GUI thumbnail.
    # stb has no tagged releases, so this tracks its master branch.
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(stb)

    # tinyfiledialogs: a single C file that opens the OS's native file
    # picker (AppleScript-driven on macOS), used for "browse for a photo"
    # instead of the user typing a raw path. No tagged releases either.
    FetchContent_Declare(
        tinyfiledialogs
        GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(tinyfiledialogs)

    add_library(tinyfiledialogs_lib STATIC ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c)
    target_include_directories(tinyfiledialogs_lib PUBLIC ${tinyfiledialogs_SOURCE_DIR})

    add_library(imgui_backend STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_backend PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_backend PUBLIC glfw OpenGL::GL)

    add_executable(inventory_gui src/gui/GuiMain.cpp)
    target_include_directories(inventory_gui PRIVATE ${stb_SOURCE_DIR})
    target_link_libraries(inventory_gui PRIVATE inventory_core imgui_backend tinyfiledialogs_lib)

    if(APPLE)
        target_link_libraries(inventory_gui PRIVATE
            "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    endif()
endif()
