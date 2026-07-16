set(kdevelop-workshop_SRCS
    kdevelop-workshop.cpp
    workshopruntime.cpp
    workshopconfigpage.cpp
    workshoptoolview.cpp
    workshopterminaltoolview.cpp
    workshopapi.cpp
    workshopwizard.cpp
    sketchsdkpanel.cpp
    parsesketchsdk.cpp
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

target_link_libraries(kdevelop-workshop
    KDev::Interfaces
    KDev::Util
    KDev::Sublime
    KF6::Parts
)
