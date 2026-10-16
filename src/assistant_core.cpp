#include "assistant_core.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

AssistantCore::AssistantCore() {}
AssistantCore::~AssistantCore() {}

void AssistantCore::speak(const std::string& text, const std::string& lang) {
    state.synthesizeText(text, lang);
}

bool AssistantCore::controlMusic(const std::string& command) {
    std::string socketPath = "/tmp/mpvsocket";
    std::string jsonCmd;

    if (command == "stop") jsonCmd = "{\"command\": [\"stop\"]}";
    else if (command == "pause") jsonCmd = "{\"command\": [\"set_property\", \"pause\", true]}";
    else if (command == "resume") jsonCmd = "{\"command\": [\"set_property\", \"pause\", false]}";
    else return false;

    return state.voice.sendMpvCommand(socketPath, jsonCmd);
}

char AssistantCore::checkKey() {
    return state.voice.getSingleKey();
}

std::string AssistantCore::queryOllama(const std::string& prompt) {
    std::string cleanPrompt = prompt;
    size_t pos = 0;
    while ((pos = cleanPrompt.find("\"", pos)) != std::string::npos) {
        cleanPrompt.replace(pos, 1, "\\\"");
        pos += 2;
    }
    std::string jsonData = "{\"model\": \"llama3.2\", \"prompt\": \"" + cleanPrompt + "\", \"stream\": false}";
    return state.voice.httpPostOllama("http://127.0.0.1:11434/api/generate", jsonData);
}

std::string AssistantCore::queryOllamaIntent(const std::string& userText) {
    std::string systemPrompt = "You are a bilingual smart home assistant intent router. Analyze user input.\n"
                               "1. Control music -> {\"action\": \"control_playback\", \"command\": \"stop|pause|resume\"}\n"
                               "2. Play music -> {\"action\": \"play_music\", \"song\": \"song and artist\"}\n"
                               "3. Chat -> {\"action\": \"chat\"}\nOutput ONLY valid JSON.";
    std::string fullPrompt = systemPrompt + "\nUser: " + userText + "\nJSON:";
    std::string jsonData = "{\"model\": \"llama3.2\", \"prompt\": \"" + fullPrompt + "\", \"stream\": false, \"format\": \"json\"}";
    return state.voice.httpPostOllama("http://127.0.0.1:11434/api/generate", jsonData);
}

void AssistantCore::recordAndSaveWav(const std::string& filename, int seconds) {
    std::vector<int16_t> audioData = state.voice.recordAudioChunk(seconds);
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) return;

    int dataSize = audioData.size() * sizeof(int16_t);
    int chunkSize = 36 + dataSize;

    outFile.write("RIFF", 4);
    outFile.write(reinterpret_cast<const char*>(&chunkSize), 4);
    outFile.write("WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x80>\x00\x00\x00}\x00\x02\x00\x10\x00data", 24);
    outFile.write(reinterpret_cast<const char*>(&dataSize), 4);
    outFile.write(reinterpret_cast<const char*>(audioData.data()), dataSize);
    outFile.close();
}

bool AssistantCore::playYouTubeMusic(const std::string& query) {
    std::cout << "[AssistantCore] Consultando stream para: " << query << std::endl;
    std::string directUrl = state.ffmpeg.getDirectStreamUrl(query);
    if (directUrl.empty()) {
        std::cerr << "[AssistantCore ERROR] No se pudo obtener la URL." << std::endl;
        return false;
    }
    return state.ffmpeg.streamAudio(directUrl);
}