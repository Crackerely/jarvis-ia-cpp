#ifndef JARVIS_LISTENER_H
#define JARVIS_LISTENER_H

#include <string>
#include <thread>
#include <atomic>
#include <functional>

struct JarvisState;

class JarvisListener {
public:
    JarvisListener(JarvisState* state, std::function<void(const std::string&)> onPromptDetected);
    ~JarvisListener();

    void start();
    void stop();

private:
    JarvisState* state_;
    std::function<void(const std::string&)> callback_;
    std::thread listenerThread_;
    std::atomic<bool> isRunning_;
    
    // Miembro configurable para la frase de activación
    std::string wake_string_ = "jarvis activar";

    void run();
};

#endif // JARVIS_LISTENER_H