include(ECMAddTests)

ecm_add_test(${CMAKE_CURRENT_SOURCE_DIR}/test_parsesketchsdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sketchsdk/parsesketchsdk.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/../src/debug.cpp
    TEST_NAME test_parsesketchsdk
    LINK_LIBRARIES Qt6::Test yaml-cpp::yaml-cpp
)

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_parsesketchsdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sketchsdk/parsesketchsdk.cpp
    PROPERTIES COMPILE_OPTIONS "-fexceptions"
)

target_include_directories(test_parsesketchsdk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sketchsdk
    ${CMAKE_CURRENT_BINARY_DIR}/../src
)
