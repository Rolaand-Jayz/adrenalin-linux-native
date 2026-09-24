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
    ../interfaces/service_readiness_client.cpp
    ../interfaces/operation_result.cpp
    ../interfaces/service_readiness_client.h
    ../interfaces/service_readiness_contract.h
)
set(ADRENALIN_SETTINGS1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Settings1.xml")
set(ADRENALIN_SERVICE1_XML "${ADRENALIN_SOURCE_ROOT}/interfaces/org.adrenalinlinux.Session1.Service1.xml")
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_SETTINGS1_XML}"
    settings1_interface
)
qt_add_dbus_interface(ADRENALIN_SESSION_CONTRACT_SOURCES
    "${ADRENALIN_SERVICE1_XML}"
    service1_interface
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
)
target_include_directories(adrenalin-session-contract-mock PUBLIC "${ADRENALIN_SOURCE_ROOT}")
target_link_libraries(adrenalin-session-contract-mock PUBLIC Qt6::Core)
target_link_libraries(adrenalin-session-contract-mock PUBLIC adrenalin-session-contract)
adrenalin_session_enable_strict_warnings(adrenalin-session-contract-mock)

if(ADRENALIN_BUILD_SESSION_SERVICE)
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
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 0 ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 1 ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_HEADER)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 2 ADRENALIN_GENERATED_SERVICE1_ADAPTOR_CPP)
    list(GET ADRENALIN_SESSION_ADAPTOR_FILES 3 ADRENALIN_GENERATED_SERVICE1_ADAPTOR_HEADER)
    set_source_files_properties("${ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_CPP}"
        PROPERTIES HEADER_FILE_ONLY TRUE
    )
    add_library(adrenalin-session-core STATIC
        session_database.cpp
        session_database.h
        session_service.cpp
        session_service.h
        service1_property_notifications.cpp
        service1_property_notifications.h
        settings1_adaptor_impl.cpp
        "${ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_SETTINGS1_ADAPTOR_HEADER}"
        "${ADRENALIN_GENERATED_SERVICE1_ADAPTOR_CPP}"
        "${ADRENALIN_GENERATED_SERVICE1_ADAPTOR_HEADER}"
        "${CMAKE_CURRENT_BINARY_DIR}/moc_settings1_adaptor.cpp"
    )
    target_include_directories(adrenalin-session-core PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${ADRENALIN_SOURCE_ROOT}"
        "${ADRENALIN_GENERATED_INCLUDE_DIR}"
    )
    target_link_libraries(adrenalin-session-core PUBLIC
        adrenalin-session-contract
        Qt6::Core
        Qt6::DBus
        Qt6::Sql
    )
    adrenalin_session_enable_strict_warnings(adrenalin-session-core)

    qt_add_executable(adrenalin-sessiond main.cpp)
    target_include_directories(adrenalin-sessiond PRIVATE "${ADRENALIN_SOURCE_ROOT}")
    target_link_libraries(adrenalin-sessiond PRIVATE adrenalin-session-core)
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
    add_subdirectory(../tests/session "${CMAKE_CURRENT_BINARY_DIR}/tests-session")
endif()
