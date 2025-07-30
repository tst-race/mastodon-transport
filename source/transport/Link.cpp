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

#include "Link.h"
#include <string>
#include <sstream>

Link::Link(const LinkID& id,
           const LinkAddress& addr,
           const LinkProperties& props,
           ITransportSdk* sdk,
           MastodonClient* mastodonClient)
    : linkId(id),
      address(addr),
      properties(props),
      sdk(sdk),
      mastodonClient(mastodonClient),
      logPrefix("[Link " + id + "] ") {
    this->properties.linkAddress = nlohmann::json(this->address).dump();
}

void Link::start() {
    // Any startup logic if needed
}

void Link::shutdown() {
    // Any shutdown/cleanup logic if needed
}

LinkID Link::getId() const {
    return linkId;
}


const LinkProperties& Link::getProperties() const {
    return properties;
}

ComponentStatus Link::enqueueContent(uint64_t actionId, const std::vector<uint8_t>& content, const std::string& contentType) {
    if (contentType == "text/plain") {
        contentQueue[actionId].textContent = content;
        contentQueue[actionId].hasText = true;
        logDebug(logPrefix + "Enqueued text content for action " + std::to_string(actionId));
    } else if (contentType == "image/jpeg") {
        contentQueue[actionId].imageContent = content;
        contentQueue[actionId].hasImage = true;
        logDebug(logPrefix + "Enqueued image content for action " + std::to_string(actionId));
    } else {
        logError(logPrefix + "Unknown content type: " + contentType);
        return COMPONENT_ERROR;
    }
    return COMPONENT_OK;
}

ComponentStatus Link::dequeueContent(uint64_t actionId) {
    contentQueue.erase(actionId);
    return COMPONENT_OK;
}

ComponentStatus Link::post(const std::vector<RaceHandle>& handles, uint64_t actionId) {
    TRACE_METHOD(linkId, handles, actionId);

    auto iter = contentQueue.find(actionId);
    if (iter == contentQueue.end()) {
        logInfo(logPrefix + "No enqueued content for action ID: " + std::to_string(actionId));
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return COMPONENT_OK;
    }

    const ActionContent& content = iter->second;
    std::string hashtag = "#" + address.hashtag;
    bool success = false;

    if (content.hasImage && content.hasText) {
        // Post both text and image together
        logDebug(logPrefix + "Posting mixed content (text + image) to Mastodon");
        std::string textStr(content.textContent.begin(), content.textContent.end());
        success = mastodonClient->postImageWithText(content.imageContent, textStr, hashtag);
    } else if (content.hasImage) {
        // Post image only
        logDebug(logPrefix + "Posting image content to Mastodon");
        success = mastodonClient->postImage(content.imageContent, hashtag);
    } else if (content.hasText) {
        // Post text only
        logDebug(logPrefix + "Posting text content to Mastodon");
        std::string textStr(content.textContent.begin(), content.textContent.end());
        success = mastodonClient->postStatus(textStr, hashtag);
    } else {
        logError(logPrefix + "No content to post for action ID: " + std::to_string(actionId));
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return COMPONENT_ERROR;
    }

    if (success) {
        updatePackageStatus(handles, PACKAGE_SENT);
        contentQueue.erase(actionId);
        return COMPONENT_OK;
    } else {
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return COMPONENT_ERROR;
    }
}

ComponentStatus Link::fetch() {
    TRACE_METHOD(linkId);

    std::string hashtag = "#" + address.hashtag;
    auto results = mastodonClient->searchStatuses(hashtag);

    logInfo(logPrefix + "Fetched " + std::to_string(results.size()) + " items for hashtag " + hashtag);

    // Group content by status (assumes all content from same status should be grouped)
    // For mixed content, we need to ensure proper ordering: text first, then image
    std::vector<MastodonContent> textContent;
    std::vector<MastodonContent> imageContent;
    
    // Separate text and image content while preserving retrieval order
    for (const auto& content : results) {
        if (content.contentType == "text/plain") {
            textContent.push_back(content);
        } else if (content.contentType == "image/jpeg") {
            imageContent.push_back(content);
        }
    }
    
    // Call sdk->onReceive in the same order as getActionParams: text first, then images
    // This ensures proper fragment ordering for message reconstruction
    
    // First, send all text content
    for (const auto& content : textContent) {
        logInfo(logPrefix + "Fetched text content, size: " + std::to_string(content.data.size()));
        sdk->onReceive(linkId, {linkId, content.contentType, false, {}}, content.data);
    }
    
    // Then, send all image content  
    for (const auto& content : imageContent) {
        logInfo(logPrefix + "Fetched image content, size: " + std::to_string(content.data.size()));
        sdk->onReceive(linkId, {linkId, content.contentType, false, {}}, content.data);
    }

    return COMPONENT_OK;
}

void Link::updatePackageStatus(const std::vector<RaceHandle>& handles, PackageStatus status) {
    for (const auto& handle : handles) {
        sdk->onPackageStatusChanged(handle, status);
    }
}
