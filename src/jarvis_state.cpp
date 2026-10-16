#include "jarvis_state.h"
#include <iostream>

JarvisState::JarvisState() {
    std::cout << "[JarvisState] Inicializando estado global con CTranslate2 y espeak-ng nativos..." << std::endl;
    
    // Inicializar la librería de espeak-ng en modo reproducción directa (AUDIO_OUTPUT_PLAYBACK)
    // El segundo parámetro (buffer length) en 0 usa el valor por defecto.
    int sampleRate = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, nullptr, 0);
    if (sampleRate < 0) {
        std::cerr << "[JarvisState ERROR] No se pudo inicializar espeak-ng." << std::endl;
    } else {
        std::cout << "[JarvisState] espeak-ng inicializado (Samplerate: " << sampleRate << " Hz)." << std::endl;
    }

    try {
        std::cout << "[JarvisState] CTranslate2 configurado correctamente en el núcleo." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[JarvisState WARNING] No se pudo inicializar el modelo CTranslate2: " << e.what() << std::endl;
    }
}

JarvisState::~JarvisState() {
    std::cout << "[JarvisState] Liberando estado global y recursos..." << std::endl;
    espeak_Terminate();
}

bool JarvisState::synthesizeText(const std::string& text, const std::string& lang) {
    if (text.empty()) return false;

    // Configurar idioma (ej. "es" o "en")
    espeak_SetVoiceByLanguage(lang.c_str());

    std::cout << "[JarvisState TTS Nativo] Sintetizando (" << lang << "): " << text << std::endl;

    // Llamada directa a la API de espeak-ng (sin subprocesos ni popen)
    // espeak_Synth(text, size, position, position_type, end_position, flags, unique_identifier, user_data)
    espeak_Error err = espeak_Synth(
        text.c_str(), 
        text.length() + 1, 
        0, 
        POS_CHARACTER, 
        0, 
        espeakCHARS_AUTO, 
        nullptr, 
        nullptr
    );

    if (err != EE_OK) {
        std::cerr << "[JarvisState ERROR] Falló la síntesis de espeak-ng." << std::endl;
        return false;
    }

    // Esperar a que termine de reproducir el búfer de audio completo
    espeak_Synchronize();
    return true;
}

std::string JarvisState::translateWithCT2(const std::vector<std::string>& tokens) {
    if (!translator) {
        std::cerr << "[JarvisState ERROR] El traductor CTranslate2 no ha sido cargado." << std::endl;
        return "";
    }

    try {
        std::vector<std::vector<std::string>> batch = {tokens};
        const auto results = translator->translate_batch(batch);
        
        if (!results.empty() && !results[0].hypotheses.empty()) {
            std::string output = "";
            for (const auto& token : results[0].hypotheses[0]) {
                output += token + " ";
            }
            return output;
        }
    } catch (const std::exception& e) {
        std::cerr << "[JarvisState ERROR] Falló la inferencia con CTranslate2: " << e.what() << std::endl;
    }

    return "";
}