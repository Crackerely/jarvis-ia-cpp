#include "jarvis_melo.h"
#include <cstdlib>
#include <iostream>

JarvisMelo::JarvisMelo() {
    // Inicialización del motor MeloTTS.cpp nativo
}

JarvisMelo::~JarvisMelo() {}

bool JarvisMelo::synthesizeAndPlay(const std::string& text, const std::string& lang) {
    // Llamada al binario/librería compilada de MeloTTS.cpp integrando salida de audio por tubería
    std::string cmd = "melotts --text \"" + text + "\" --language " + lang + " | aplay -q > /dev/null 2>&1";
    int res = std::system(cmd.c_str());
    if (res != 0) {
        // Fallback de emergencia por si el binario requiere ruta específica
        std::string fallbackCmd = "echo \"" + text + "\" | espeak-ng -v es -p 40 -s 155";
        return std::system(fallbackCmd.c_str()) == 0;
    }
    return true;
}
