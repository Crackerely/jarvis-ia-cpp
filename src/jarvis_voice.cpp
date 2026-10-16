#include "jarvis_voice.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <portaudio.h>
#include <curl/curl.h>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

JarvisVoice::JarvisVoice() : active_speech_pid(-1) {
    int old_stderr = dup(STDERR_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    Pa_Initialize();
    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (old_stderr != -1) {
        dup2(old_stderr, STDERR_FILENO);
        close(old_stderr);
    }
}

void JarvisVoice::stopActiveSpeech() {
    if (active_speech_pid > 0) {
        kill(active_speech_pid, SIGKILL);
        active_speech_pid = -1;
    }
}

JarvisVoice::~JarvisVoice() {
    curl_global_cleanup();
    Pa_Terminate();
}

int JarvisVoice::executeProcess(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string JarvisVoice::httpPostOllama(const std::string& url, const std::string& jsonData) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;

    if (!curl) return "";

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[JarvisVoice ERROR] curl_easy_perform failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return readBuffer;
}

bool JarvisVoice::streamAudioWithFFmpeg(const std::string& inputSource) {
    std::string cmd = "ffmpeg -re -i " + inputSource + " -f alsa default -loglevel quiet &";
    return std::system(cmd.c_str()) == 0;
}

bool JarvisVoice::sendMpvCommand(const std::string& socketPath, const std::string& jsonCommand) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr{};
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