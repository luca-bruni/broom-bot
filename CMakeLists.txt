cmake_minimum_required(VERSION 3.20)
project(broom_bot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Avoid admin-only default (Program Files) on Windows
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

option(BB_BUILD_DPP "Build DPP submodule (in-tree). OFF uses external/DPP/install" OFF)

set(DPP_SOURCE_DIR   "${CMAKE_SOURCE_DIR}/external/DPP")
set(DPP_SUBBUILD_DIR "${CMAKE_BINARY_DIR}/external/DPP")
set(DPP_INSTALL_DIR  "${DPP_SOURCE_DIR}/install")
set(DPP_DEPS_BIN_DIR "${DPP_SOURCE_DIR}/win32/bin") # where libssl/libcrypto/zlib1/opus live

# Prefer repo-local DPP install when BB_BUILD_DPP=OFF
list(PREPEND CMAKE_PREFIX_PATH "${DPP_INSTALL_DIR}")

if(BB_BUILD_DPP)
  if(NOT EXISTS "${DPP_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "DPP submodule not found. Run: git submodule update --init --recursive")
  endif()

  set(DPP_BUILD_TEST    OFF CACHE BOOL "" FORCE)
  set(TESTING           OFF CACHE BOOL "" FORCE)

  add_subdirectory("${DPP_SOURCE_DIR}" "${DPP_SUBBUILD_DIR}" EXCLUDE_FROM_ALL)
else()
  find_package(dpp CONFIG REQUIRED)
endif()

add_executable(broom_bot src/main.cpp)

if(BB_BUILD_DPP)
  target_link_libraries(broom_bot PRIVATE dpp)
else()
  target_link_libraries(broom_bot PRIVATE dpp::dpp)
endif()

# Make build-tree exe runnable on Windows: copy DPP + its packaged deps next to the exe
add_custom_command(TARGET broom_bot POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${DPP_INSTALL_DIR}/bin/dpp.dll"
            "$<TARGET_FILE_DIR:broom_bot>/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${DPP_DEPS_BIN_DIR}"
            "$<TARGET_FILE_DIR:broom_bot>"
    COMMENT "Copying DPP + runtime DLLs next to broom_bot.exe"
    VERBATIM
)

# Make install-tree runnable too
include(GNUInstallDirs)
install(TARGETS broom_bot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${DPP_INSTALL_DIR}/bin/dpp.dll" DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY "${DPP_DEPS_BIN_DIR}/" DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING PATTERN "*.dll")

# When building DPP in-tree, stage it into external/DPP/install during top-level install
if(BB_BUILD_DPP)
  add_custom_target(stage_dpp
    COMMAND "${CMAKE_COMMAND}" --install "${DPP_SUBBUILD_DIR}" --prefix "${DPP_INSTALL_DIR}"
    COMMENT "Staging DPP into ${DPP_INSTALL_DIR}"
  )

  install(CODE
    "execute_process(
       COMMAND \"${CMAKE_COMMAND}\" --install \"${DPP_SUBBUILD_DIR}\" --prefix \"${DPP_INSTALL_DIR}\"
       COMMAND_ERROR_IS_FATAL ANY
     )"
  )
endif()
