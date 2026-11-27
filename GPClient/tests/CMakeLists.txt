# Tests for the request helper that every Portal and Gateway request goes through.
# They need Qt6 Test and the openssl CLI (to generate throwaway certificates at
# build time). Without them the tests are skipped with a warning, so the normal
# build and install keep working.

find_program(OPENSSL_EXECUTABLE openssl)
find_package(Qt6 QUIET COMPONENTS Test)
if (NOT OPENSSL_EXECUTABLE OR NOT Qt6Test_FOUND)
    message(WARNING "openssl or Qt6 Test not found, the GPClient tests are not built")
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
