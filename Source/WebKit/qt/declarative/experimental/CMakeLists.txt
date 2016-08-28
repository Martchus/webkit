add_library(qmlwebkitexperimentalplugin SHARED plugin.cpp)
target_link_libraries(qmlwebkitexperimentalplugin
    WebKit2 Qt5::Quick)
