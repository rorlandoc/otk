cmake_minimum_required(VERSION 3.16)

execute_process(
      COMMAND git log -1 --format=%h
      WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      OUTPUT_VARIABLE GIT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE)
string(TIMESTAMP BUILD_TIME "%Y%m%d" UTC)

project(OTK
    VERSION 0.1.${BUILD_TIME}
    DESCRIPTION "Abaqus ODB Toolkit - ODB<->VTK file reader and writer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK REQUIRED)

if(WIN32)
    set(otk_abaqus_dir "C:\\SIMULIA\\EstProducts\\2023\\win_b64" CACHE STRING "")
elseif(UNIX)
    set(otk_abaqus_dir "/usr/SIMULIA/EstProducts/2023/linux_a64" CACHE STRING "")
endif()

if(EXISTS ${otk_abaqus_dir})
    message(STATUS "Abaqus installation found at ${otk_abaqus_dir}")
else()
    message(FATAL_ERROR "Abaqus installation not found at ${otk_abaqus_dir}")
endif()

get_filename_component(otk_abaqus_pardir ${otk_abaqus_dir} DIRECTORY)

set(abq_odb_api_libraries "${otk_abaqus_dir}/code/lib/ABQSMAOdbApi.lib")
set(abq_odb_api_includes "${otk_abaqus_dir}/code/include" "${otk_abaqus_pardir}")

include(FetchContent)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP True)
FetchContent_Declare(argparse
    GIT_REPOSITORY "https://github.com/p-ranav/argparse")

FetchContent_MakeAvailable(json)
FetchContent_MakeAvailable(argparse)

add_library(otk STATIC
    include/otk/otk.hpp
    src/otk/otk.cpp
)
target_link_libraries(otk PUBLIC 
  ${VTK_LIBRARIES} 
  ${abq_odb_api_libraries} 
  nlohmann_json::nlohmann_json
  argparse)
target_include_directories(otk PUBLIC 
  ${abq_odb_api_includes} 
  include 
  src)

add_executable(otk_cli src/otk_cli/otk_cli.cpp)
target_link_libraries(otk_cli PRIVATE otk)
target_include_directories(otk_cli PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(otk_cli PRIVATE "-DOTK_VERSION=${CMAKE_PROJECT_VERSION}_${GIT_HASH}")

if(WIN32)
    target_compile_definitions(otk PUBLIC _WINDOWS_SOURCE)
endif()
