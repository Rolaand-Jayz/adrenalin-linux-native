cmake_minimum_required(VERSION 3.24)

if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
    message(FATAL_ERROR "The canonical build generator is Ninja. Configure with -G Ninja.")
endif()

project(AdrenalinLinux VERSION 0.1.0 LANGUAGES CXX)

option(ADRENALIN_BUILD_GUI "Build the native Qt/QML desktop application" ON)
option(ADRENALIN_BUILD_SESSION_SERVICE "Build the per-user session service" ON)
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
set(ADRENALIN_SESSION_BUS_NAME "org.adrenalinlinux.Session1")
set(ADRENALIN_SESSION_OBJECT_PATH "/org/adrenalinlinux/Session")
set(ADRENALIN_SETTINGS_INTERFACE "org.adrenalinlinux.Session1.Settings1")
set(ADRENALIN_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

include(GNUInstallDirs)

function(adrenalin_quote_exec_argument output syntax argument)
    string(REPLACE "\\" "\\\\" escaped "${argument}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    if(syntax STREQUAL "desktop")
        string(REPLACE "`" "\\`" escaped "${escaped}")
        string(REPLACE "$" "\\$" escaped "${escaped}")
        string(REPLACE "%" "%%" escaped "${escaped}")
    elseif(syntax STREQUAL "systemd")
        string(REPLACE "$" "$$" escaped "${escaped}")
        string(REPLACE "%" "%%" escaped "${escaped}")
    else()
        message(FATAL_ERROR "Unsupported executable path syntax: ${syntax}")
    endif()
    set("${output}" "\"${escaped}\"" PARENT_SCOPE)
endfunction()

function(adrenalin_path_contains_ascii_whitespace output path)
    set(contains_whitespace FALSE)
    foreach(code RANGE 9 13)
        string(ASCII "${code}" whitespace_character)
        string(FIND "${path}" "${whitespace_character}" found_at)
        if(NOT found_at EQUAL -1)
            set(contains_whitespace TRUE)
        endif()
    endforeach()
    string(FIND "${path}" " " found_at)
    if(NOT found_at EQUAL -1)
        set(contains_whitespace TRUE)
    endif()
    set("${output}" "${contains_whitespace}" PARENT_SCOPE)
endfunction()

adrenalin_quote_exec_argument(ADRENALIN_DESKTOP_EXEC desktop
    "${CMAKE_INSTALL_FULL_BINDIR}/adrenalin-shell")
if(ADRENALIN_BUILD_GUI AND CMAKE_INSTALL_FULL_BINDIR MATCHES "[=\r\n]")
    message(FATAL_ERROR
        "The desktop executable path cannot contain '=' or line breaks; "
        "choose a different logical install prefix or binary directory")
endif()
set(ADRENALIN_DBUS_SESSION_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/adrenalin-sessiond")
adrenalin_path_contains_ascii_whitespace(
    ADRENALIN_DBUS_SESSION_EXEC_HAS_WHITESPACE "${ADRENALIN_DBUS_SESSION_EXEC}")
if(ADRENALIN_BUILD_SESSION_SERVICE AND ADRENALIN_DBUS_SESSION_EXEC_HAS_WHITESPACE)
    message(FATAL_ERROR
        "The D-Bus activation executable path must not contain whitespace; "
        "choose a logical install prefix and binary directory without whitespace")
endif()
adrenalin_quote_exec_argument(ADRENALIN_SYSTEMD_SESSION_EXEC systemd
    "${CMAKE_INSTALL_FULL_BINDIR}/adrenalin-sessiond")

configure_file(
    cmake/application_config.h.in
    "${ADRENALIN_GENERATED_INCLUDE_DIR}/application_config.h"
    @ONLY
)
configure_file(
    cmake/session_identity.h.in
    "${ADRENALIN_GENERATED_INCLUDE_DIR}/session_identity.h"
    @ONLY
)
configure_file(
    packaging/application.desktop.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/${ADRENALIN_APP_ID}.desktop"
    @ONLY
)

if(ADRENALIN_BUILD_GUI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2)
    qt_standard_project_setup(REQUIRES 6.8)
endif()

include(CTest)

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

if(ADRENALIN_BUILD_GUI OR ADRENALIN_BUILD_SESSION_SERVICE)
    add_subdirectory(sessiond)
endif()

if(ADRENALIN_BUILD_GUI)
    set_source_files_properties(qml/preferences/ProductTelemetryConsent.qml
        PROPERTIES QT_RESOURCE_ALIAS ProductTelemetryConsent.qml
    )
    qt_add_executable(adrenalin-shell
        src/main.cpp
    )

    qt_add_qml_module(adrenalin-shell
        URI Adrenalin.Shell
        VERSION 1.0
        QML_FILES
            qml/Main.qml
            qml/preferences/ProductTelemetryConsent.qml
    )

    target_link_libraries(adrenalin-shell
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
            adrenalin-session-contract
    )
    target_include_directories(adrenalin-shell PRIVATE "${ADRENALIN_GENERATED_INCLUDE_DIR}")
    adrenalin_enable_strict_warnings(adrenalin-shell)

    install(TARGETS adrenalin-shell
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/${ADRENALIN_APP_ID}.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
endif()

if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)

    add_executable(adrenalin-identity-test
        tests/identity_test.cpp
    )
    target_link_libraries(adrenalin-identity-test PRIVATE Catch2::Catch2WithMain)
    target_include_directories(adrenalin-identity-test PRIVATE "${ADRENALIN_GENERATED_INCLUDE_DIR}")
    adrenalin_enable_strict_warnings(adrenalin-identity-test)
    add_test(NAME adrenalin-identity COMMAND adrenalin-identity-test)

    if(ADRENALIN_BUILD_GUI)
        find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
        find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
        find_program(XVFB_RUN_EXECUTABLE xvfb-run REQUIRED)
        add_executable(adrenalin-shell-smoke-test
            tests/shell_smoke_test.cpp
        )
        target_link_libraries(adrenalin-shell-smoke-test PRIVATE Qt6::Test Qt6::Gui)
        adrenalin_enable_strict_warnings(adrenalin-shell-smoke-test)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
           AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
            target_compile_options(adrenalin-shell-smoke-test PRIVATE
                -Wno-error=sfinae-incomplete
            )
        endif()
        add_test(NAME adrenalin-shell-smoke
            COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- "${XVFB_RUN_EXECUTABLE}" -a
                -s "-screen 0 2560x1600x24" $<TARGET_FILE:adrenalin-shell-smoke-test>)
        set_tests_properties(adrenalin-shell-smoke PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ADRENALIN_SHELL_EXECUTABLE=$<TARGET_FILE:adrenalin-shell>"
            TIMEOUT 30
        )
    endif()
endif()
