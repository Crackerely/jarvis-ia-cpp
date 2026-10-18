#ifndef JARVIS_STATE_H
#define JARVIS_STATE_H

#include "jarvis_voice.h"
#include "jarvis_ffmpeg.h"
#include "jarvis_whisper.h"
#include "jarvis_listener.h"
#include <string>
#include <vector>
#include <memory>
#include <ctranslate2/translator.h>
#include <espeak-ng/speak_lib.h>
#include <functional>

typedef struct JarvisState {
    JarvisVoice voice;
    JarvisFFmpeg ffmpeg;
    JarvisWhisper whisper;
    std::unique_ptr<JarvisListener> listener;

    std::unique_ptr<ctranslate2::Translator> translator;

    JarvisState();
    ~JarvisState();

    bool synthesizeText(const std::string& text, const std::string& lang = "ES");
    std::string translateWithCT2(const std::vector<std::string>& tokens);

    // Control del listener de micrófono en segundo plano
    void startListening(std::function<void(const std::string&)> callback);
    void stopListening();
} JarvisState;

#endif