#ifndef MEMORY_PLAYER_H
#define MEMORY_PLAYER_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <cstdint>
#include <cstring>

typedef struct AudioBuffer {
    std::vector<uint8_t> data;
    size_t read_offset = 0;
    bool is_finished = false;
    std::mutex mtx;
    std::condition_variable cv;

    // Añade fragmentos descargados por cURL al buffer
    void append(const uint8_t* chunk, size_t size) {
        std::lock_guard<std::mutex> lock(mtx);
        data.insert(data.end(), chunk, chunk + size);
        cv.notify_all();
    }

    // Lee datos del buffer de forma síncrona (espera si no hay suficientes datos)
    size_t read(uint8_t* dest, size_t size) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this, size]() {
            return read_offset + size <= data.size() || is_finished;
        });

        if (read_offset >= data.size() && is_finished) {
            return 0; // Fin del archivo / Stream finalizado
        }

        size_t available = data.size() - read_offset;
        size_t bytes_to_read = (size < available) ? size : available;

        if (bytes_to_read > 0) {
            std::memcpy(dest, data.data() + read_offset, bytes_to_read);
            read_offset += bytes_to_read;
        }
        return bytes_to_read;
    }

    void set_finished() {
        std::lock_guard<std::mutex> lock(mtx);
        is_finished = true;
        cv.notify_all();
    }

    // Libera la memoria reservada y restablece los estados del buffer
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        data.clear();
        data.shrink_to_fit(); // Libera la capacidad asignada al heap
        read_offset = 0;
        is_finished = false;
    }
} AudioBuffer;

class MemoryMusicPlayer {
public:
    MemoryMusicPlayer();
    ~MemoryMusicPlayer();

    bool playStream(const std::string& audioUrl);

private:
    AudioBuffer buffer_;
    
    static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int ffmpegReadPacket(void* opaque, uint8_t* buf, int buf_size);
};

#endif // MEMORY_PLAYER_H