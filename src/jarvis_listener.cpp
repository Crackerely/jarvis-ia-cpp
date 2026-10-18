#include "jarvis_listener.h"
#include "jarvis_state.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cmath>

JarvisListener::JarvisListener(JarvisState* state, std::function<void(const std::string&)> onPromptDetected)
    : state_(state), callback_(onPromptDetected), isRunning_(false) {}

JarvisListener::~JarvisListener() {
    stop();
}

void JarvisListener::start() {
    if (isRunning_) return;
    isRunning_ = true;
    listenerThread_ = std::thread(&JarvisListener::run, this);
    
    std::ofstream logFile("/tmp/jarvis_listener.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << "[JarvisListener] Hilo de escucha con FFmpeg nativo iniciado.\n";
    }
}

void JarvisListener::stop() {
    if (!isRunning_) return;
    isRunning_ = false;
    if (listenerThread_.joinable()) {
        listenerThread_.join();
    }
}

void JarvisListener::run() {
    while (isRunning_) {
        if (!state_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 1. Grabamos directamente una ventana continua de 5 segundos (captura wake word y comando juntos)
        std::vector<int16_t> audioChunk = state_->voice.recordAudioChunk(5);
        if (audioChunk.empty() || !isRunning_) {
            continue;
        }

        // Filtro de energía RMS para omitir silencio absoluto o ruido ambiente/aplausos vacíos
        double sum = 0;
        for (int16_t sample : audioChunk) {
            sum += (double)sample * sample;
        }
        double rms = std::sqrt(sum / audioChunk.size());

        if (rms < 350.0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Guardar WAV temporal usando la API nativa de C de FFmpeg
        std::string tempFilename = "/tmp/jarvis_listen.wav";
        state_->ffmpeg.savePcmToWav(audioChunk, tempFilename, 16000);

        auto transcriptionResult = state_->whisper.transcribe(tempFilename);
        std::string transcript = "";
        for (const auto& segment : transcriptionResult.first) {
            transcript += segment.text + " ";
        }

        if (!transcript.empty()) {
            std::string lowerTranscript = transcript;
            std::transform(lowerTranscript.begin(), lowerTranscript.end(), lowerTranscript.begin(), ::tolower);

            // Verificamos si la wake_string_ está presente en el bloque completo
            if (lowerTranscript.find(wake_string_) != std::string::npos) {
                
                // --- EQUIVALENTE A TU LÓGICA EN LUA (:gsub) ---
                // Eliminamos la frase de activación de la transcripción obtenida
                std::string prompt = transcript;
                size_t posToReplace = prompt.find(wake_string_);
                if (posToReplace == std::string::npos) {
                    std::string lowerPromptForSearch = prompt;
                    std::transform(lowerPromptForSearch.begin(), lowerPromptForSearch.end(), lowerPromptForSearch.begin(), ::tolower);
                    posToReplace = lowerPromptForSearch.find(wake_string_);
                }

                if (posToReplace != std::string::npos) {
                    prompt.erase(posToReplace, wake_string_.length());
                }

                // Limpieza profunda de espacios, comas, puntos o signos sobrantes al inicio y final
                while (!prompt.empty() && (prompt[0] == ' ' || prompt[0] == ',' || prompt[0] == '.' || prompt[0] == ':' || prompt[0] == '-' || prompt[0] == '\n')) {
                    prompt.erase(0, 1);
                }
                while (!prompt.empty() && (prompt.back() == ' ' || prompt.back() == '.' || prompt.back() == ',' || prompt.back() == '\n')) {
                    prompt.pop_back();
                }

                {
                    std::ofstream logFile("/tmp/jarvis_listener.log", std::ios::app);
                    if (logFile.is_open()) {
                        logFile << "[JarvisListener] Transcripción completa: '" << transcript << "'\n";
                        logFile << "[JarvisListener] Prompt final limpio: '" << prompt << "'\n";
                    }
                }

                // REQUISITO ESTRICTO: Si el prompt está vacío (solo dijiste la frase clave sin orden), IGNORAR
                if (prompt.empty()) {
                    continue;
                }

                if (callback_) {
                    callback_(prompt);
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}