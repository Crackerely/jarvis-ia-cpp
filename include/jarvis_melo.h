#ifndef JARVIS_MELO_H
#define JARVIS_MELO_H

#include <string>

class JarvisMelo {
public:
    JarvisMelo();
    ~JarvisMelo();

    // Sintetiza texto a voz usando MeloTTS.cpp y lo reproduce o genera audio directo
    bool synthesizeAndPlay(const std::string& text, const std::string& lang = "ES");
};

#endif
