find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick Test)
find_package(Threads REQUIRED)
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
    adrenalin-session-core-testable
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

qt_add_executable(adrenalin-service-readiness-client-test
    service_readiness_client_test.cpp
)
target_include_directories(adrenalin-service-readiness-client-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-service-readiness-client-test PRIVATE
    adrenalin-session-contract
    Qt6::Core
    Qt6::DBus
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-service-readiness-client-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-service-readiness-client
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:adrenalin-service-readiness-client-test>
)
set_tests_properties(adrenalin-service-readiness-client PROPERTIES TIMEOUT 30)

qt_add_executable(adrenalin-hardware1-contract-test
    hardware1_contract_test.cpp
)
set_source_files_properties("${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Hardware1.xml"
    PROPERTIES QT_RESOURCE_ALIAS org.adrenalinlinux.Session1.Hardware1.xml
)
qt_add_resources(adrenalin-hardware1-contract-test hardware1_contract_schema
    PREFIX "/hardware1"
    FILES "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Hardware1.xml"
)
target_include_directories(adrenalin-hardware1-contract-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-hardware1-contract-test PRIVATE
    adrenalin-session-contract
    adrenalin-session-contract-mock
    Qt6::Core
    Qt6::DBus
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-hardware1-contract-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-hardware1-contract
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:adrenalin-hardware1-contract-test>
)
set_tests_properties(adrenalin-hardware1-contract PROPERTIES TIMEOUT 30)

qt_add_executable(adrenalin-hardware1-session-service-test
    hardware1_session_service_test.cpp
)
target_include_directories(adrenalin-hardware1-session-service-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
    "${ADRENALIN_SOURCE_ROOT}/sessiond"
)
target_link_libraries(adrenalin-hardware1-session-service-test PRIVATE
    adrenalin-session-contract
    adrenalin-session-core-testable
    Qt6::Core Qt6::DBus Qt6::Sql Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-hardware1-session-service-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-hardware1-session-service
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:adrenalin-hardware1-session-service-test>
)
set_tests_properties(adrenalin-hardware1-session-service PROPERTIES TIMEOUT 30)

# Fixture-only Telemetry1 ABI and private-bus contract checks. These sources
# are intentionally excluded from the production session contract/provider.
qt_add_executable(adrenalin-telemetry1-abi-test
    telemetry1_abi_test.cpp
    ../../interfaces/telemetry1_types.cpp
    ../../interfaces/telemetry1_fixture_mock.cpp
    ../../interfaces/telemetry1_ring.cpp
)
target_include_directories(adrenalin-telemetry1-abi-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-telemetry1-abi-test PRIVATE
    Qt6::Core
    Qt6::DBus
    Threads::Threads
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-telemetry1-abi-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-telemetry1-abi COMMAND adrenalin-telemetry1-abi-test)

set(ADRENALIN_TELEMETRY1_XML
    "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Telemetry1.xml")
set_source_files_properties("${ADRENALIN_TELEMETRY1_XML}"
    PROPERTIES QT_RESOURCE_ALIAS org.adrenalinlinux.Session1.Telemetry1.xml
)
qt_add_executable(adrenalin-telemetry1-contract-test
    telemetry1_contract_test.cpp
    ../../interfaces/telemetry1_types.cpp
    ../../interfaces/telemetry1_fixture_mock.cpp
    ../../interfaces/telemetry1_ring.cpp
)
qt_add_resources(adrenalin-telemetry1-contract-test telemetry1_contract_schema
    PREFIX "/telemetry"
    FILES "${ADRENALIN_TELEMETRY1_XML}"
)
target_include_directories(adrenalin-telemetry1-contract-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-telemetry1-contract-test PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-telemetry1-contract-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-telemetry1-contract
    COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- $<TARGET_FILE:adrenalin-telemetry1-contract-test>
)
set_tests_properties(adrenalin-telemetry1-contract PROPERTIES TIMEOUT 30)

