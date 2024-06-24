cmake_minimum_required(VERSION 3.20)

execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(TIMESTAMP BUILD_TIME "%Y%m%d" UTC)

project(OTK
    VERSION 0.1.0
    DESCRIPTION "Abaqus ODB Toolkit - ODB<->VTK file reader and writer"
    LANGUAGES CXX)
message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION} (${BUILD_TIME}.${GIT_HASH}.${GIT_BRANCH})")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK REQUIRED)
message(STATUS "VTK version: ${VTK_VERSION}")
message(STATUS "VTK path: ${VTK_DIR}")

if(WIN32)
    # set(otk_abaqus_dir "C:\\SIMULIA\\EstProducts\\2023\\win_b64" CACHE STRING "")
    set(otk_abaqus_dir "C:\\SIMULIA\\Snapshot\\2024\\win_b64" CACHE STRING "")
elseif(UNIX)
    set(otk_abaqus_dir "/usr/SIMULIA/EstProducts/2023/linux_a64" CACHE STRING "")
endif()

if(EXISTS ${otk_abaqus_dir})
    message(STATUS "Abaqus installation found at ${otk_abaqus_dir}")
else()
    message(FATAL_ERROR "Abaqus installation not found at ${otk_abaqus_dir}")
endif()

get_filename_component(otk_abaqus_pardir ${otk_abaqus_dir} DIRECTORY)

set(abq_odb_api_libraries
    "${otk_abaqus_dir}/code/lib/ABQSMAOdbApi.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAOdbCore.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAOdbAttrEO2.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAOdbAttrEO.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAOdbCoreGeom.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAShpCore.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMARomDiagEx.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMARfmInterface.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAAbuGeom.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMAAbuBasicUtils.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMABasShared.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMABasCoreUtils.lib"
    "${otk_abaqus_dir}/code/lib/ABQSMABASAlloc.lib"
)
set(abq_odb_api_includes "${otk_abaqus_dir}/code/include" "${otk_abaqus_pardir}")

include(FetchContent)

FetchContent_Declare(fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG "10.2.1")
FetchContent_Declare(argparse
    GIT_REPOSITORY "https://github.com/p-ranav/argparse"
    GIT_TAG "v3.0")
FetchContent_Declare(json
    GIT_REPOSITORY "https://github.com/nlohmann/json"
    GIT_TAG "v3.11.3")

FetchContent_GetProperties(fmt)
FetchContent_GetProperties(argparse)
FetchContent_GetProperties(json)

if(NOT fmt_POPULATED)
    FetchContent_Populate(fmt)
    add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

if(NOT argparse_POPULATED)
    FetchContent_Populate(argparse)
    add_subdirectory(${argparse_SOURCE_DIR} ${argparse_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

if(NOT json_POPULATED)
    FetchContent_Populate(json)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(otk
    ${CMAKE_SOURCE_DIR}/src/otk/otk.cpp

    ${CMAKE_SOURCE_DIR}/src/otk/cli.cpp
    ${CMAKE_SOURCE_DIR}/include/otk/cli.hpp

    ${CMAKE_SOURCE_DIR}/src/otk/odb.cpp
    ${CMAKE_SOURCE_DIR}/include/otk/odb.hpp

    ${CMAKE_SOURCE_DIR}/src/otk/output.cpp
    ${CMAKE_SOURCE_DIR}/include/otk/output.hpp

    ${CMAKE_SOURCE_DIR}/src/otk/converter.cpp
    ${CMAKE_SOURCE_DIR}/include/otk/converter.hpp)
target_link_libraries(otk PUBLIC
    ${VTK_LIBRARIES}
    ${abq_odb_api_libraries}
    fmt::fmt-header-only
    argparse
    nlohmann_json::nlohmann_json)
target_include_directories(otk PUBLIC
    ${abq_odb_api_includes}
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(otk PRIVATE
    "OTK_VERSION=${CMAKE_PROJECT_VERSION}"
    "OTK_BUILD=${BUILD_TIME}.${GIT_HASH}.${GIT_BRANCH}")

if(WIN32)
    target_compile_definitions(otk PUBLIC "_WINDOWS_SOURCE")
endif()
