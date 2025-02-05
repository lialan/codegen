#
# Copyright © 2019 Paweł Dziepak
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.12)

project(codegen
  LANGUAGES CXX
)

option(CODEGEN_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer." ON)

list(APPEND CODEGEN_CXX_FLAGS -Wall -Wextra -Wno-unused-parameter)
if (CODEGEN_SANITIZERS)
  list(APPEND CODEGEN_CXX_DEBUG_FLAGS -fsanitize=address -fsanitize=undefined)
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  list(APPEND CODEGEN_CXX_FLAGS ${CODEGEN_CXX_DEBUG_FLAGS})
else()
  list(APPEND CODEGEN_CXX_FLAGS ${CODEGEN_CXX_RELEASE_FLAGS})
endif()

set(CODEGEN_CXX_FILESYSTEM stdc++fs)
if(APPLE)
  set(CODEGEN_CXX_FILESYSTEM c++fs)
endif()

find_package(LLVM 12 REQUIRED)
find_package(fmt REQUIRED)

add_library(codegen INTERFACE)

target_include_directories(codegen INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  ${LLVM_INCLUDE_DIRS}
)

target_compile_features(codegen INTERFACE cxx_std_20)
target_link_libraries(codegen INTERFACE ${CODEGEN_CXX_FLAGS})
target_compile_options(codegen INTERFACE ${CODEGEN_CXX_FLAGS})
target_link_libraries(codegen INTERFACE
  LLVMCore
  LLVMX86CodeGen
  LLVMOrcJIT
  LLVMSupport
  fmt::fmt
  ${CODEGEN_CXX_FILESYSTEM}
)

add_subdirectory(tests)

