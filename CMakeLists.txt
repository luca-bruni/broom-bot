cmake_minimum_required(VERSION 3.16)
project(broom_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ON:  build the DPP submodule as part of this project and install it into
#      external/DPP/install (use on first build, or after bumping the submodule).
# OFF: link the pre-built DPP from external/DPP/install, or any prefix passed
#      via -DCMAKE_PREFIX_PATH.
option(BB_BUILD_DPP "Build the DPP submodule from source" OFF)

set(DPP_INSTALL_DIR "${CMAKE_SOURCE_DIR}/external/DPP/install")

if(BB_BUILD_DPP)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/external/DPP/CMakeLists.txt")
        message(FATAL_ERROR
            "DPP submodule not found. Run: git submodule update --init --recursive")
    endif()

    set(DPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(DPP_INSTALL    ON  CACHE BOOL "" FORCE)
    add_subdirectory(external/DPP "${CMAKE_BINARY_DIR}/dpp")
    add_library(dpp::dpp ALIAS dpp)

    # Populate external/DPP/install so later configures can use BB_BUILD_DPP=OFF.
    add_custom_target(dpp_install ALL
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}/dpp"
                --prefix "${DPP_INSTALL_DIR}" --config $<CONFIG>
        DEPENDS dpp
        COMMENT "Installing DPP into ${DPP_INSTALL_DIR}")
else()
    list(APPEND CMAKE_PREFIX_PATH "${DPP_INSTALL_DIR}")
    find_package(dpp CONFIG REQUIRED)

    # DPP's exported target omits its include directory; derive it from the
    # package location (<prefix>/lib/cmake/dpp -> <prefix>/include).
    get_filename_component(DPP_PREFIX "${dpp_DIR}/../../.." ABSOLUTE)
    set_target_properties(dpp::dpp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DPP_PREFIX}/include")
endif()

add_executable(broom_bot
    src/main.cpp
    src/core/config.cpp
    src/core/registry.cpp
    src/commands/all_commands.cpp
    src/commands/ping.cpp
)
target_include_directories(broom_bot PRIVATE src)
target_link_libraries(broom_bot PRIVATE dpp::dpp)

# On Windows, place dpp.dll and its dependencies next to the executable.
if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    add_custom_command(TARGET broom_bot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:broom_bot> $<TARGET_FILE_DIR:broom_bot>
        COMMAND_EXPAND_LISTS)
endif()
