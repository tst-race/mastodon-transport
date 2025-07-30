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

################################################################################
# Transport component
################################################################################

set(LIB_DEPS
    curl
    nlohmann_json::nlohmann_json
    xml2
    # Add other dependencies here if needed
)

setup_component_target(
    TARGET PluginMastodon
    SOURCES
	../common/base64.cpp
        Link.cpp
        LinkAddress.cpp
        LinkMap.cpp
        MessageHashQueue.cpp
        MastodonClient.cpp
        PluginMastodon.cpp
        ../common/log.cpp
)

# Add libxml2 include directories
target_include_directories(PluginMastodon PRIVATE 
    ${libxml2_SOURCE_DIR}/include
    ${libxml2_BINARY_DIR}
)
