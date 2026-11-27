# GPClient tests. They need Qt6 Test; the request helper tests also need the
# openssl CLI (to generate throwaway certificates at build time). Without them
# the tests are skipped with a warning, so the normal build and install keep
# working.

find_package(Qt6 QUIET COMPONENTS Test)
if (NOT Qt6Test_FOUND)
    message(WARNING "Qt6 Test not found, the GPClient tests are not built")
    return()
endif()

# Tests for the connection presenter (what the window shows for each state).
add_executable(tst_connectionview
    tst_connectionview.cpp
    ../connectionview.cpp
    ../gpgateway.cpp
    ../resources.qrc
)
target_include_directories(tst_connectionview PRIVATE ${CMAKE_SOURCE_DIR}/GPClient)
target_link_libraries(tst_connectionview Qt6::Test Qt6::Core Qt6::Gui Qt6::Svg)
add_test(NAME tst_connectionview COMMAND tst_connectionview)
set_tests_properties(tst_connectionview PROPERTIES TIMEOUT 60)

# Tests for the About dialog text.
add_executable(tst_aboutinfo
    tst_aboutinfo.cpp
    ../aboutinfo.cpp
)
target_include_directories(tst_aboutinfo PRIVATE ${CMAKE_SOURCE_DIR}/GPClient)
target_link_libraries(tst_aboutinfo Qt6::Test Qt6::Core)
add_test(NAME tst_aboutinfo COMMAND tst_aboutinfo)
set_tests_properties(tst_aboutinfo PROPERTIES TIMEOUT 60)

# Tests for the Client OS choices of the Settings dialog.
add_executable(tst_clientos
    tst_clientos.cpp
    ../clientos.cpp
)
target_include_directories(tst_clientos PRIVATE ${CMAKE_SOURCE_DIR}/GPClient)
target_link_libraries(tst_clientos Qt6::Test Qt6::Core)
add_test(NAME tst_clientos COMMAND tst_clientos)
set_tests_properties(tst_clientos PROPERTIES TIMEOUT 60)

# Tests for the request helper that every Portal and Gateway request goes through.
find_program(OPENSSL_EXECUTABLE openssl)
if (NOT OPENSSL_EXECUTABLE)
    message(WARNING "openssl not found, the request helper tests are not built")
    return()
endif()

# Test-only certificates, generated in the build tree:
# - ca.pem / ca.key: a test CA
# - server.pem / server.key: a server certificate for 127.0.0.1, signed by the test CA
# - selfsigned.pem / selfsigned.key: a self-signed server certificate for 127.0.0.1
set(CERT_DIR "${CMAKE_CURRENT_BINARY_DIR}/certs")
set(CERT_OUTPUTS
    "${CERT_DIR}/ca.pem"
    "${CERT_DIR}/server.pem"
    "${CERT_DIR}/server.key"
    "${CERT_DIR}/selfsigned.pem"
    "${CERT_DIR}/selfsigned.key"
)
add_custom_command(
    OUTPUT ${CERT_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CERT_DIR}"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/gen-certs.sh" "${OPENSSL_EXECUTABLE}" "${CERT_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/gen-certs.sh"
    COMMENT "Generating test certificates"
    VERBATIM
)
add_custom_target(gpclient_test_certs DEPENDS ${CERT_OUTPUTS})

add_executable(tst_requesthelper
    tst_requesthelper.cpp
    ../gphelper.cpp
    ../gpgateway.cpp
    ../clientos.cpp
)

add_dependencies(tst_requesthelper
    gpclient_test_certs
    plog-${PROJECT_NAME}
)

target_include_directories(tst_requesthelper PRIVATE
    ${CMAKE_SOURCE_DIR}/GPClient
    ${plog_INCLUDE_DIR}
    ${QTKEYCHAIN_INCLUDE_DIRS}/qt6keychain
)

target_compile_definitions(tst_requesthelper PRIVATE
    TEST_CERT_DIR="${CERT_DIR}"
)

target_link_libraries(tst_requesthelper
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    ${QTKEYCHAIN_LIBRARIES}
)

add_test(NAME tst_requesthelper COMMAND tst_requesthelper)
set_tests_properties(tst_requesthelper PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60
)