# The inventory source uses real libdrm enumeration in production; this target
# enables only its validation seam so malformed records can be tested without
# relying on host GPU visibility.
add_library(adrenalin-linux-drm-inventory-source-testable STATIC
    ../../interfaces/linux_drm_inventory_source.cpp
    ../../interfaces/linux_drm_inventory_source.h
)
target_compile_definitions(adrenalin-linux-drm-inventory-source-testable PUBLIC
    ADRENALIN_DRM_INVENTORY_SOURCE_TESTING
)
target_include_directories(adrenalin-linux-drm-inventory-source-testable PUBLIC
    "${ADRENALIN_SOURCE_ROOT}/interfaces"
)
target_link_libraries(adrenalin-linux-drm-inventory-source-testable PUBLIC
    Qt6::Core
    PkgConfig::LIBDRM
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-drm-inventory-source-testable PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

qt_add_executable(adrenalin-linux-drm-inventory-source-test
    linux_drm_inventory_source_test.cpp
)
target_include_directories(adrenalin-linux-drm-inventory-source-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-linux-drm-inventory-source-test PRIVATE
    adrenalin-linux-drm-inventory-source-testable
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-drm-inventory-source-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-linux-drm-inventory-source
    COMMAND $<TARGET_FILE:adrenalin-linux-drm-inventory-source-test>
)


qt_add_executable(adrenalin-pci-gpu-subject-identity-test
    pci_gpu_subject_identity_test.cpp
    ../../interfaces/pci_gpu_subject_identity.cpp
    ../../interfaces/pci_gpu_subject_identity.h
)
target_include_directories(adrenalin-pci-gpu-subject-identity-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-pci-gpu-subject-identity-test PRIVATE
    Qt6::Core
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-pci-gpu-subject-identity-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-pci-gpu-subject-identity
    COMMAND $<TARGET_FILE:adrenalin-pci-gpu-subject-identity-test>
)

# Read-only display and CPU topology sources have validation seams so malformed
# provider evidence can be exercised without depending on this host's devices.
add_library(adrenalin-linux-drm-display-source-testable STATIC
    ../../interfaces/linux_drm_display_source.cpp
    ../../interfaces/linux_drm_display_source.h
    ../../interfaces/pci_gpu_subject_identity.cpp
    ../../interfaces/pci_gpu_subject_identity.h
)
target_compile_definitions(adrenalin-linux-drm-display-source-testable PUBLIC
    ADRENALIN_DRM_DISPLAY_SOURCE_TESTING
)
target_include_directories(adrenalin-linux-drm-display-source-testable PUBLIC
    "${ADRENALIN_SOURCE_ROOT}/interfaces"
)
target_link_libraries(adrenalin-linux-drm-display-source-testable PUBLIC
    Qt6::Core
    PkgConfig::LIBDRM
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-drm-display-source-testable PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

qt_add_executable(adrenalin-linux-drm-display-source-test
    linux_drm_display_source_test.cpp
)
target_include_directories(adrenalin-linux-drm-display-source-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-linux-drm-display-source-test PRIVATE
    adrenalin-linux-drm-display-source-testable
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-drm-display-source-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-linux-drm-display-source
    COMMAND $<TARGET_FILE:adrenalin-linux-drm-display-source-test>
)

add_library(adrenalin-linux-cpu-package-source-testable STATIC
    ../../interfaces/linux_cpu_package_source.cpp
    ../../interfaces/linux_cpu_package_source.h
)
target_compile_definitions(adrenalin-linux-cpu-package-source-testable PUBLIC
    ADRENALIN_CPU_PACKAGE_SOURCE_TESTING
)
target_include_directories(adrenalin-linux-cpu-package-source-testable PUBLIC
    "${ADRENALIN_SOURCE_ROOT}/interfaces"
)
target_link_libraries(adrenalin-linux-cpu-package-source-testable PUBLIC
    Qt6::Core
    PkgConfig::HWLOC
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-cpu-package-source-testable PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

qt_add_executable(adrenalin-linux-cpu-package-source-test
    linux_cpu_package_source_test.cpp
)
target_include_directories(adrenalin-linux-cpu-package-source-test PRIVATE
    "${ADRENALIN_SOURCE_ROOT}"
)
target_link_libraries(adrenalin-linux-cpu-package-source-test PRIVATE
    adrenalin-linux-cpu-package-source-testable
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-cpu-package-source-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-linux-cpu-package-source
    COMMAND $<TARGET_FILE:adrenalin-linux-cpu-package-source-test>
)

# The provider test seam accepts captured source-result values only in this
# test target. Production code always calls the real Linux providers.
add_library(adrenalin-linux-hardware1-inventory-provider-testable STATIC
    ../../interfaces/linux_hardware1_inventory_provider.cpp
    ../../interfaces/linux_hardware1_inventory_provider.h
)
target_compile_definitions(adrenalin-linux-hardware1-inventory-provider-testable PUBLIC
    ADRENALIN_HARDWARE1_INVENTORY_TESTING
)
target_include_directories(adrenalin-linux-hardware1-inventory-provider-testable PUBLIC
    "${ADRENALIN_SOURCE_ROOT}/interfaces"
)
target_link_libraries(adrenalin-linux-hardware1-inventory-provider-testable PUBLIC
    adrenalin-session-contract
    adrenalin-linux-drm-inventory-source-testable
    adrenalin-linux-drm-display-source-testable
    adrenalin-linux-cpu-package-source-testable
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-hardware1-inventory-provider-testable PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

qt_add_executable(adrenalin-linux-hardware1-inventory-provider-test
    linux_hardware1_inventory_provider_test.cpp
)
target_link_libraries(adrenalin-linux-hardware1-inventory-provider-test PRIVATE
    adrenalin-linux-hardware1-inventory-provider-testable
    Qt6::Test
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(adrenalin-linux-hardware1-inventory-provider-test PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()
add_test(NAME adrenalin-linux-hardware1-inventory-provider
    COMMAND $<TARGET_FILE:adrenalin-linux-hardware1-inventory-provider-test>
)
