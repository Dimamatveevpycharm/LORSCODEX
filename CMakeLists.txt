cmake_minimum_required(VERSION 3.20)
project(lorsc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(LORSC_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${LORSC_GENERATED_DIR})

set(LORSC_PROJECT_SOURCES
  src/main.cpp
  src/AST.cpp
  src/Diagnostics.cpp
  src/Parser.cpp
  src/Sema.cpp
  src/Codegen.cpp
)

bison_target(
  LorsParser
  ${CMAKE_SOURCE_DIR}/src/parser.y
  ${LORSC_GENERATED_DIR}/parser.tab.cpp
  DEFINES_FILE ${LORSC_GENERATED_DIR}/parser.tab.hpp
)

flex_target(
  LorsLexer
  ${CMAKE_SOURCE_DIR}/src/lexer.l
  ${LORSC_GENERATED_DIR}/lexer.yy.cpp
)

add_flex_bison_dependency(LorsLexer LorsParser)

set(LORSC_GENERATED_SOURCES
  ${BISON_LorsParser_OUTPUTS}
  ${FLEX_LorsLexer_OUTPUTS}
)

add_executable(lorsc ${LORSC_PROJECT_SOURCES} ${LORSC_GENERATED_SOURCES})

target_include_directories(
  lorsc
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LORSC_GENERATED_DIR}
)

target_include_directories(
  lorsc
  SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
set(LORSC_LLVM_COMPILE_DEFINITIONS)
set(LORSC_LLVM_EXTRA_COMPILE_OPTIONS)
foreach(LLVM_DEF IN LISTS LLVM_DEFINITIONS_LIST)
  if(LLVM_DEF MATCHES "^-D(.+)")
    set(LLVM_DEF_NAME "${CMAKE_MATCH_1}")
    # Toolchains often define this already; forwarding it from LLVM can cause noisy redefinition warnings.
    if(NOT LLVM_DEF_NAME STREQUAL "_GNU_SOURCE")
      list(APPEND LORSC_LLVM_COMPILE_DEFINITIONS "${LLVM_DEF_NAME}")
    endif()
  else()
    list(APPEND LORSC_LLVM_EXTRA_COMPILE_OPTIONS "${LLVM_DEF}")
  endif()
endforeach()
list(REMOVE_DUPLICATES LORSC_LLVM_COMPILE_DEFINITIONS)
list(REMOVE_DUPLICATES LORSC_LLVM_EXTRA_COMPILE_OPTIONS)

target_compile_definitions(lorsc PRIVATE ${LORSC_LLVM_COMPILE_DEFINITIONS})

# Keep strict warnings for project code while reducing external-header noise.
target_compile_options(
  lorsc
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    ${LORSC_LLVM_EXTRA_COMPILE_OPTIONS}
)

if(TARGET LLVM)
  target_link_libraries(lorsc PRIVATE LLVM)
else()
  llvm_map_components_to_libnames(
    LLVM_LIBS
    Core
    Support
    IRReader
    MC
    MCParser
    Target
    CodeGen
    AsmPrinter
    Passes
  )
  target_link_libraries(lorsc PRIVATE ${LLVM_LIBS})
endif()
