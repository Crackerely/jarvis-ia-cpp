#include "jarvis_downloader.h"
#include <iostream>
#include <curl/curl.h>
#include <string>
#include <algorithm>
#include <fstream>
#include <json-c/json.h>

JarvisDownloader::JarvisDownloader() {}
JarvisDownloader::~JarvisDownloader() {}

size_t JarvisDownloader::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

static std::string urlEncode(const std::string& str) {
    std::string encoded = "";
    for (char c : str) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            encoded += buf;
        }
    }
    return encoded;
}

static std::string extractVideoId(const std::string& input) {
    if (input.find("watch?v=") != std::string::npos) {
        size_t pos = input.find("watch?v=");
        return input.substr(pos + 8, 11);
    } else if (input.find("youtu.be/") != std::string::npos) {
        size_t pos = input.find("youtu.be/");
        return input.substr(pos + 9, 11);
    } else if (input.length() == 11 && input.find(' ') == std::string::npos) {
        return input;
    }
    return "";
}

std::string JarvisDownloader::getAudioUrl(const std::string& searchQuery) {
    std::string videoId = extractVideoId(searchQuery);

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[JarvisDownloader ERROR] No fue posible inicializar cURL." << std::endl;
        return "";
    }

    if (videoId.empty()) {
        videoId = searchQuery;
    }

    std::cout << "[JarvisDownloader] Obteniendo stream de audio para videoId/Query: " << videoId << std::endl;

    std::string playerBuffer;
    std::string playerPayload = "{\"context\":{\"client\":{\"clientName\":\"ANDROID\",\"clientVersion\":\"20.10.38\",\"hl\":\"es\",\"gl\":\"MX\"}},\"videoId\":\"" + videoId + "\"}";

    curl_easy_setopt(curl, CURLOPT_URL, "https://www.youtube.com/youtubei/v1/player");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, playerPayload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &playerBuffer);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[JarvisDownloader ERROR] Falló la petición al reproductor de InnerTube." << std::endl;
        return "";
    }

    struct json_object* root = json_tokener_parse(playerBuffer.c_str());
    if (!root) {
        std::cerr << "[JarvisDownloader ERROR] No se pudo parsear el JSON de respuesta de InnerTube." << std::endl;
        return "";
    }

    std::string directUrl = "";
    struct json_object* streamingData = NULL;

    if (json_object_object_get_ex(root, "streamingData", &streamingData)) {
        struct json_object* formats = NULL;
        if (json_object_object_get_ex(streamingData, "formats", &formats) && json_object_is_type(formats, json_type_array)) {
            int arrayLen = json_object_array_length(formats);
            for (int i = 0; i < arrayLen; i++) {
                struct json_object* fmt = json_object_array_get_idx(formats, i);
                struct json_object* itagObj = NULL;
                
                if (json_object_object_get_ex(fmt, "itag", &itagObj)) {
                    int itagVal = json_object_get_int(itagObj);
                    if (itagVal == 18) {
                        struct json_object* urlObj = NULL;
                        if (json_object_object_get_ex(fmt, "url", &urlObj)) {
                            directUrl = json_object_get_string(urlObj);
                            break;
                        }
                    }
                }
            }
        }
    }

    json_object_put(root);
    return directUrl;
}

std::string JarvisDownloader::findUrlFromPrompt(const std::string& prompt) {
    std::string query = prompt;
    
    // Si ya es una URL completa o un ID directo de 11 caracteres, lo tratamos directo
    if (query.find("watch?v=") != std::string::npos || query.find("youtu.be/") != std::string::npos || query.length() == 11) {
        return getAudioUrl(query);
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[JarvisDownloader ERROR] No fue posible inicializar cURL para la búsqueda." << std::endl;
        return "";
    }

    std::string searchBuffer;
    std::string searchUrl = "https://www.youtube.com/results?search_query=" + urlEncode(query);

    curl_easy_setopt(curl, CURLOPT_URL, searchUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &searchBuffer);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/119.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[JarvisDownloader ERROR] Falló la petición de búsqueda web en YouTube." << std::endl;
        return "";
    }

    std::string videoId = "";
    size_t vIdPos = searchBuffer.find("\"videoId\":\"");
    if (vIdPos != std::string::npos) {
        videoId = searchBuffer.substr(vIdPos + 11, 11);
    }

    if (videoId.empty()) {
        std::cerr << "[JarvisDownloader ERROR] No se encontró ningún videoId para la consulta: " << query << std::endl;
        return "";
    }

    std::cout << "[JarvisDownloader SUCCESS] Primer resultado encontrado -> VideoId: " << videoId << std::endl;
    return getAudioUrl(videoId);
}