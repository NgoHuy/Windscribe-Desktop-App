cmake_minimum_required(VERSION 3.5)

set(PROJECT_SOURCES
        apiresources_test_main.cpp
        apiresources_test_widget.cpp
        apiresources_test_widget.h
        apiresources_test_widget.ui
	apiresources_test.qrc
)

add_executable(apiresources_visual_test ${PROJECT_SOURCES} )
target_link_libraries(apiresources_visual_test PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Test engine common ${OS_SPECIFIC_LIBRARIES})

target_include_directories(apiresources_visual_test PRIVATE
    ${PROJECT_DIRECTORY}/engine
    ${PROJECT_DIRECTORY}/common
    ${WINDSCRIBE_BUILD_LIBS_PATH}/curl/include
    ${WINDSCRIBE_BUILD_LIBS_PATH}/openssl/include
)

set_target_properties(apiresources_visual_test PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

