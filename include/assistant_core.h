#ifndef ASSISTANT_CORE_H
#define ASSISTANT_CORE_H

#include "jarvis_voice.h"
#include "jarvis_ffmpeg.h"
#include "jarvis_opencv.h"
#include "jarvis_melo.h"
#include <string>

class AssistantCore {
private:
    JarvisVoice voice;
    JarvisFFmpeg ffmpeg;
    JarvisOpenCV opencv;
    JarvisMelo meloTTS;

public:
    AssistantCore();
    ~AssistantCore();

    void speak(const std::string& text, const std::string& lang = "es");
    bool controlMusic(const std::string& command);
    char checkKey();
    std::string queryOllama(const std::string& prompt);
    std::string queryOllamaIntent(const std::string& userText);
    void recordAndSaveWav(const std::string& filename, int seconds);
    bool takeSnapshot(const std::string& filename);

    // Nuevo flujo nativo: assistant_core -> yt-dlp -> ffmpeg
    bool playYouTubeMusic(const std::string& query);
};

#endif
