#ifndef JARVIS_FFMPEG_H
#define JARVIS_FFMPEG_H

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

// Bloque extern "C" para integración nativa de yt-dlp accesible desde C y C++
bool ytdlp_get_stream_url(const char* query, char* output_buffer, int max_len);

#ifdef __cplusplus
}
#endif

class JarvisFFmpeg {
public:
    JarvisFFmpeg();
    ~JarvisFFmpeg();

    // Reproduce audio en streaming mediante FFmpeg
    bool streamAudio(const std::string& url);

    // Obtiene la URL directa de reproducción usando el helper en C de yt-dlp
    std::string getDirectStreamUrl(const std::string& searchQuery);
};

#endif
