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
    
    // Método exclusivo para reproducir una URL directa (sin sobrecarga ambigua)
    bool streamAudioFromUrl(const std::string& url);
    
    // Método exclusivo para aceptar metadatos completos de la LinkedList
    bool streamAudioWithMetadata(const std::string& title, const std::string& artist, const std::string& url, double duration);

    // Método nativo utilizando libavformat para exportar PCM a WAV
    bool savePcmToWav(const std::vector<int16_t>& pcmData, const std::string& filename, int sampleRate = 16000);
    
    // Getters de control y progreso para la lista enlazada
    std::string getCurrentSongTitle();
    std::string getCurrentSongArtist();
    double getCurrentSongProgress();
    double getCurrentSongDuration();
    bool controlMusic(const std::string& command); // "stop", "pause", "resume"
    int getQueueSize();
};

#endif // JARVIS_FFMPEG_H