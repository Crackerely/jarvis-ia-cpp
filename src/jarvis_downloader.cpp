#include "jarvis_downloader.h"
#include <iostream>
#include <curl/curl.h>
#include <string>
#include <algorithm>
#include <fstream>
#include <json-c/json.h> // <--- Incluimos json-c

JarvisDownloader::JarvisDownloader() {}
JarvisDownloader::~JarvisDownloader() {}

size_t JarvisDownloader::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
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

    // 1. Búsqueda en InnerTube si no es un ID directo
    if (videoId.empty()) {
        std::cout << "[JarvisDownloader] Buscando video en InnerTube para: " << searchQuery << std::endl;

        std::string searchBuffer;
        std::string searchPayload = "{\"context\":{\"client\":{\"clientName\":\"ANDROID\",\"clientVersion\":\"20.10.38\"}},\"query\":\"" + searchQuery + "\"}";

        curl_easy_setopt(curl, CURLOPT_URL, "https://www.youtube.com/youtubei/v1/search");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, searchPayload.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &searchBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "[JarvisDownloader ERROR] Falló la petición de búsqueda." << std::endl;
            curl_easy_cleanup(curl);
            return "";
        }

        struct json_object* searchRoot = json_tokener_parse(searchBuffer.c_str());
        if (searchRoot) {
            // Nota: Aquí también podrías navegar de forma robusta con json-c si lo deseas, 
            // o mantener la extracción rápida del primer videoId.
            size_t vIdPos = searchBuffer.find("\"videoId\":\"");
            if (vIdPos != std::string::npos) {
                videoId = searchBuffer.substr(vIdPos + 11, 11);
            }
            json_object_put(searchRoot);
        }

        if (videoId.empty()) {
            std::cerr << "[JarvisDownloader ERROR] No se encontró ningún videoId para la consulta." << std::endl;
            curl_easy_cleanup(curl);
            return "";
        }
    }

    std::cout << "[JarvisDownloader] Obteniendo stream de audio para videoId: " << videoId << std::endl;

    // 2. Consultar el endpoint /player de InnerTube
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

    // Guardar debug
    {
        std::ofstream debugFile("embed_debug.json");
        if (debugFile.is_open()) {
            debugFile << playerBuffer;
            debugFile.close();
        }
    }

    // 3. Extracción robusta mediante json-c (Adiós a las adivinanzas por subcadenas)
    struct json_object* root = json_tokener_parse(playerBuffer.c_str());
    if (!root) {
        std::cerr << "[JarvisDownloader ERROR] No se pudo parsear el JSON de respuesta de InnerTube." << std::endl;
        return "";
    }

    std::string directUrl = "";
    struct json_object* streamingData = NULL;

    if (json_object_object_get_ex(root, "streamingData", &streamingData)) {
        struct json_object* formats = NULL;
        // YouTube guarda los formatos progresivos en "formats" y los adaptativos en "adaptiveFormats"
        if (json_object_object_get_ex(streamingData, "formats", &formats) && json_object_is_type(formats, json_type_array)) {
            int arrayLen = json_object_array_length(formats);
            for (int i = 0; i < arrayLen; i++) {
                struct json_object* fmt = json_object_array_get_idx(formats, i);
                struct json_object* itagObj = NULL;
                
                if (json_object_object_get_ex(fmt, "itag", &itagObj)) {
                    int itagVal = json_object_get_int(itagObj);
                    if (itagVal == 18) { // Buscamos explícitamente el itag 18
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

    // Liberar memoria principal del árbol JSON
    json_object_put(root);

    if (directUrl.empty()) {
        std::cerr << "[JarvisDownloader ERROR] No se pudo encontrar una URL válida para el itag 18 usando json-c." << std::endl;
        return "";
    }

    std::cout << "[JarvisDownloader SUCCESS] URL de itag 18 extraída limpiamente con json-c." << std::endl;
    return directUrl;
}