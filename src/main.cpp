#include "assistant_core.h"
#include <iostream>

int main() {
    std::cout << "[C++] Iniciando núcleo de Jarvis..." << std::endl;
    AssistantCore assistant;
    
    // Prueba rápida de síntesis de voz nativa desde C++
    assistant.speak("Núcleo de C plus plus cargado exitosamente.", "es");
    
    return 0;
}