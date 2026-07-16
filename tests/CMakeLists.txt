include(ECMAddTests)

ecm_add_test(test_parsesketchsdk.cpp ../src/parsesketchsdk.cpp
    TEST_NAME test_parsesketchsdk
    LINK_LIBRARIES Qt6::Test
)

target_include_directories(test_parsesketchsdk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
