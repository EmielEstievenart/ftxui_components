cmake_minimum_required(VERSION 3.21)

project(ftxui_components LANGUAGES CXX)

if(NOT TARGET component)
  set(FTXUI_BUILD_DOCS
      OFF
      CACHE BOOL "Disable FTXUI docs" FORCE)
  set(FTXUI_BUILD_EXAMPLES
      OFF
      CACHE BOOL "Disable FTXUI examples" FORCE)
  set(FTXUI_BUILD_TESTS
      OFF
      CACHE BOOL "Disable FTXUI tests" FORCE)
  set(FTXUI_BUILD_TESTS_FUZZER
      OFF
      CACHE BOOL "Disable FTXUI fuzz tests" FORCE)
  set(FTXUI_BUILD_MODULES
      OFF
      CACHE BOOL "Disable FTXUI C++20 modules" FORCE)

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/ftxui/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/libs/ftxui"
                     "${CMAKE_CURRENT_BINARY_DIR}/ftxui")
  else()
    message(
      FATAL_ERROR
        "FTXUI was not found at libs/ftxui. Initialize this repository's libs/ftxui submodule first."
    )
  endif()
endif()

add_subdirectory(code)

if(PROJECT_IS_TOP_LEVEL)
  set(FTXUI_COMPONENTS_BUILD_EXAMPLES_DEFAULT ON)
else()
  set(FTXUI_COMPONENTS_BUILD_EXAMPLES_DEFAULT OFF)
endif()

option(FTXUI_COMPONENTS_BUILD_EXAMPLES "Build ftxui_components examples"
       ${FTXUI_COMPONENTS_BUILD_EXAMPLES_DEFAULT})

if(FTXUI_COMPONENTS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
