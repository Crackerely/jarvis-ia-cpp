#include "jarvis_state.h"
#include <iostream>
#include <portaudio.h>

// Puntero global temporal para el stream de PortAudio durante la síntesis mediante callback
static PaStream* global_pa_stream = nullptr;

// Callback de espeak-ng para capturar muestras PCM en memoria sin tocar ALSA directamente
static int espeak_synth_callback(short* wav, int numsamples, espeak_EVENT* events) {
    (void)events;
    if (wav == nullptr && numsamples == 0) {
        return 0; // Fin de la síntesis
    }
    if (numsamples > 0 && wav != nullptr && global_pa_stream != nullptr) {
        Pa_WriteStream(global_pa_stream, wav, numsamples);
    }
    return 0;
}

JarvisState::JarvisState() {
    std::cout << "[JarvisState] Inicializando estado global con CTranslate2 y espeak-ng nativos (RETRIEVAL)..." << std::endl;
    
    // Inicializar en modo AUDIO_OUTPUT_RETRIEVAL para evitar errores de tarjeta de sonido en contenedores
    int sampleRate = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 0, nullptr, 0);
    if (sampleRate < 0) {
        std::cerr << "[JarvisState ERROR] No se pudo inicializar espeak-ng." << std::endl;
    } else {
        std::cout << "[JarvisState] espeak-ng inicializado (Samplerate: " << sampleRate << " Hz)." << std::endl;
        espeak_SetSynthCallback(espeak_synth_callback);
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

    espeak_SetVoiceByName(lang.c_str());
    std::cout << "[JarvisState TTS Nativo] Sintetizando (" << lang << "): " << text << std::endl;

    // Intentar abrir stream de PortAudio (si no hay dispositivo, se maneja de forma segura en modo headless)
    PaError paErr = Pa_OpenDefaultStream(&global_pa_stream, 0, 1, paInt16, 22050, 512, nullptr, nullptr);
    if (paErr != paNoError) {
        std::cerr << "[JarvisState WARNING] PortAudio sin salida de audio activa (entorno headless/silencioso): " << Pa_GetErrorText(paErr) << std::endl;
        global_pa_stream = nullptr;
    } else {
        Pa_StartStream(global_pa_stream);
    }

    espeak_ERROR err = espeak_Synth(
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
        if (global_pa_stream) {
            Pa_StopStream(global_pa_stream);
            Pa_CloseStream(global_pa_stream);
            global_pa_stream = nullptr;
        }
        return false;
    }

    espeak_Synchronize();

    if (global_pa_stream) {
        Pa_StopStream(global_pa_stream);
        Pa_CloseStream(global_pa_stream);
        global_pa_stream = nullptr;
    }

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

void JarvisState::startListening(std::function<void(const std::string&)> callback) {
    if (!listener) {
        listener = std::make_unique<JarvisListener>(this, callback);
    }
    listener->start();
}

void JarvisState::stopListening() {
    if (listener) {
        listener->stop();
    }
}