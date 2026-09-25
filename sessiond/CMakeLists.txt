find_package(Qt6 6.8 REQUIRED COMPONENTS Core DBus)
set(CMAKE_AUTOMOC ON)
get_filename_component(ADRENALIN_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(adrenalin_session_enable_strict_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_library(adrenalin-session-contract STATIC
    ../interfaces/settings1_client.cpp
    ../interfaces/notifications1_client.cpp
    ../interfaces/profiles1_client.cpp
    ../interfaces/service_readiness_client.cpp
    ../interfaces/hardware1_client.cpp
    ../interfaces/display1_client.cpp
    ../interfaces/operation_result.cpp
    ../interfaces/hardware1_contract_types.cpp
    ../interfaces/display1_contract_types.cpp
    ../interfaces/notifications1_contract_types.cpp
    ../interfaces/profiles1_contract_types.cpp
    ../interfaces/hotkeys1_contract_types.cpp
    ../interfaces/hardware1_registry.cpp
    ../interfaces/hardware1_contract.h
    ../interfaces/hardware1_contract_types.h
    ../interfaces/display1_contract.h
    ../interfaces/display1_contract_types.h
    ../interfaces/notifications1_contract_types.h
    ../interfaces/notifications1_contract.h
    ../interfaces/profiles1_contract.h
    ../interfaces/profiles1_contract_types.h
    ../interfaces/hotkeys1_contract_types.h
    ../interfaces/hotkeys1_contract.h
    ../interfaces/hardware1_registry.h
    ../interfaces/service_readiness_client.h
    ../interfaces/hardware1_client.h
    ../interfaces/display1_client.h
    ../interfaces/notifications1_client.h
    ../interfaces/profiles1_client.h
    ../interfaces/service_readiness_contract.h
)
set(ADRENALIN_SETTINGS1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Settings1.xml")
set(ADRENALIN_SERVICE1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Service1.xml")
set(ADRENALIN_HARDWARE1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Hardware1.xml")
set(ADRENALIN_DISPLAY1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Display1.xml")
set(ADRENALIN_NOTIFICATIONS1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Notifications1.xml")
set(ADRENALIN_PROFILES1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Profiles1.xml")
set(ADRENALIN_HOTKEYS1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Hotkeys1.xml")
set_source_files_properties("${ADRENALIN_HARDWARE1_XML}"
    PROPERTIES INCLUDE "${ADRENALIN_SOURCE_ROOT}/interfaces/hardware1_contract_types.h"
)
set_source_files_properties("${ADRENALIN_DISPLAY1_XML}"
    PROPERTIES INCLUDE "${ADRENALIN_SOURCE_ROOT}/interfaces/display1_contract_types.h"
)
set_source_files_properties("${ADRENALIN_NOTIFICATIONS1_XML}"
    PROPERTIES INCLUDE "${ADRENALIN_SOURCE_ROOT}/interfaces/notifications1_contract_types.h"
)
set_source_files_properties("${ADRENALIN_PROFILES1_XML}"
    PROPERTIES INCLUDE "${ADRENALIN_SOURCE_ROOT}/interfaces/profiles1_contract_types.h"
)
set_source_files_properties("${ADRENALIN_HOTKEYS1_XML}"
    PROPERTIES INCLUDE "${ADRENALIN_SOURCE_ROOT}/interfaces/hotkeys1_contract_types.h"
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_SETTINGS1_XML}"
    settings1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_SERVICE1_XML}"
    service1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_HARDWARE1_XML}"
    hardware1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_DISPLAY1_XML}"
    display1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_NOTIFICATIONS1_XML}"
    notifications1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_PROFILES1_XML}"
    profiles1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_HOTKEYS1_XML}"
    hotkeys1_interface
)
target_sources(adrenalin-session-contract PRIVATE ${ADRENALIN_SESSION_CONTRACT_SOURCES})
target_include_directories(adrenalin-session-contract PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${ADRENALIN_SOURCE_ROOT}"
    "${ADRENALIN_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(adrenalin-session-contract PUBLIC Qt6::Core Qt6::DBus)
adrenalin_session_enable_strict_warnings(adrenalin-session-contract)

add_library(adrenalin-session-contract-mock STATIC
    ../interfaces/settings1_mock.cpp
    ../interfaces/notifications1_mock.cpp
    ../interfaces/notifications1_mock.h
    ../interfaces/profiles1_mock.cpp
    ../interfaces/profiles1_mock.h
    ../interfaces/hardware1_mock.cpp
    ../interfaces/hardware1_mock.h
    ../interfaces/display1_mock.cpp
    ../interfaces/display1_mock.h
    ../interfaces/hotkeys1_mock.cpp
    ../interfaces/hotkeys1_mock.h
)
target_include_directories(adrenalin-session-contract-mock PUBLIC "${ADRENALIN_SOURCE_ROOT}")
target_link_libraries(adrenalin-session-contract-mock PUBLIC Qt6::Core)
target_link_libraries(adrenalin-session-contract-mock PUBLIC adrenalin-session-contract)
adrenalin_session_enable_strict_warnings(adrenalin-session-contract-mock)

if(ADRENALIN_BUILD_SESSION_SERVICE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
    pkg_check_modules(HWLOC REQUIRED IMPORTED_TARGET hwloc)
    pkg_check_modules(LIBUDEV REQUIRED IMPORTED_TARGET libudev)
    set(ADRENALIN_HARDWARE_INVENTORY_SOURCES
        ../interfaces/linux_drm_inventory_source.cpp
        ../interfaces/linux_drm_inventory_source.h
        ../interfaces/pci_gpu_subject_identity.cpp
        ../interfaces/pci_gpu_subject_identity.h
        ../interfaces/linux_drm_display_source.cpp
        ../interfaces/linux_drm_display_source.h
        ../interfaces/linux_cpu_package_source.cpp
        ../interfaces/linux_cpu_package_source.h
        ../interfaces/linux_hardware1_inventory_provider.cpp
        ../interfaces/linux_hardware1_inventory_provider.h
    )
    add_library(adrenalin-linux-hardware-inventory-sources STATIC
        ${ADRENALIN_HARDWARE_INVENTORY_SOURCES}
    )
    target_include_directories(adrenalin-linux-hardware-inventory-sources PUBLIC
        "${ADRENALIN_SOURCE_ROOT}/interfaces"
    )
    target_link_libraries(adrenalin-linux-hardware-inventory-sources PUBLIC
        Qt6::Core
        PkgConfig::LIBDRM
        PkgConfig::HWLOC
        adrenalin-session-contract
    )
    adrenalin_session_enable_strict_warnings(adrenalin-linux-hardware-inventory-sources)

    find_package(Qt6 6.8 REQUIRED COMPONENTS Sql)
    set(ADRENALIN_SESSION_ADAPTOR_FILES)
    qt_add_dbus_adaptor(ADRENALIN_SESSION_ADAPTOR_FILES
        "${ADRENALIN_SETTINGS1_XML}"
        "sessiond/session_service.h"
        ""
        settings1_adaptor
        Settings1Adaptor
    )
    qt_add_dbus_adaptor(ADRENALIN_SESSION_ADAPTOR_FILES
        "${ADRENALIN_SERVICE1_XML}"
        "sessiond/session_service.h"
        ""
        service1_adaptor
        Service1Adaptor
    )
    qt_add_dbus_adaptor(ADRENALIN_SESSION_ADAPTOR_FILES
        "${ADRENALIN_HARDWARE1_XML}"
        "sessiond/session_service.h"
        ""
        hardware1_adaptor
        Hardware1Adaptor
    )
    qt_add_dbus_adaptor(ADRENALIN_SESSION_ADAPTOR_FILES
        "${ADRENALIN_NOTIFICATIONS1_XML}"
        "sessiond/session_service.h"
        ""
        notifications1_adaptor
        Notifications1Adaptor
    )
    qt_add_dbus_adaptor(ADRENALIN_SESSION_ADAPTOR_FILES
        "${ADRENALIN_DISPLAY1_XML}"
        "sessiond/session_service.h"
        ""
        display1_adaptor
        Display1Adaptor
    )
    qt_add_dbus_adaptor(ADRENALIN_SESSION_ADAPTOR_FILES
        "${ADRENALIN_PROFILES1_XML}"
        "sessiond/session_service.h"
        ""
        profiles1_adaptor
        Profiles1Adaptor
    )
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 0 ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 1 ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_HEADER)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 2 ADRENALIN_GENERATED_SERVICE1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 3 ADRENALIN_GENERATED_SERVICE1_ADAPTOR_HEADER)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 4 ADRENALIN_GENERATED_HARDWARE1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 5 ADRENALIN_GENERATED_HARDWARE1_ADAPTOR_HEADER)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 6 ADRENALIN_GENERATED_NOTIFICATIONS1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 7 ADRENALIN_GENERATED_NOTIFICATIONS1_ADAPTOR_HEADER)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 8 ADRENALIN_GENERATED_DISPLAY1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 9 ADRENALIN_GENERATED_DISPLAY1_ADAPTOR_HEADER)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 10 ADRENALIN_GENERATED_PROFILES1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 11 ADRENALIN_GENERATED_PROFILES1_ADAPTOR_HEADER)
    set_source_files_properties(
        "${ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_HARDWARE1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_NOTIFICATIONS1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_DISPLAY1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_PROFILES1_ADAPTOR_CPP}"
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
    set(ADRENALIN_SESSION_CORE_SOURCES
        session_database.cpp
        session_database.h
        session_service.cpp
        session_service.h
        service1_property_notifications.cpp
        service1_property_notifications.h
        settings1_adaptor_impl.cpp
        hardware1_adaptor_impl.cpp
        notifications1_adaptor_impl.cpp
        display1_adaptor_impl.cpp
        profiles1_adaptor_impl.cpp
        "${ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_HEADER}"
        "${ADRENALIN_GENERATED_SERVICE1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_SERVICE1_ADAPTOR_HEADER}"
        "${ADRENALIN_GENERATED_HARDWARE1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_HARDWARE1_ADAPTOR_HEADER}"
        "${ADRENALIN_GENERATED_NOTIFICATIONS1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_NOTIFICATIONS1_ADAPTOR_HEADER}"
        "${ADRENALIN_GENERATED_DISPLAY1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_DISPLAY1_ADAPTOR_HEADER}"
        "${ADRENALIN_GENERATED_PROFILES1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_PROFILES1_ADAPTOR_HEADER}"
        "${CMAKE_CURRENT_BINARY_DIR}/moc_settings1_adaptor.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/moc_hardware1_adaptor.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/moc_notifications1_adaptor.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/moc_display1_adaptor.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/moc_profiles1_adaptor.cpp"
    )
    add_library(adrenalin-session-core STATIC ${ADRENALIN_SESSION_CORE_SOURCES})
    target_include_directories(adrenalin-session-core PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${ADRENALIN_SOURCE_ROOT}"
        "${ADRENALIN_GENERATED_INCLUDE_DIR}"
    )
    target_link_libraries(adrenalin-session-core PUBLIC
        adrenalin-session-contract
        adrenalin-linux-hardware-inventory-sources
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
    )
    adrenalin_session_enable_strict_warnings(adrenalin-session-core)
    if(BUILD_TESTING)
        add_library(adrenalin-session-core-testable STATIC ${ADRENALIN_SESSION_CORE_SOURCES})
        target_compile_definitions(adrenalin-session-core-testable PRIVATE
            ADRENALIN_SESSION_HARDWARE1_TESTING
        )
        target_include_directories(adrenalin-session-core-testable PUBLIC
            "${CMAKE_CURRENT_BINARY_DIR}"
            "${ADRENALIN_SOURCE_ROOT}"
            "${ADRENALIN_GENERATED_INCLUDE_DIR}"
        )
        target_link_libraries(adrenalin-session-core-testable PUBLIC
            adrenalin-session-contract
            adrenalin-linux-hardware-inventory-sources-testable
            Qt6::Core Qt6::DBus Qt6::Sql
        )
        adrenalin_session_enable_strict_warnings(adrenalin-session-core-testable)
        target_compile_definitions(adrenalin-session-core-testable INTERFACE
            ADRENALIN_SESSION_HARDWARE1_TESTING
        )
    endif()

    qt_add_executable(adrenalin-sessiond
        main.cpp
        drm_udev_monitor.cpp
        drm_udev_monitor.h
    )
    target_include_directories(adrenalin-sessiond PRIVATE "${ADRENALIN_SOURCE_ROOT}")
    target_link_libraries(adrenalin-sessiond PRIVATE adrenalin-session-core PkgConfig::LIBUDEV)
    adrenalin_session_enable_strict_warnings(adrenalin-sessiond)

    include(GNUInstallDirs)
    find_package(PkgConfig REQUIRED)
    pkg_get_variable(ADRENALIN_SYSTEMD_USER_UNIT_DIR systemd systemduserunitdir)
    if(NOT IS_ABSOLUTE "${ADRENALIN_SYSTEMD_USER_UNIT_DIR}")
        message(FATAL_ERROR
            "The systemd pkg-config module did not provide an absolute user-unit install directory")
    endif()
    configure_file(
        org.adrenalinlinux.Session1.service.in
        "${CMAKE_CURRENT_BINARY_DIR}/${ADRENALIN_SESSION_BUS_NAME}.service"
        @ONLY
    )
    configure_file(
        adrenalin-sessiond.service.in
        "${CMAKE_CURRENT_BINARY_DIR}/adrenalin-sessiond.service"
        @ONLY
    )
    install(TARGETS adrenalin-sessiond RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${ADRENALIN_SESSION_BUS_NAME}.service"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/dbus-1/services")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/adrenalin-sessiond.service"
        DESTINATION "${ADRENALIN_SYSTEMD_USER_UNIT_DIR}")
endif()

if(BUILD_TESTING AND ADRENALIN_BUILD_SESSION_SERVICE)
    add_library(adrenalin-linux-hardware-inventory-sources-testable STATIC
        ${ADRENALIN_HARDWARE_INVENTORY_SOURCES}
    )
    target_compile_definitions(adrenalin-linux-hardware-inventory-sources-testable PUBLIC
        ADRENALIN_HARDWARE1_INVENTORY_TESTING
    )
    target_include_directories(adrenalin-linux-hardware-inventory-sources-testable PUBLIC
        "${ADRENALIN_SOURCE_ROOT}/interfaces"
    )
    target_link_libraries(adrenalin-linux-hardware-inventory-sources-testable PUBLIC
        Qt6::Core PkgConfig::LIBDRM PkgConfig::HWLOC adrenalin-session-contract
    )
    adrenalin_session_enable_strict_warnings(adrenalin-linux-hardware-inventory-sources-testable)

    add_subdirectory(../tests/session "${CMAKE_CURRENT_BINARY_DIR}/tests-session")
endif()
