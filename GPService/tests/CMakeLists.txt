# GPService tests. They need Qt6 Test; without it the tests are skipped with a
# warning, so the normal build and install keep working.

find_package(Qt6 QUIET COMPONENTS Test)
if (NOT Qt6Test_FOUND)
    message(WARNING "Qt6 Test not found, the GPService tests are not built")
    return()
endif()

# Tests for the matching of openconnect output lines to Tunnel events.
add_executable(tst_tunnelevent
    tst_tunnelevent.cpp
    ../tunnelevent.cpp
)
target_include_directories(tst_tunnelevent PRIVATE ${CMAKE_SOURCE_DIR}/GPService)
target_link_libraries(tst_tunnelevent Qt6::Test Qt6::Core)
add_test(NAME tst_tunnelevent COMMAND tst_tunnelevent)
set_tests_properties(tst_tunnelevent PROPERTIES TIMEOUT 60)
