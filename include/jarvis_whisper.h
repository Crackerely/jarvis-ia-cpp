#ifndef JARVIS_WHISPER_H
#define JARVIS_WHISPER_H

#include <string>
#include <vector>

struct WhisperSegment {
    std::string text;
};

struct WhisperInfo {
    std::string language;
};

class JarvisWhisper {
private:
    std::string model_name;
    std::string device;
    std::string compute_type;

public:
    JarvisWhisper(const std::string& model_size = "tiny", const std::string& dev = "cpu", const std::string& comp_type = "int8");
    ~JarvisWhisper();

    // Devuelve un par con los segmentos de texto y la info (idioma detectado)
    std::pair<std::vector<WhisperSegment>, WhisperInfo> transcribe(const std::string& audio_path, int beam_size = 5);
};

#endif
