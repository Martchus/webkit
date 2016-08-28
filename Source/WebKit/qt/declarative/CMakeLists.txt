add_definitions(-DHAVE_WEBKIT2=1)
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"

    # The WebKit2 Qt APIs depend on qwebkitglobal.h, which lives in WebKit
    "${WEBKIT_DIR}/qt/Api"

    "${WEBKIT2_DIR}/Platform"
    "${WEBKIT2_DIR}/Platform/IPC"
    "${WEBKIT2_DIR}/Shared/qt"
    "${WEBKIT2_DIR}/UIProcess/API/qt"

    ${WTF_DIR}
)
include_directories(SYSTEM
    ${Qt5Quick_INCLUDE_DIRS}
    ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
)

add_library(qmlwebkitplugin SHARED plugin.cpp)
target_link_libraries(qmlwebkitplugin
    WebKit2 Qt5::Quick)

add_subdirectory(experimental)
