cmake_minimum_required(VERSION 3.20)
project(broom_bot VERSION 0.1.0 LANGUAGES C CXX)

# Single-config generators (Ninja): Microsoft CMake defaults CMAKE_BUILD_TYPE to Debug.
# We want Release by default without requiring -DCMAKE_BUILD_TYPE=Release.
set(BB_BUILD_TYPE "Release" CACHE STRING "broom_bot build type (single-config only)")
set_property(CACHE BB_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "${BB_BUILD_TYPE}" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Avoid admin-only default (Program Files) on Windows
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

# ON:  build the DPP submodule as part of this project and install it into
#      external/DPP/install (use on first build, or after bumping the submodule).
# OFF: link the pre-built DPP from external/DPP/install, or any prefix passed
#      via -DCMAKE_PREFIX_PATH.
option(BB_BUILD_DPP "Build the DPP submodule from source" OFF)

# OFF skips the bot executable (and the DPP dependency entirely) and builds
# only the DPP-free unit tests — used by the sanitizer CI job, where
# instrumenting the tests is cheap but building DPP is not.
option(BB_BUILD_BOT "Build the bot executable (requires DPP)" ON)

set(DPP_SOURCE_DIR   "${CMAKE_SOURCE_DIR}/external/DPP")
set(DPP_SUBBUILD_DIR "${CMAKE_BINARY_DIR}/dpp")
set(DPP_INSTALL_DIR  "${DPP_SOURCE_DIR}/install")
set(DPP_DEPS_BIN_DIR "${DPP_SOURCE_DIR}/win32/bin") # DPP's bundled libssl/libcrypto/zlib1/opus DLLs

# Prefer the repo-local DPP install when BB_BUILD_DPP=OFF
list(PREPEND CMAKE_PREFIX_PATH "${DPP_INSTALL_DIR}")

# On Windows, pin OpenSSL to DPP's bundled 1.1 SDK (headers + import libs that
# match the prebuilt dpp.dll and the libssl-1_1/libcrypto-1_1 DLLs shipped next
# to the exe). Without this, find_package(OpenSSL) — invoked transitively by
# DPP's package config — can latch onto an unrelated OpenSSL on PATH (e.g.
# PostgreSQL's 3.x), which is ABI-incompatible with dpp.dll.
if(WIN32 AND EXISTS "${DPP_SOURCE_DIR}/win32/lib/libssl.lib")
    set(OPENSSL_ROOT_DIR "${DPP_SOURCE_DIR}/win32" CACHE PATH "OpenSSL root (DPP bundled)" FORCE)
endif()

if(BB_BUILD_BOT AND BB_BUILD_DPP)
    if(NOT EXISTS "${DPP_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "DPP submodule not found. Run: git submodule update --init --recursive")
    endif()

    set(DPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(DPP_INSTALL    ON  CACHE BOOL "" FORCE)
    set(TESTING        OFF CACHE BOOL "" FORCE)

    add_subdirectory("${DPP_SOURCE_DIR}" "${DPP_SUBBUILD_DIR}" EXCLUDE_FROM_ALL)
    add_library(dpp::dpp ALIAS dpp)

    # Populate external/DPP/install so later configures can use BB_BUILD_DPP=OFF.
    add_custom_target(dpp_install ALL
        COMMAND ${CMAKE_COMMAND} --install "${DPP_SUBBUILD_DIR}"
                --prefix "${DPP_INSTALL_DIR}" --config $<CONFIG>
        DEPENDS dpp
        COMMENT "Installing DPP into ${DPP_INSTALL_DIR}")
elseif(BB_BUILD_BOT)
    find_package(dpp CONFIG REQUIRED)

    # DPP's exported target omits its include directory; derive it from the
    # package location (<prefix>/lib/cmake/dpp -> <prefix>/include). On Windows
    # DPP installs headers into a versioned subdir (include/dpp-X.Y).
    get_filename_component(DPP_PREFIX "${dpp_DIR}/../../.." ABSOLUTE)
    if(EXISTS "${DPP_PREFIX}/include/dpp/dpp.h")
        set(DPP_INCLUDE_DIR "${DPP_PREFIX}/include")
    else()
        file(GLOB DPP_VERSIONED_INCLUDES "${DPP_PREFIX}/include/dpp-*")
        list(LENGTH DPP_VERSIONED_INCLUDES DPP_VERSIONED_COUNT)
        if(DPP_VERSIONED_COUNT EQUAL 0)
            message(FATAL_ERROR "Could not locate dpp/dpp.h under ${DPP_PREFIX}/include")
        endif()
        list(GET DPP_VERSIONED_INCLUDES 0 DPP_INCLUDE_DIR)
    endif()
    set_target_properties(dpp::dpp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${DPP_INCLUDE_DIR}")
endif()

if(BB_BUILD_BOT)

# Vendored SQLite amalgamation (see ARCHITECTURE.md; not a submodule)
add_library(sqlite3 STATIC external/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC external/sqlite)
target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_LOAD_EXTENSION=1)
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PRIVATE Threads::Threads)

add_executable(broom_bot
    src/main.cpp
    src/core/config.cpp
    src/core/db.cpp
    src/core/jobs.cpp
    src/core/registry.cpp
    src/commands/all_commands.cpp
    src/commands/about.cpp
    src/commands/avatar.cpp
    src/commands/channelinfo.cpp
    src/commands/choose.cpp
    src/commands/coinflip.cpp
    src/commands/eightball.cpp
    src/commands/emojiinfo.cpp
    src/commands/help.cpp
    src/commands/jobs.cpp
    src/commands/ping.cpp
    src/commands/purge.cpp
    src/commands/remind.cpp
    src/commands/roleinfo.cpp
    src/commands/roll.cpp
    src/commands/schedule.cpp
    src/commands/serverinfo.cpp
    src/commands/stats.cpp
    src/commands/userinfo.cpp
)
target_include_directories(broom_bot PRIVATE src)
target_compile_definitions(broom_bot PRIVATE BROOM_VERSION="${broom_bot_VERSION}")
target_link_libraries(broom_bot PRIVATE dpp::dpp sqlite3)

include(GNUInstallDirs)
install(TARGETS broom_bot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif() # BB_BUILD_BOT

# Make the exe runnable on Windows: copy dpp.dll + DPP's packaged deps next to it,
# in both the build tree and the install tree.
if(BB_BUILD_BOT AND WIN32)
    if(BB_BUILD_DPP)
        # Ensure dpp.dll is staged into DPP_INSTALL_DIR before the copy below runs.
        add_dependencies(broom_bot dpp_install)
    endif()

    add_custom_command(TARGET broom_bot POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${DPP_INSTALL_DIR}/bin/dpp.dll"
                "$<TARGET_FILE_DIR:broom_bot>/"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${DPP_DEPS_BIN_DIR}"
                "$<TARGET_FILE_DIR:broom_bot>"
        COMMENT "Copying DPP + runtime DLLs next to broom_bot.exe"
        VERBATIM)

    install(FILES "${DPP_INSTALL_DIR}/bin/dpp.dll" DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY "${DPP_DEPS_BIN_DIR}/" DESTINATION ${CMAKE_INSTALL_BINDIR}
            FILES_MATCHING PATTERN "*.dll")
endif()

# Unit tests (vendored doctest, header-only). Cover the pure logic — parsers and
# filter predicates — that runs without a live Discord connection. New commands
# should extend these. Toggle off with -DBB_BUILD_TESTS=OFF.
option(BB_BUILD_TESTS "Build unit tests" ON)
if(BB_BUILD_TESTS)
    enable_testing()
    add_executable(broom_tests
        tests/doctest_main.cpp
        tests/test_duration.cpp
        tests/test_timeparse.cpp
        tests/test_purge_filter.cpp
        tests/test_info_format.cpp
        tests/test_pagination.cpp
        tests/test_remind_rules.cpp
    )
    target_include_directories(broom_tests PRIVATE src external/doctest)
    add_test(NAME broom_tests COMMAND broom_tests)
endif()
