set(kdevelop-workshop_SRCS
    kdevelop-workshop.cpp
    api/workshopapi.cpp
    config/workshopconfigpage.cpp
    runtime/workshopruntime.cpp
    views/workshopcontextmenu.cpp
    views/workshoptoolview.cpp
    views/workshopterminaltoolview.cpp
    wizard/workshopwizard.cpp
    sketchsdk/sketchsdkpanel.cpp
    sketchsdk/parsesketchsdk.cpp
)

ecm_qt_declare_logging_category(kdevelop-workshop_SRCS
    HEADER debug.h
    IDENTIFIER PLUGIN_KDEVELOP_WORKSHOP
    CATEGORY_NAME "kdevelop.plugins.kdevelop-workshop"
    DESCRIPTION "KDevelop plugin: kdevelop-workshop"
    EXPORT KDEVELOP_WORKSHOP
)

kdevplatform_add_plugin(kdevelop-workshop
    SOURCES ${kdevelop-workshop_SRCS}
)

set_source_files_properties(
    sketchsdk/parsesketchsdk.cpp
    wizard/workshopwizard.cpp
    PROPERTIES COMPILE_OPTIONS "-fexceptions"
)

target_link_libraries(kdevelop-workshop
    KDev::Interfaces
    KDev::Util
    KDev::Sublime
    KF6::Parts
    KF6::WidgetsAddons
    Qt6::Network
    yaml-cpp::yaml-cpp
)

# Ensure src/ and build/src/ are on the include path for all subdirectory sources
target_include_directories(kdevelop-workshop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
