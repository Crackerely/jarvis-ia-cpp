#include "assistant_core.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

static std::string escapeJsonString(const std::string& input) {
    std::string output = "";
    for (char c : input) {
        switch (c) {
            case '\"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output += c;
                break;
        }
    }
    return output;
}

AssistantCore::AssistantCore() {
    std::cout << "[AssistantCore] Inicializando núcleo asistente..." << std::endl;
}

AssistantCore::~AssistantCore() {
    std::cout << "[AssistantCore] Finalizando núcleo asistente..." << std::endl;
}

void AssistantCore::speak(const std::string& text, const std::string& lang) {
    // Delegación limpia al estado global centralizado
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
    std::string cleanPrompt = escapeJsonString(prompt);
    std::string jsonData = "{\"model\": \"llama3.2\", \"prompt\": \"" + cleanPrompt + "\", \"stream\": false}";
    return state.voice.httpPostOllama("http://127.0.0.1:11434/api/generate", jsonData);
}

std::string AssistantCore::loadQueryPrompt(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[AssistantCore ERROR] No se pudo abrir el archivo de prompt: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string AssistantCore::queryOllamaIntent(const std::string& userText) {
    // Cargar el prompt dinámicamente desde el archivo de texto
    std::string systemPrompt = loadQueryPrompt("queryprompt.txt");
    
    // Fallback de seguridad por si el archivo no existe o está vacío
    if (systemPrompt.empty()) {
        // systemPrompt = "You are a bilingual smart home assistant intent router. Output ONLY valid JSON.";
        std::cerr << "[AssistantCore ERROR] No se pudo cargar el FILE-STREAM del prompt (queryprompt.txt)." << std::endl;
        return nullptr;
    }

    std::string cleanUserText = userText;
    size_t pos = 0;
    while ((pos = cleanUserText.find("\"", pos)) != std::string::npos) {
        cleanUserText.replace(pos, 1, "\\\"");
        pos += 2;
    }

    std::string fullPrompt = systemPrompt + "\nUser: " + userText + "\nJSON:";
    std::string cleanPrompt = escapeJsonString(fullPrompt);
    std::string jsonData = "{\"model\": \"llama3.2\", \"prompt\": \"" + cleanPrompt + "\", \"stream\": false, \"format\": \"json\"}";
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

std::string AssistantCore::findYouTubeUrl(const std::string& query) {
    std::cout << "[AssistantCore] Buscando URL en YouTube para: " << query << std::endl;
    return state.ffmpeg.getDirectStreamUrl(query);
}

bool AssistantCore::playYouTubeMusic(const std::string& query) {
    std::cout << "[AssistantCore] Procesando música (URL directa): " << query << std::endl;
    std::string directUrl = query;
    
    if (directUrl.find("googlevideo.com") == std::string::npos && directUrl.find("watch?v=") == std::string::npos) {
        directUrl = findYouTubeUrl(query);
    }

    if (directUrl.empty()) {
        std::cerr << "[AssistantCore ERROR] No se pudo obtener la URL de streaming." << std::endl;
        return false;
    }
    return state.ffmpeg.streamAudioFromUrl(directUrl);
}

bool AssistantCore::playYouTubeMusicWithMetadata(const std::string& title, const std::string& artist, const std::string& url, double duration) {
    std::cout << "[AssistantCore] Añadiendo a la lista enlazada con metadatos: " << title << std::endl;
    std::string directUrl = url;
    
    if (directUrl.find("googlevideo.com") == std::string::npos && directUrl.find("watch?v=") == std::string::npos) {
        directUrl = findYouTubeUrl(url);
    }

    if (directUrl.empty()) {
        std::cerr << "[AssistantCore ERROR] No se pudo obtener la URL de streaming con metadatos." << std::endl;
        return false;
    }

    return state.ffmpeg.streamAudioWithMetadata(title, artist, directUrl, duration);
}

void AssistantCore::startListening(std::function<void(const std::string&)> callback) {
    state.startListening(callback);
}

void AssistantCore::stopListening() {
    state.stopListening();
}

std::string AssistantCore::getCurrentSongTitle() {
    return state.ffmpeg.getCurrentSongTitle();
}

std::string AssistantCore::getCurrentSongArtist() {
    return state.ffmpeg.getCurrentSongArtist();
}

double AssistantCore::getCurrentSongProgress() {
    return state.ffmpeg.getCurrentSongProgress();
}

double AssistantCore::getCurrentSongDuration() {
    return state.ffmpeg.getCurrentSongDuration();
}

int AssistantCore::getQueueSize() {
    return state.ffmpeg.getQueueSize();
}