#include "jarvis_ffmpeg.h"
#include <iostream>
#include <cstdio>
#include <memory>
#include <array>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

bool ytdlp_get_stream_url(const char* query, char* output_buffer, int max_len) {
    if (!query || !output_buffer || max_len <= 0) return false;

    // Comando optimizado de yt-dlp para extraer únicamente el enlace directo de audio
    std::string cmd = "yt-dlp --get-url --format bestaudio --default-search ytsearch1 \"" + std::string(query) + "\" 2>/dev/null";

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return false;
    }

    std::array<char, 256> buffer;
    std::string result = "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Limpiar saltos de línea residuales
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    if (result.empty() || result.find("http") == std::string::npos) {
        return false;
    }

    if (static_cast<int>(result.size()) >= max_len) {
        return false; // El buffer es demasiado pequeño para la URL
    }

    std::snprintf(output_buffer, max_len, "%s", result.c_str());
    return true;
}

#ifdef __cplusplus
}
#endif

JarvisFFmpeg::JarvisFFmpeg() {}
JarvisFFmpeg::~JarvisFFmpeg() {}

bool JarvisFFmpeg::streamAudio(const std::string& url) {
    std::string cmd = "ffmpeg -re -i " + url + " -f alsa default -loglevel quiet &";
    return std::system(cmd.c_str()) == 0;
}

std::string JarvisFFmpeg::getDirectStreamUrl(const std::string& searchQuery) {
    char buffer[1024];
    if (ytdlp_get_stream_url(searchQuery.c_str(), buffer, sizeof(buffer))) {
        return std::string(buffer);
    }
    return "";
}
