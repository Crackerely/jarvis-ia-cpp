#include "include/jarvis_voice.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>
#include <portaudio.h>

// En el constructor JarvisVoice::JarvisVoice() inicializamos el PID:
JarvisVoice::JarvisVoice() : active_speech_pid(-1) {
    // Silenciar errores molestos de ALSA/JACK en la terminal
    int old_stderr = dup(STDERR_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    Pa_Initialize();

    // Restaurar stderr
    if (old_stderr != -1) {
        dup2(old_stderr, STDERR_FILENO);
        close(old_stderr);
    }
}

// Implementación del método de la clase para detener voz de forma segura
void JarvisVoice::stopActiveSpeech() {
    if (active_speech_pid > 0) {
        kill(active_speech_pid, SIGKILL);
        active_speech_pid = -1;
    }
}

JarvisVoice::~JarvisVoice() {
    Pa_Terminate();
}

int JarvisVoice::executeProcess(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string JarvisVoice::httpPostOllama(const std::string& host, int port, const std::string& endpoint, const std::string& jsonData) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        close(sock);
        return "";
    }

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        close(sock);
        return "";
    }

    std::string request = "POST " + endpoint + " HTTP/1.1\r\n" +
                          "Host: " + host + "\r\n" +
                          "Content-Type: application/json\r\n" +
                          "Content-Length: " + std::to_string(jsonData.size()) + "\r\n" +
                          "Connection: close\r\n\r\n" +
                          jsonData;

    send(sock, request.c_str(), request.size(), 0);

    std::string response = "";
    char buffer[4096];
    int bytesReceived = 0;
    while ((bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        response += buffer;
    }

    close(sock);

    size_t pos = response.find("\r\n\r\n");
    if (pos != std::string::npos) {
        return response.substr(pos + 4);
    }
    return response;
}

bool JarvisVoice::streamAudioWithFFmpeg(const std::string& inputSource) {
    std::string cmd = "ffmpeg -re -i " + inputSource + " -f alsa default -loglevel quiet &";
    return std::system(cmd.c_str()) == 0;
}

bool JarvisVoice::sendMpvCommand(const std::string& socketPath, const std::string& jsonCommand) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    std::string payload = jsonCommand + "\n";
    send(sock, payload.c_str(), payload.size(), 0);
    close(sock);
    return true;
}

void* JarvisVoice::openAudioStream() {
    PaStream* stream;
    Pa_OpenDefaultStream(&stream, 1, 0, paInt16, 16000, 1024, nullptr, nullptr);
    Pa_StartStream(stream);
    return static_cast<void*>(stream);
}

std::vector<int16_t> JarvisVoice::readAudioStreamChunk(void* streamHandle, int chunkSize) {
    PaStream* stream = static_cast<PaStream*>(streamHandle);
    std::vector<int16_t> buffer(chunkSize);
    Pa_ReadStream(stream, buffer.data(), chunkSize);
    return buffer;
}

void JarvisVoice::closeAudioStream(void* streamHandle) {
    PaStream* stream = static_cast<PaStream*>(streamHandle);
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
}

std::vector<int16_t> JarvisVoice::recordAudioChunk(int durationSeconds) {
    int sampleRate = 16000;
    int totalFrames = sampleRate * durationSeconds;
    std::vector<int16_t> buffer(totalFrames);

    void* stream = openAudioStream();
    PaStream* paStream = static_cast<PaStream*>(stream);
    Pa_ReadStream(paStream, buffer.data(), totalFrames);
    closeAudioStream(stream);

    return buffer;
}

char JarvisVoice::getSingleKey() {
    struct termios oldt, newt;
    char ch = 0;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) return ch;
    return 0;
}
