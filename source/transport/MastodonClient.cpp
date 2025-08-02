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

// Callback function to write binary data
static size_t WriteBinaryCallback(void* contents, size_t size, size_t nmemb, std::vector<uint8_t>* data) {
    size_t totalSize = size * nmemb;
    
    if (!contents || !data) {
        return 0; // Signal an error to libcurl
    }
    
    try {
        // Reserve additional space if needed
        if (data->capacity() < data->size() + totalSize) {
            data->reserve(data->size() + totalSize + 4096); // Reserve extra space
        }
        
        // Use push_back in a loop instead of insert with iterators
        const uint8_t* byteData = static_cast<const uint8_t*>(contents);
        for (size_t i = 0; i < totalSize; ++i) {
            data->push_back(byteData[i]);
        }
    } catch (const std::exception& e) {
        // Log error if logging is available
        return 0; // Signal an error to libcurl
    }
    
    return totalSize;
}

// Debug callback function
static int CurlDebugCallback(CURL* handle, curl_infotype type, char* data, size_t size, void* userptr) {
    // Cast userptr to the logging prefix or context (if needed)
    std::string* logPrefix = static_cast<std::string*>(userptr);

    // Process the debug information based on its type
    switch (type) {
        case CURLINFO_TEXT:
            logDebug(*logPrefix + "CURL INFO: " + std::string(data, size));
            break;
        case CURLINFO_HEADER_IN:
            logDebug(*logPrefix + "CURL HEADER IN: " + std::string(data, size));
            break;
        case CURLINFO_HEADER_OUT:
            logDebug(*logPrefix + "CURL HEADER OUT: " + std::string(data, size));
            break;
        case CURLINFO_DATA_IN:
            logDebug(*logPrefix + "CURL DATA IN: " + std::to_string(size) + " bytes");
            break;
        case CURLINFO_DATA_OUT:
            logDebug(*logPrefix + "CURL DATA OUT: " + std::to_string(size) + " bytes");
            break;
        default:
            break;
    }

    return 0; // Returning 0 indicates success
}

// Configure CURL to use the debug callback
void configureCurlDebug(CURL* curl, const std::string& logPrefix) {
    // Enable verbose mode
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // Set the debug callback function
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CurlDebugCallback);

    // Pass the log prefix or context to the callback
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &logPrefix);

    // Redirect default verbose output to /dev/null to suppress it
    FILE* devNull = fopen("/dev/null", "w");
    if (devNull) {
        curl_easy_setopt(curl, CURLOPT_STDERR, devNull);
    }
}

MastodonClient::MastodonClient(const std::string& server, const std::string& accessToken)
    : serverUrl(server), accessToken(accessToken) {
    curl = curl_easy_init();
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
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose logging
}

void MastodonClient::setCommonCurlOptions(CURL* curl, const std::string& url, const std::string& logPrefix) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30-second timeout
    configureCurlDebug(curl, logPrefix); // Set debug callback
}

