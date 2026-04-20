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

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

bison_target(
  LorsParser
  ${CMAKE_SOURCE_DIR}/src/parser.y
  ${CMAKE_BINARY_DIR}/generated/parser.tab.cpp
  DEFINES_FILE ${CMAKE_BINARY_DIR}/generated/parser.tab.hpp
)

flex_target(
  LorsLexer
  ${CMAKE_SOURCE_DIR}/src/lexer.l
  ${CMAKE_BINARY_DIR}/generated/lexer.yy.cpp
)

add_flex_bison_dependency(LorsLexer LorsParser)

add_executable(
  lorsc
  src/main.cpp
  src/AST.cpp
  src/Diagnostics.cpp
  src/Parser.cpp
  src/Sema.cpp
  src/Codegen.cpp
  ${BISON_LorsParser_OUTPUTS}
  ${FLEX_LorsLexer_OUTPUTS}
)

target_include_directories(
  lorsc
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
    ${LLVM_INCLUDE_DIRS}
)

add_definitions(${LLVM_DEFINITIONS})
target_compile_options(lorsc PRIVATE -Wall -Wextra -Wpedantic)

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
