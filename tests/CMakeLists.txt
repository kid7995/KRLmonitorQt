set(KRLMONITOR_TEST_CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/protocol.cpp
    ${PROJECT_SOURCE_DIR}/src/station.cpp
    ${PROJECT_SOURCE_DIR}/src/model.cpp
    ${PROJECT_SOURCE_DIR}/src/decimate.cpp
    ${PROJECT_SOURCE_DIR}/src/csvio.cpp
    ${PROJECT_SOURCE_DIR}/src/recorder.cpp
)

add_library(KRLmonitorTestCore STATIC ${KRLMONITOR_TEST_CORE_SOURCES})
target_include_directories(KRLmonitorTestCore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(KRLmonitorTestCore PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

function(add_krlmonitor_test test_name)
    add_executable(${test_name} ${ARGN})
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${test_name} PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        KRLmonitorTestCore
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endfunction()

add_krlmonitor_test(tst_protocol tst_protocol.cpp)
add_krlmonitor_test(tst_model tst_model.cpp)
add_krlmonitor_test(tst_decimate tst_decimate.cpp)
add_krlmonitor_test(tst_csvio tst_csvio.cpp)
add_krlmonitor_test(tst_recorder tst_recorder.cpp)
add_krlmonitor_test(tst_station tst_station.cpp)