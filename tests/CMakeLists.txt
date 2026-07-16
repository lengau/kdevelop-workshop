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

ecm_add_test(${CMAKE_CURRENT_SOURCE_DIR}/test_workshopcontextmenu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/views/workshopcontextmenu.cpp
    TEST_NAME test_workshopcontextmenu
    LINK_LIBRARIES Qt6::Test Qt6::Widgets
)

target_include_directories(test_workshopcontextmenu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/views
)

set_tests_properties(test_workshopcontextmenu PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

ecm_add_test(${CMAKE_CURRENT_SOURCE_DIR}/test_projectselectionguard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/views/projectselectionguard.cpp
    TEST_NAME test_projectselectionguard
    LINK_LIBRARIES Qt6::Test
)

target_include_directories(test_projectselectionguard PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/views
)
