cmake_minimum_required(VERSION 3.25...4.3)

project(OpTimeNTP
        VERSION 0.1.0
        DESCRIPTION "Cross-platform NTP server comparison desktop application"
        HOMEPAGE_URL "https://github.com/opmatrixsoftware/OpTimeNTP"
        LANGUAGES CXX)

set(OPTIME_DISPLAY_NAME "OpTime NTP")
set(OPTIME_ORGANIZATION_NAME "OpMatrix Inc.")
set(OPTIME_APP_ID "io.github.opmatrixsoftware.optimentp")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(OPTIME_DEPLOY_QT_RUNTIME "Deploy the Qt runtime beside the application after building" ON)

if(WIN32)
    message(STATUS "Configuring OpTimeNTP for Windows x64")
    if(CMAKE_VERSION VERSION_LESS 4.2)
        message(FATAL_ERROR "Windows builds require CMake 4.2 or newer for the Visual Studio 18 2026 generator.")
    endif()
    if(NOT CMAKE_GENERATOR STREQUAL "Visual Studio 18 2026")
        message(FATAL_ERROR "Windows builds require the Visual Studio 18 2026 CMake generator.")
    endif()
    if(NOT MSVC OR NOT MSVC_TOOLSET_VERSION STREQUAL "145")
        message(FATAL_ERROR "Windows builds require the Visual Studio 2026 v145 MSVC toolset.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "OpTimeNTP supports Windows x64 only.")
    endif()
    set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders ON)
    if(NOT DEFINED CMAKE_PREFIX_PATH)
        set(CMAKE_PREFIX_PATH "C:/Development/Qt/6.10.3/msvc2022_64")
    endif()
elseif(APPLE)
    message(STATUS "Configuring OpTimeNTP for macOS ARM64")
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS target architectures")
    endif()
    if(NOT CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)arm64($|;)")
        message(FATAL_ERROR "OpTimeNTP supports macOS ARM64 only.")
    endif()
    if(NOT DEFINED CMAKE_PREFIX_PATH)
        set(CMAKE_PREFIX_PATH "/Library/Qt/6.10.3/macos")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Configuring OpTimeNTP for Linux x64")
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        message(FATAL_ERROR "OpTimeNTP supports Linux x64 only.")
    endif()
    if(NOT DEFINED CMAKE_PREFIX_PATH)
        set(CMAKE_PREFIX_PATH "/home/akbailey/Qt/6.10.3/gcc_64")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Core Gui Widgets Network)
include(FetchContent)
include(GNUInstallDirs)

set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(yaml-cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

set(OPTIME_SOURCES
        src/main.cpp
        src/FormatUtils.cpp
        src/MainWindow.cpp
        src/NtpClient.cpp
        src/NtpProtocol.cpp
        src/ServerCard.cpp
        src/ServerDialog.cpp
        src/ServerListYaml.cpp
        src/ServerValidation.cpp
        src/SystemTimeSetter.cpp
)

set(OPTIME_HEADERS
        src/FormatUtils.h
        src/MainWindow.h
        src/NtpClient.h
        src/NtpProtocol.h
        src/NtpTypes.h
        src/ServerCard.h
        src/ServerDialog.h
        src/ServerListYaml.h
        src/ServerValidation.h
        src/SystemTimeSetter.h
)

set(OPTIME_RESOURCES
        resources/app.qrc
)

set(_op_time_platform_flags)
if(WIN32)
    list(APPEND _op_time_platform_flags WIN32)
elseif(APPLE)
    list(APPEND _op_time_platform_flags MACOSX_BUNDLE)
endif()

add_executable(${PROJECT_NAME}
        ${_op_time_platform_flags}
        ${OPTIME_SOURCES}
        ${OPTIME_HEADERS}
        ${OPTIME_RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        yaml-cpp::yaml-cpp
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Shell32)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        "OPTIME_DISPLAY_NAME=\"${OPTIME_DISPLAY_NAME}\""
        "OPTIME_ORGANIZATION_NAME=\"${OPTIME_ORGANIZATION_NAME}\""
        "OPTIME_APP_ID=\"${OPTIME_APP_ID}\""
        "OPTIME_VERSION=\"${PROJECT_VERSION}\""
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${OPTIME_DISPLAY_NAME}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${OPTIME_APP_ID}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(OPTIME_DEPLOY_QT_RUNTIME AND WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt.exe
            HINTS "C:/Development/Qt/6.10.3/msvc2022_64/bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DWINDEPLOYQT_EXECUTABLE:FILEPATH=${WINDEPLOYQT_EXECUTABLE}
                    -DAPPLICATION_FILE:FILEPATH=$<TARGET_FILE:${PROJECT_NAME}>
                    -DBUILD_CONFIGURATION:STRING=$<CONFIG>
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_windeployqt.cmake"
                VERBATIM
                COMMENT "Deploying the Qt runtime for OpTimeNTP")
    else()
        message(WARNING "windeployqt was not found; the build output may require Qt on PATH.")
    endif()
elseif(OPTIME_DEPLOY_QT_RUNTIME AND APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
            HINTS "/Library/Qt/6.10.3/macos/bin")
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" -verbose=1
                VERBATIM
                COMMENT "Deploying the Qt runtime for OpTimeNTP.app")
    else()
        message(WARNING "macdeployqt was not found; the app bundle will not be self-contained.")
    endif()
endif()

install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE.txt THIRD_PARTY_NOTICES.md DESTINATION .)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES resources/io.github.akevinbailey.optimentp.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/optime_ntp.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME io.github.akevinbailey.optimentp.svg)
endif()

qt_generate_deploy_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT OPTIME_DEPLOY_SCRIPT
        NO_TRANSLATIONS
        NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${OPTIME_DEPLOY_SCRIPT})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Test)
    add_executable(OpTimeNTPTests
            tests/NtpProtocolTests.cpp
            src/FormatUtils.cpp
            src/NtpProtocol.cpp
            src/ServerListYaml.cpp
            src/ServerValidation.cpp
    )
    target_include_directories(OpTimeNTPTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(OpTimeNTPTests PRIVATE
            Qt6::Core
            Qt6::Test
            yaml-cpp::yaml-cpp
    )
    if(MSVC)
        target_compile_options(OpTimeNTPTests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(OpTimeNTPTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME OpTimeNTP.ProtocolAndFormatting COMMAND OpTimeNTPTests)
    if(WIN32)
        set_tests_properties(OpTimeNTP.ProtocolAndFormatting PROPERTIES
                ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endif()
