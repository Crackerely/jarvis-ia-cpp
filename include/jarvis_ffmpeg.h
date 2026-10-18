#ifndef JARVIS_FFMPEG_H
#define JARVIS_FFMPEG_H

#include <string>
#include <vector>
#include <cstdint>
#include "memory_player.h"

class JarvisFFmpeg {
private:
    MemoryMusicPlayer memoryPlayer;

public:
    JarvisFFmpeg();
    ~JarvisFFmpeg();

    std::string getDirectStreamUrl(const std::string& searchQuery);
    bool streamAudio(const std::string& url);

    // Método nativo utilizando libavformat para exportar PCM a WAV sin popen ni cabeceras manuales
    bool savePcmToWav(const std::vector<int16_t>& pcmData, const std::string& filename, int sampleRate = 16000);
};

#endif // JARVIS_FFMPEG_H