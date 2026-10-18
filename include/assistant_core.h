#ifndef ASSISTANT_CORE_H
#define ASSISTANT_CORE_H

#include "jarvis_state.h"
#include <string>
#include <functional>
#include <espeak-ng/speak_lib.h>

class AssistantCore {
private:
    JarvisState state;
    std::string loadQueryPrompt(const std::string& filepath);
public:
    AssistantCore();
    ~AssistantCore();

    void speak(const std::string& text, const std::string& lang = "es");
    bool controlMusic(const std::string& command);
    char checkKey();
    std::string queryOllama(const std::string& prompt);
    std::string queryOllamaIntent(const std::string& userText);
    void recordAndSaveWav(const std::string& filename, int seconds);
    bool playYouTubeMusic(const std::string& query);

    // Métodos para el listener en segundo plano (Modo Alexa)
    void startListening(std::function<void(const std::string&)> callback);
    void stopListening();
};

#endif