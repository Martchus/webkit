set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

# FIXME: It should be in WebKitFS actually
set(FORWARDING_HEADERS_DIR "${DERIVED_SOURCES_DIR}/ForwardingHeaders")

if (ENABLE_WEBKIT2)
    add_definitions(-DHAVE_WEBKIT2=1)

    add_custom_target(TestWebKitAPI-forwarding-headers
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${FORWARDING_HEADERS_DIR} --platform qt
        DEPENDS WebKit2-forwarding-headers
    )
    set(ForwardingHeadersForTestWebKitAPI_NAME TestWebKitAPI-forwarding-headers)
endif ()


include_directories(
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore
    ${TESTWEBKITAPI_DIR}

    # The WebKit2 Qt APIs depend on qwebkitglobal.h, which lives in WebKit
    "${WEBKIT_DIR}/qt/Api"

    "${WEBKIT2_DIR}/UIProcess/API/qt"
)

include_directories(SYSTEM
    ${ICU_INCLUDE_DIRS}
    ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5Quick_INCLUDE_DIRS}
    ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
)

add_definitions(
    -DAPITEST_SOURCE_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    -DROOT_BUILD_DIR="${CMAKE_BINARY_DIR}"
    -DQT_NO_CAST_FROM_ASCII
)

if (WIN32)
    add_definitions(-DUSE_CONSOLE_ENTRY_POINT)
    add_definitions(-DWEBCORE_EXPORT=)
    add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
endif ()

set(test_main_SOURCES
    qt/main.cpp
)

set(bundle_harness_SOURCES
    qt/InjectedBundleControllerQt.cpp
    qt/PlatformUtilitiesQt.cpp
)

list(APPEND test_wtf_LIBRARIES
    ${Qt5Gui_LIBRARIES}
    ${DEPEND_STATIC_LIBS}
)

add_executable(TestWebCore
    ${test_main_SOURCES}
    ${TESTWEBKITAPI_DIR}/TestsController.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/LayoutUnit.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/URL.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/SharedBuffer.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/FileSystem.cpp
    ${TESTWEBKITAPI_DIR}/Tests/WebCore/PublicSuffix.cpp
)

target_link_libraries(TestWebCore ${test_webcore_LIBRARIES})
if (ENABLE_WEBKIT2)
    add_dependencies(TestWebCore ${ForwardingHeadersForTestWebKitAPI_NAME})
    target_link_libraries(TestWebCore WebKit2)
    list(APPEND test_wtf_LIBRARIES
        WebKit2
    )
endif ()

add_test(TestWebCore ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore/TestWebCore)
set_tests_properties(TestWebCore PROPERTIES TIMEOUT 60)
set_target_properties(TestWebCore PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WebCore)
