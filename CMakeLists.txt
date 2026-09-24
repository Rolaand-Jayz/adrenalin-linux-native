cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
    message(FATAL_ERROR "The canonical build generator is Ninja. Configure with -G Ninja.")
endif()

project(AdrenalinLinux VERSION 0.1.0 LANGUAGES CXX)

option(ADRENALIN_BUILD_GUI "Build the native Qt/QML desktop application" ON)
option(ADRENALIN_ENABLE_SANITIZERS "Build project targets with ASan and UBSan" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ADRENALIN_APP_NAME "AMD Software")
set(ADRENALIN_APP_ORGANIZATION "Adrenalin Linux")
set(ADRENALIN_APP_ID "org.adrenalinlinux.App")

configure_file(
    cmake/application_config.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/application_config.h"
    @ONLY
)
configure_file(
    packaging/application.desktop.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/${ADRENALIN_APP_ID}.desktop"
    @ONLY
)

if(ADRENALIN_BUILD_GUI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.8)
endif()

function(adrenalin_enable_strict_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

if(ADRENALIN_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "ADRENALIN_ENABLE_SANITIZERS requires a GCC- or Clang-compatible compiler")
    endif()
endif()

if(ADRENALIN_BUILD_GUI)
    qt_add_executable(adrenalin-shell
        src/main.cpp
    )

    qt_add_qml_module(adrenalin-shell
        URI Adrenalin.Shell
        VERSION 1.0
        QML_FILES
            qml/Main.qml
    )

    target_link_libraries(adrenalin-shell
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
    )
    target_include_directories(adrenalin-shell PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    adrenalin_enable_strict_warnings(adrenalin-shell)

    include(GNUInstallDirs)
    install(TARGETS adrenalin-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/${ADRENALIN_APP_ID}.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)

    add_executable(adrenalin-identity-test
        tests/identity_test.cpp
    )
    target_link_libraries(adrenalin-identity-test PRIVATE Catch2::Catch2WithMain)
    target_include_directories(adrenalin-identity-test PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    adrenalin_enable_strict_warnings(adrenalin-identity-test)
    add_test(NAME adrenalin-identity COMMAND adrenalin-identity-test)

    if(ADRENALIN_BUILD_GUI)
        find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
        add_executable(adrenalin-shell-smoke-test
            tests/shell_smoke_test.cpp
        )
        target_link_libraries(adrenalin-shell-smoke-test PRIVATE Qt6::Test)
        adrenalin_enable_strict_warnings(adrenalin-shell-smoke-test)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(adrenalin-shell-smoke-test PRIVATE
                -Wno-error=sfinae-incomplete
            )
        endif()
        add_test(NAME adrenalin-shell-smoke COMMAND adrenalin-shell-smoke-test)
        set_tests_properties(adrenalin-shell-smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ADRENALIN_SHELL_EXECUTABLE=$<TARGET_FILE:adrenalin-shell>"
            TIMEOUT 15
        )
    endif()
endif()
