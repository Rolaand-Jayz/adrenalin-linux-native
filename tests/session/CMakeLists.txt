find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick Test)
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
get_filename_component(ADRENALIN_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

qt_add_executable(adrenalin-session-contract-test
    session_contract_test.cpp
)
set_source_files_properties(PreferenceHarness.qml
    PROPERTIES QT_RESOURCE_ALIAS PreferenceHarness.qml
)
set_source_files_properties("${ADRENALIN_SOURCE_ROOT}/qml/preferences/ProductTelemetryConsent.qml"
    PROPERTIES QT_RESOURCE_ALIAS ProductTelemetryConsent.qml
)
qt_add_qml_module(adrenalin-session-contract-test
    URI Adrenalin.SessionTests
    VERSION 1.0
    QML_FILES
        PreferenceHarness.qml
        "${ADRENALIN_SOURCE_ROOT}/qml/preferences/ProductTelemetryConsent.qml"
)
target_include_directories(adrenalin-session-contract-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
    "${ADRENALIN_SOURCE_ROOT}/sessiond"
)
target_link_libraries(adrenalin-session-contract-test PRIVATE
    adrenalin-session-contract
    adrenalin-session-contract-mock
    adrenalin-session-core
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Sql
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-session-contract-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME adrenalin-session-contract
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:adrenalin-session-contract-test>
)
set_tests_properties(adrenalin-session-contract PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30
)
