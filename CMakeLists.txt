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

# Paths
set(DPP_SOURCE_DIR      "${CMAKE_SOURCE_DIR}/external/DPP")
set(DPP_BUILD_DIR       "${DPP_SOURCE_DIR}/build")
set(DPP_INSTALL_DIR     "${DPP_SOURCE_DIR}/install")
set(DPP_LIB_PATH "${DPP_INSTALL_DIR}/lib")
set(DPP_INCLUDE_PATH "${DPP_INSTALL_DIR}/include")

# Target
add_executable(broom_bot
    src/main.cpp
)

# Submodules

# Always prefer the repo-local DPP install prefix by default.
# Users can still override by passing -DCMAKE_PREFIX_PATH=... on the command line.
list(APPEND CMAKE_PREFIX_PATH "${DPP_INSTALL_DIR}")

if(BB_BUILD_DPP)
    # Error out if submodule does not exist
    if(NOT EXISTS "${DPP_SOURCE_DIR}/CMakeLists.txt")
        message(
            FATAL_ERROR
            "DPP submodule not found.\n" 
            "Run: git submodule update --init --recursive"
        )
    endif()

    # Configure DPP
    set(DPP_BUILD_TEST  OFF CACHE BOOL "" FORCE)
    set(DPP_INSTALL     ON  CACHE BOOL "" FORCE)
    set(TESTING         OFF CACHE BOOL "" FORCE)

    # Force install prefix for DPP
    set(CMAKE_INSTALL_PREFIX
        "${DPP_INSTALL_DIR}"
        CACHE PATH "DPP install prefix" FORCE
    )

    add_subdirectory("${DPP_SOURCE_DIR}" "${DPP_BUILD_DIR}")
    target_link_libraries(broom_bot PRIVATE dpp)

    # Generalized install target
    add_custom_target(install_submodules ALL
        COMMAND ${CMAKE_COMMAND} --install "${DPP_BUILD_DIR}" --prefix "${DPP_INSTALL_DIR}"
        COMMENT "Installing DPP"
    )
else()
    # Cross-platform library check
    if(WIN32)
        # Windows: check DLL import lib first, then static lib
        if(EXISTS "${DPP_LIB_PATH}/dpp.dll" AND EXISTS "${DPP_LIB_PATH}/dpp.lib")
            # We have a DLL + import library → shared
            set(DPP_LIB_FILE "${DPP_LIB_PATH}/dpp.lib")
            set(DPP_LIB_TYPE SHARED)
        elseif(EXISTS "${DPP_LIB_PATH}/dpp.lib")
            # Only lib exists → static
            set(DPP_LIB_FILE "${DPP_LIB_PATH}/dpp.lib")
            set(DPP_LIB_TYPE STATIC)
        else()
            message(FATAL_ERROR "No DPP library found in ${DPP_LIB_PATH}")
        endif()
    elseif(APPLE)
        # macOS: prefer dynamic, fall back to static
        if(EXISTS "${DPP_LIB_PATH}/libdpp.dylib")
            set(DPP_LIB_FILE "${DPP_LIB_PATH}/libdpp.dylib")
            set(DPP_LIB_TYPE SHARED)
        elseif(EXISTS "${DPP_LIB_PATH}/libdpp.a")
            set(DPP_LIB_FILE "${DPP_LIB_PATH}/libdpp.a")
            set(DPP_LIB_TYPE STATIC)
        else()
            message(FATAL_ERROR "No DPP library found in ${DPP_LIB_PATH}")
        endif()
    else()
        # Linux/Unix: prefer .so, fall back to .a
        if(EXISTS "${DPP_LIB_PATH}/libdpp.so")
            set(DPP_LIB_FILE "${DPP_LIB_PATH}/libdpp.so")
            set(DPP_LIB_TYPE SHARED)
        elseif(EXISTS "${DPP_LIB_PATH}/libdpp.a")
            set(DPP_LIB_FILE "${DPP_LIB_PATH}/libdpp.a")
            set(DPP_LIB_TYPE STATIC)
        else()
            message(FATAL_ERROR "No DPP library found in ${DPP_LIB_PATH}")
        endif()
    endif()

    if(NOT EXISTS "${DPP_LIB_FILE}")
        message(FATAL_ERROR "DPP library not found in ${DPP_LIB_PATH}. Build or install DPP first.")
    endif()

    # Create imported target
    add_library(dpp ${DPP_LIB_TYPE} IMPORTED)
    set_target_properties(dpp PROPERTIES
        IMPORTED_LOCATION "${DPP_LIB_FILE}"
        INTERFACE_INCLUDE_DIRECTORIES "${DPP_INCLUDE_PATH}"
    )

    target_link_libraries(broom_bot PRIVATE dpp)
endif()
