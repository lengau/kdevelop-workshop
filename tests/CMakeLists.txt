include(ECMAddTests)

ecm_add_test(${CMAKE_CURRENT_SOURCE_DIR}/test_parsesketchsdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parsesketchsdk.cpp
    TEST_NAME test_parsesketchsdk
    LINK_LIBRARIES Qt6::Test
)

target_include_directories(test_parsesketchsdk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
