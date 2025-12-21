cmake_minimum_required(VERSION 3.13)

project(broom_bot LANGUAGES CXX)

# Standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(
    BB_BUILD_DPP
    "Build submodule DPP and install into external/DPP/install"
    OFF
)

# Environment variables
set(DPP_SOURCE_DIR      "${CMAKE_SOURCE_DIR}/external/DPP")
set(DPP_BUILD_DIR       "${DPP_SOURCE_DIR}/build")
set(DPP_SUBBUILD_DIR    "${CMAKE_BINARY_DIR}/external/DPP")
set(DPP_INSTALL_DIR     "${DPP_SOURCE_DIR}/install")

# Always prefer the repo-local DPP install prefix by default.
# Users can still override by passing -DCMAKE_PREFIX_PATH=... on the command line.
list(APPEND CMAKE_PREFIX_PATH "${DPP_INSTALL_DIR}")

if(BB_BUILD_DPP)
    # Error out if submodule does not exist
    if(NOT EXISTS "${DPP_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "DPP submodule not found. Run: git submodule update --init --recursive in project root")
    endif()

    set(DPP_BUILD_TEST  OFF CACHE BOOL "" FORCE)
    set(TESTING         OFF CACHE BOOL "" FORCE)
    set(DPP_INSTALL     ON  CACHE BOOL "" FORCE)

    add_subdirectory("${DPP_SOURCE_DIR}" EXCLUDE_FROM_ALL)

    add_custom_target(dpp_stage ALL
        COMMAND ${CMAKE_COMMAND} --install "${DPP_SUBBUILD_DIR}" --prefix "{DPP_INSTALL_DIR}"
        COMMENT "Staging DPP into ${DPP_INSTALL_DIR}"
    )
else()
    find_package(dpp CONFIG REQUIRED)
endif()

add_executable(broom_bot
    src/main.cpp
)

target_link_libraries(broom_bot PRIVATE dpp)
