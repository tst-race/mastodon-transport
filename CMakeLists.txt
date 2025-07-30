# 
# Copyright 2023 Two Six Technologies
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 

cmake_minimum_required(VERSION 3.20)
project(decomposed-exemplars LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

set(BUILD_VERSION "" CACHE STRING "Version of the plugin being built")
if("${BUILD_VERSION}" STREQUAL "")
    message(SEND_ERROR "Missing or empty BUILD_VERSION argument")
endif()

# include(race/clang-format)
# include(race/coverage)
# include(race/lint)
# include(race/test-targets)
# include(race/valgrind)
# include(race/warnings)
# FetchContent for dependencies
include(FetchContent)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# # cpr (HTTP client)
# FetchContent_Declare(
#   cpr
#   GIT_REPOSITORY https://github.com/libcpr/cpr.git
#   GIT_TAG 1.10.5
# )
# set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # Ensure -fPIC is used
# set(CPR_FORCE_USE_SYSTEM_CURL OFF)      # Ensure cpr builds its own curl
# set(BUILD_SHARED_LIBS OFF)              # Build cpr as a static library
# FetchContent_MakeAvailable(cpr)

# FetchContent_Declare(
#   curl
#   GIT_REPOSITORY https://github.com/curl/curl.git
#   GIT_TAG curl-7_88_1
# )
# set(CMAKE_POSITION_INDEPENDENT_CODE ON) 
# set(BUILD_SHARED_LIBS OFF)  # Build curl as a static library

# # Enable SSL support with OpenSSL
# set(CURL_USE_OPENSSL ON CACHE BOOL "Use OpenSSL for SSL/TLS")
# set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" CACHE PATH "Path to OpenSSL installation") # Adjust as needed
# set(OPENSSL_USE_STATIC_LIBS TRUE CACHE BOOL "Link OpenSSL statically")

# FetchContent_MakeAvailable(curl)
# Fetch Gumbo HTML parser
FetchContent_Declare(
  libxml2
  GIT_REPOSITORY https://gitlab.gnome.org/GNOME/libxml2.git
  GIT_TAG v2.11.5 # Use the latest stable release or a specific version
)
set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "Disable Python bindings")
set(LIBXML2_WITH_LZMA OFF CACHE BOOL "Disable LZMA support")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libxml2 as a static library")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Ensure -fPIC is used")
FetchContent_MakeAvailable(libxml2)

add_subdirectory(source)
