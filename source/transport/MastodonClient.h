#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>
#include <map>
#include <set>

/**
 * @brief Structure to hold content with its MIME type
 */
struct MastodonContent {
    std::string contentType;  // "text/plain" or "image/jpeg"
    std::vector<uint8_t> data;  // Raw data (text as bytes or image as bytes)
};

/**
 * @brief Simple Mastodon REST API client for posting and searching statuses.
 *
 * This client is initialized with a Mastodon server hostname and an API access token.
 * It provides methods to post a public status (toot) and search for public statuses
 * containing a specific hashtag.
 *
 * All content is assumed to be base64-encoded and posted as plain text.
 */
class MastodonClient {
public:
    MastodonClient(const std::string& server, const std::string& accessToken);
    ~MastodonClient();

    /**
     * @brief Posts a public status (toot) to Mastodon.
     *
     * @param content The base64-encoded content to post as the status text.
     * @param hashtag The hashtag to include for indexing (e.g., "#raceboat_link_123").
     * @return true if the post succeeded, false otherwise.
     */
    bool postStatus(const std::string& content, const std::string& hashtag);

    /**
     * @brief Posts an image to Mastodon as a media attachment with a hashtag.
     *
     * @param imageData The raw JPEG image data as bytes.
     * @param hashtag The hashtag to include for indexing (e.g., "#raceboat_link_123").
     * @return true if the post succeeded, false otherwise.
     */
    bool postImage(const std::vector<uint8_t>& imageData, const std::string& hashtag);

    /**
     * @brief Posts an image with text to Mastodon as a media attachment with custom text and hashtag.
     *
     * @param imageData The raw JPEG image data as bytes.
     * @param text The text content to include in the status.
     * @param hashtag The hashtag to include for indexing (e.g., "#raceboat_link_123").
     * @return true if the post succeeded, false otherwise.
     */
    bool postImageWithText(const std::vector<uint8_t>& imageData, const std::string& text, const std::string& hashtag);

    /**
     * @brief Searches for public statuses containing the given hashtag.
     *
     * @param hashtag The hashtag to search for (e.g., "#raceboat_link_123").
     * @return A vector of MastodonContent objects containing both text and image content.
     */
    std::vector<MastodonContent> searchStatuses(const std::string& hashtag);

private:
    std::string serverUrl;
    std::string accessToken;
    CURL* curl;
    std::map<std::string, std::set<std::string>> seenPostsByHashtag;

    void initCurl();
    void setCommonCurlOptions(CURL* curl, const std::string& url, const std::string& logPrefix);
    struct curl_slist* createAuthHeader(); // Create Authorization header
    std::vector<uint8_t> downloadImage(const std::string& imageUrl);
 };
