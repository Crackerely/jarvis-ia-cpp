#ifndef JARVIS_VOICE_H
#define JARVIS_VOICE_H

#include <string>
#include <vector>
#include <cstdint>
#include <sys/types.h>

class JarvisVoice {
private:
    pid_t active_speech_pid;

public:
    JarvisVoice();
    ~JarvisVoice();

    int executeProcess(const std::string& cmd);
    // Cambiado para aceptar la URL completa directamente
    std::string httpPostOllama(const std::string& url, const std::string& jsonData);
    bool sendMpvCommand(const std::string& socketPath, const std::string& jsonCommand);
    bool streamAudioWithFFmpeg(const std::string& inputSource);
    
    std::vector<int16_t> recordAudioChunk(int durationSeconds);
    void* openAudioStream();
    std::vector<int16_t> readAudioStreamChunk(void* streamHandle, int chunkSize);
    void closeAudioStream(void* streamHandle);

    void stopActiveSpeech();
    char getSingleKey();
};

#endif