bool MastodonClient::postStatus(const std::string& content, const std::string& hashtag) {
    std::string url = serverUrl + "/api/v1/statuses";
    std::string body = "status=" + content + " " + hashtag + "&visibility=public";

    CURL* postCurl = curl_easy_init();
    if (!postCurl) {
        logError("MastodonClient::postStatus: Failed to initialize CURL");
        return false;
    }

    setCommonCurlOptions(postCurl, url, "MastodonClient::postStatus: ");

    // Add Authorization and Content-Type headers
    struct curl_slist* headers = createAuthHeader();
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(postCurl, CURLOPT_HTTPHEADER, headers);

    // Set POST options
    curl_easy_setopt(postCurl, CURLOPT_POST, 1L);
    curl_easy_setopt(postCurl, CURLOPT_POSTFIELDS, body.c_str());

    // Perform the request
    CURLcode res = curl_easy_perform(postCurl);

    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(postCurl);

    if (res != CURLE_OK) {
        logError("MastodonClient::postStatus: CURL error: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    return true;
}

bool MastodonClient::postImage(const std::vector<uint8_t>& imageData, const std::string& hashtag) {
    std::string mediaUrl = serverUrl + "/api/v1/media";
    std::string mediaId;

    CURL* postCurl = curl_easy_init();
    if (!postCurl) {
        logError("MastodonClient::postImage: Failed to initialize CURL");
        return false;
    }

    setCommonCurlOptions(postCurl, mediaUrl, "MastodonClient::postImage: ");

    // Create multipart form data
    curl_mime* mime = curl_mime_init(postCurl);
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_data(part, reinterpret_cast<const char*>(imageData.data()), imageData.size());
    curl_mime_name(part, "file");
    curl_mime_filename(part, "image.jpg");
    curl_mime_type(part, "image/jpeg");

    // Add Authorization header
    struct curl_slist* headers = createAuthHeader();
    curl_easy_setopt(postCurl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(postCurl, CURLOPT_MIMEPOST, mime);

    // Perform the request
    std::string mediaResponse;
    curl_easy_setopt(postCurl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(postCurl, CURLOPT_WRITEDATA, &mediaResponse);

    CURLcode res = curl_easy_perform(postCurl);

    // Clean up
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(postCurl);

    if (res != CURLE_OK) {
        logError("MastodonClient::postImage: CURL error: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    // Parse media response to get media ID
    try {
        auto mediaJson = nlohmann::json::parse(mediaResponse);
        if (mediaJson.contains("id")) {
            mediaId = mediaJson["id"].get<std::string>();
        } else {
            logError("MastodonClient::postImage: Media upload failed: no ID in response");
            return false;
        }
    } catch (const std::exception& e) {
        logError("MastodonClient::postImage: Error parsing media response: " + std::string(e.what()));
        return false;
    }

    // Use mediaId to post the status (similar to postStatus)
    
    // return postStatus("#" + hashtag, mediaId);

    std::string statusUrl = serverUrl + "/api/v1/statuses";
    std::string statusBody = "status=" + hashtag + "&visibility=public&media_ids[]=" + mediaId;    

    CURL* curl = curl_easy_init();
    if (!curl) {
        logError("MastodonClient::postStatus: Failed to initialize CURL");
        return false;
    }

    setCommonCurlOptions(curl, statusUrl, "MastodonClient::postStatus: ");

    // Add Authorization and Content-Type headers
    struct curl_slist* statusHeaders = createAuthHeader();
    statusHeaders = curl_slist_append(statusHeaders, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, statusHeaders);

    // Set POST options
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, statusBody.c_str());

    // Perform the request
    CURLcode statusRes = curl_easy_perform(curl);

    // Clean up
    curl_slist_free_all(statusHeaders);
    curl_easy_cleanup(curl);

    if (statusRes != CURLE_OK) {
        logError("MastodonClient::postStatus: CURL error: " + std::string(curl_easy_strerror(statusRes)));
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
        logDebug("MastodonClient::searchStatuses: parsing response");

        auto jsonResponse = nlohmann::json::parse(responseString);

        // Extract statuses from the JSON response
        if (jsonResponse.contains("statuses")) {
            for (const auto& status : jsonResponse["statuses"]) {
                 logDebug("MastodonClient::searchStatuses: parsing status");

                // Check if this status has media attachments (images)
                if (status.contains("media_attachments") && 
                    status["media_attachments"].is_array() && 
                    !status["media_attachments"].empty()) {
                    
                    logDebug("MastodonClient::searchStatuses: has attachment");
                    // Process media attachments
                    for (const auto& media : status["media_attachments"]) {
                        if (media.contains("type") && media["type"].get<std::string>() == "image") {
                            if (media.contains("url")) {
                                std::string imageUrl = media["url"].get<std::string>();
                                logDebug("MastodonClient::searchStatuses: caling downlaod for url: " + imageUrl);
                    
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
    logDebug("MastodonClient::downloadImage: called: " + imageUrl);

    CURL* imgCurl = curl_easy_init();
    if (imgCurl) {
        std::string logPrefix = "MastodonClient::downloadImage: ";
        setCommonCurlOptions(imgCurl, imageUrl, logPrefix);

        // Set binary write callback
        curl_easy_setopt(imgCurl, CURLOPT_WRITEFUNCTION, WriteBinaryCallback);
        curl_easy_setopt(imgCurl, CURLOPT_WRITEDATA, &imageData);

        // Add Authorization header
        struct curl_slist* headers = createAuthHeader();
        curl_easy_setopt(imgCurl, CURLOPT_HTTPHEADER, headers);

        // Perform the request
        logDebug("MastodonClient::downloadImage: performing curl");
        CURLcode res = curl_easy_perform(imgCurl);

        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(imgCurl);

        if (res != CURLE_OK) {
            logError("MastodonClient::downloadImage: CURL error: " + std::string(curl_easy_strerror(res)));
        }
    }

    return imageData;
}

struct curl_slist* MastodonClient::createAuthHeader() {
    std::string authHeader = "Authorization: Bearer " + accessToken;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.c_str());
    return headers;
}
