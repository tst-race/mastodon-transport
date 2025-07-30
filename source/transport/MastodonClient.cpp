#include "MastodonClient.h"
// #include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include "log.h"
#include <stdexcept>
#include <iostream>
#include <curl/curl.h> // For curl_easy_escape
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLtree.h>

// Callback function to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

MastodonClient::MastodonClient(const std::string& server, const std::string& accessToken)
    : serverUrl(server), accessToken(accessToken), curl(curl_easy_init()) {
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    initCurl();
}

MastodonClient::~MastodonClient() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

void MastodonClient::initCurl() {
    // Set common CURL options
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    // curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt"); // Set CA certificate path
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose logging
}

bool MastodonClient::postStatus(const std::string& content, const std::string& hashtag) {
    std::string url = serverUrl + "/api/v1/statuses";
    std::string body = "status=" + content + " " + hashtag + "&visibility=public";

    // Create the Authorization header
    std::string authHeader = "Authorization: Bearer " + accessToken;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Clean up headers
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return false;
    }

    return true;
}

bool MastodonClient::postImage(const std::vector<uint8_t>& imageData, const std::string& hashtag) {
    // First, upload the media to get a media ID
    std::string mediaUrl = serverUrl + "/api/v1/media";
    std::string mediaId;
    
    // Create multipart form data for image upload
    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    
    // Set the image data
    curl_mime_data(part, reinterpret_cast<const char*>(imageData.data()), imageData.size());
    curl_mime_name(part, "file");
    curl_mime_filename(part, "image.jpg");
    curl_mime_type(part, "image/jpeg");
    
    // Authorization header for media upload
    std::string authHeader = "Authorization: Bearer " + accessToken;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    
    // Capture media upload response
    std::string mediaResponse;
    curl_easy_setopt(curl, CURLOPT_URL, mediaUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mediaResponse);
    
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up mime and headers
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error during media upload: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    // Parse media response to get media ID
    try {
        auto mediaJson = nlohmann::json::parse(mediaResponse);
        if (mediaJson.contains("id")) {
            mediaId = mediaJson["id"].get<std::string>();
        } else {
            std::cerr << "Media upload failed: no ID in response" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing media upload response: " << e.what() << std::endl;
        return false;
    }
    
    // Now create a status with the media attachment
    std::string statusUrl = serverUrl + "/api/v1/statuses";
    std::string statusBody = "status=" + hashtag + "&visibility=public&media_ids[]=" + mediaId;
    
    // Create headers for status post
    headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    
    // Reset curl options for status post
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, statusUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, statusBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
    
    // Perform the status post request
    res = curl_easy_perform(curl);
    
    // Clean up headers
    curl_slist_free_all(headers);
    
    // Reinitialize curl for future use
    initCurl();
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error during status post: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    return true;
}

bool MastodonClient::postImageWithText(const std::vector<uint8_t>& imageData, const std::string& text, const std::string& hashtag) {
    // First, upload the media to get a media ID
    std::string mediaUrl = serverUrl + "/api/v1/media";
    std::string mediaId;
    
    // Create multipart form data for image upload
    curl_mime *mime = curl_mime_init(curl);
    curl_mimepart *part = curl_mime_addpart(mime);
    
    // Set the image data
    curl_mime_data(part, reinterpret_cast<const char*>(imageData.data()), imageData.size());
    curl_mime_name(part, "file");
    curl_mime_filename(part, "image.jpg");
    curl_mime_type(part, "image/jpeg");
    
    // Authorization header for media upload
    std::string authHeader = "Authorization: Bearer " + accessToken;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    
    // Capture media upload response
    std::string mediaResponse;
    curl_easy_setopt(curl, CURLOPT_URL, mediaUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mediaResponse);
    
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up mime and headers
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error during media upload: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    // Parse media response to get media ID
    try {
        auto mediaJson = nlohmann::json::parse(mediaResponse);
        if (mediaJson.contains("id")) {
            mediaId = mediaJson["id"].get<std::string>();
        } else {
            std::cerr << "Media upload failed: no ID in response" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing media upload response: " << e.what() << std::endl;
        return false;
    }
    
    // Now create a status with both text and media attachment
    std::string statusUrl = serverUrl + "/api/v1/statuses";
    std::string statusBody = "status=" + text + " " + hashtag + "&visibility=public&media_ids[]=" + mediaId;
    
    // Create headers for status post
    headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    
    // Reset curl options for status post
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, statusUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, statusBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
    
    // Perform the status post request
    res = curl_easy_perform(curl);
    
    // Clean up headers
    curl_slist_free_all(headers);
    
    // Reinitialize curl for future use
    initCurl();
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error during status post: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    return true;
}

// Helper function to strip HTML tags using libxml2
std::string stripHtmlWithLibxml2(const std::string& html) {
    // Parse the HTML content
    htmlDocPtr doc = htmlReadMemory(html.c_str(), html.size(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
    if (!doc) {
        throw std::runtime_error("Failed to parse HTML content");
    }

    // Extract plain text
    xmlChar* plainText = xmlNodeGetContent(xmlDocGetRootElement(doc));
    std::string result;
    if (plainText) {
        result = reinterpret_cast<const char*>(plainText);
        xmlFree(plainText);
    }

    // Free the document
    xmlFreeDoc(doc);
    return result;
}

std::vector<MastodonContent> MastodonClient::searchStatuses(const std::string& hashtag) {
    std::vector<MastodonContent> results;

    // Validate the hashtag parameter
    if (hashtag.empty()) {
        std::cerr << "Error: Hashtag parameter is empty." << std::endl;
        return results;
    }

    // URL-encode the hashtag
    char* encodedHashtag = curl_easy_escape(curl, hashtag.c_str(), hashtag.length());
    if (!encodedHashtag) {
        std::cerr << "Error: Failed to URL-encode the hashtag." << std::endl;
        return results;
    }

    // Construct the URL with the encoded hashtag
    std::string url = serverUrl + "/api/v2/search?q=" + encodedHashtag + "&type=statuses&resolve=true";
    logDebug("MastodonClient::searchStatuses: Searching for hashtag: " + url);

    // Free the encoded string after use
    curl_free(encodedHashtag);

    // Create the Authorization header
    std::string authHeader = "Authorization: Bearer " + accessToken;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());

    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

    // Capture the response
    std::string responseString;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    // Clean up headers
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return results;
    }

    // Parse the JSON response
    try {
        auto jsonResponse = nlohmann::json::parse(responseString);

        // Extract statuses from the JSON response
        if (jsonResponse.contains("statuses")) {
            for (const auto& status : jsonResponse["statuses"]) {
                
                // Check if this status has media attachments (images)
                if (status.contains("media_attachments") && 
                    status["media_attachments"].is_array() && 
                    !status["media_attachments"].empty()) {
                    
                    // Process media attachments
                    for (const auto& media : status["media_attachments"]) {
                        if (media.contains("type") && media["type"].get<std::string>() == "image") {
                            if (media.contains("url")) {
                                std::string imageUrl = media["url"].get<std::string>();
                                
                                // Download the image
                                std::vector<uint8_t> imageData = downloadImage(imageUrl);
                                if (!imageData.empty()) {
                                    // Store image data directly as raw bytes
                                    MastodonContent content;
                                    content.contentType = "image/jpeg";
                                    content.data = std::move(imageData);
                                    results.push_back(content);
                                    logDebug("MastodonClient::searchStatuses: Downloaded image, size: " + std::to_string(content.data.size()));
                                }
                            }
                        }
                    }
                }
                
                // Also process text content if available
                if (status.contains("content")) {
                    std::string rawContent = status["content"].get<std::string>();

                    // Strip HTML tags using libxml2
                    std::string plainTextContent = stripHtmlWithLibxml2(rawContent);

                    // Remove the hashtag from text content
                    size_t pos = plainTextContent.find(" " + hashtag);
                    if (pos != std::string::npos) {
                        plainTextContent = plainTextContent.substr(0, pos);
                    }

                    // Only add non-empty text content
                    if (!plainTextContent.empty() && plainTextContent != hashtag) {
                        MastodonContent content;
                        content.contentType = "text/plain";
                        content.data = std::vector<uint8_t>(plainTextContent.begin(), plainTextContent.end());
                        results.push_back(content);
                    }
                }
            }
        } else {
            std::cerr << "Error: 'statuses' field not found in the response." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
    }

    return results;
}

std::vector<uint8_t> MastodonClient::downloadImage(const std::string& imageUrl) {
    std::vector<uint8_t> imageData;
    
    // Create a new CURL handle for image download
    CURL* imgCurl = curl_easy_init();
    if (!imgCurl) {
        std::cerr << "Failed to initialize CURL for image download" << std::endl;
        return imageData;
    }
    
    // Set up curl for binary data download
    curl_easy_setopt(imgCurl, CURLOPT_URL, imageUrl.c_str());
    curl_easy_setopt(imgCurl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(imgCurl, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, std::vector<uint8_t>* data) -> size_t {
        size_t totalSize = size * nmemb;
        data->insert(data->end(), static_cast<uint8_t*>(contents), static_cast<uint8_t*>(contents) + totalSize);
        return totalSize;
    });
    curl_easy_setopt(imgCurl, CURLOPT_WRITEDATA, &imageData);
    curl_easy_setopt(imgCurl, CURLOPT_TIMEOUT, 30L); // 30 second timeout
    
    CURLcode res = curl_easy_perform(imgCurl);
    curl_easy_cleanup(imgCurl);
    
    if (res != CURLE_OK) {
        std::cerr << "CURL error during image download: " << curl_easy_strerror(res) << std::endl;
        imageData.clear();
    }
    
    return imageData;
}

std::string MastodonClient::base64Encode(const std::vector<uint8_t>& data) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}
