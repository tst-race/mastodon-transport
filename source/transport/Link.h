//
// Copyright 2023 Two Six Technologies
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "LinkAddress.h"
#include "LinkProperties.h"
#include "ITransportSdk.h"
#include "MastodonClient.h"
#include "log.h"

/**
 * @brief Structure to hold content with its type for mixed posting
 */
struct ActionContent {
    std::vector<uint8_t> textContent;
    std::vector<uint8_t> imageContent;
    bool hasText = false;
    bool hasImage = false;
};

class Link {
public:
    Link(const LinkID& id,
         const LinkAddress& addr,
         const LinkProperties& props,
         ITransportSdk* sdk,
         MastodonClient* mastodonClient);

    void start();
    void shutdown();
    
    /**
     * @brief Get the ID of this link. This function is thread-safe.
     *
     * @return The ID of this link
     */
    virtual LinkID getId() const;


    const LinkProperties& getProperties() const;

    // Enqueue content for a POST action with content type
    ComponentStatus enqueueContent(uint64_t actionId, const std::vector<uint8_t>& content, const std::string& contentType);

    // Remove content for a POST action
    ComponentStatus dequeueContent(uint64_t actionId);

    // Post content as a Mastodon toot with a unique hashtag
    ComponentStatus post(const std::vector<RaceHandle>& handles, uint64_t actionId);

    // Fetch Mastodon toots with the link's unique hashtag
    ComponentStatus fetch();

private:
    LinkID linkId;
    LinkAddress address;
    LinkProperties properties;
    ITransportSdk* sdk;
    MastodonClient* mastodonClient;
    std::string logPrefix;

    // Maps actionId to mixed content (text and/or image)
    std::unordered_map<uint64_t, ActionContent> contentQueue;

    void updatePackageStatus(const std::vector<RaceHandle>& handles, PackageStatus status);
};