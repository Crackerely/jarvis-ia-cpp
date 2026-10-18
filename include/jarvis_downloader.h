#ifndef JARVIS_DOWNLOADER_H
#define JARVIS_DOWNLOADER_H

#include <string>

class JarvisDownloader {
public:
    JarvisDownloader();
    ~JarvisDownloader();

    // Recibe una URL, un ID de YouTube o una búsqueda de texto y devuelve la URL directa del stream de audio
    std::string getAudioUrl(const std::string& searchQuery);

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
};

#endif // JARVIS_DOWNLOADER_H