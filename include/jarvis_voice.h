#ifndef JARVIS_VOICE_H
#define JARVIS_VOICE_H

#include <string>
#include <vector>
#include <cstdint>
#include <sys/types.h>

class JarvisVoice {
private:
    pid_t active_speech_pid; // Variable de instancia protegida dentro de la clase

public:
    JarvisVoice();
    ~JarvisVoice();

    int executeProcess(const std::string& cmd);
    std::string httpPostOllama(const std::string& host, int port, const std::string& endpoint, const std::string& jsonData);
    bool sendMpvCommand(const std::string& socketPath, const std::string& jsonCommand);
    bool streamAudioWithFFmpeg(const std::string& inputSource);
    
    // Grabación de audio
    std::vector<int16_t> recordAudioChunk(int durationSeconds);
    void* openAudioStream();
    std::vector<int16_t> readAudioStreamChunk(void* streamHandle, int chunkSize);
    void closeAudioStream(void* streamHandle);

    // Control de procesos limpio
    void stopActiveSpeech();

    char getSingleKey();
};

#endif
