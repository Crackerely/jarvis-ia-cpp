#ifndef MEMORY_PLAYER_H
#define MEMORY_PLAYER_H

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <cstring>

struct SongNode {
    std::string title;
    std::string artist;
    std::string url;
    double durationSeconds;
    SongNode* next;

    SongNode(const std::string& t, const std::string& art, const std::string& u, double dur)
        : title(t), artist(art), url(u), durationSeconds(dur), next(nullptr) {}
};

typedef struct AudioBuffer {
    std::vector<uint8_t> data;
    size_t read_offset = 0;
    bool is_finished = false;
    std::mutex mtx;
    std::condition_variable cv;

    void append(const uint8_t* chunk, size_t size) {
        std::lock_guard<std::mutex> lock(mtx);
        data.insert(data.end(), chunk, chunk + size);
        cv.notify_all();
    }

    size_t read(uint8_t* dest, size_t size) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this, size]() {
            return read_offset + size <= data.size() || is_finished;
        });

        if (read_offset >= data.size() && is_finished) {
            return 0;
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

    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        data.clear();
        data.shrink_to_fit();
        read_offset = 0;
        is_finished = false;
    }
} AudioBuffer;

class MemoryMusicPlayer {
public:
    MemoryMusicPlayer();
    ~MemoryMusicPlayer();

    void addSong(const std::string& title, const std::string& artist, const std::string& url, double duration);
    void playNow(const std::string& title, const std::string& artist, const std::string& url, double duration);
    void stop();
    
    bool isPlayingOrQueued();

    std::string getCurrentSongTitle();
    std::string getCurrentSongArtist();
    double getCurrentSongProgress();
    double getCurrentSongDuration();
    int getQueueSize();

private:
    AudioBuffer buffer_;
    
    SongNode* head_;
    SongNode* tail_;
    SongNode* currentSong_;

    std::thread playerThread_;
    std::mutex queueMtx_;
    std::condition_variable queueCv_;
    std::atomic<bool> isRunning_;
    std::atomic<bool> isPlayingCurrent_;
    std::atomic<bool> abortCurrent_;
    
    double currentProgressSeconds_; // Protegido por queueMtx_

    void playerLoop();
    bool playStreamInternal(SongNode* song);

    static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int ffmpegReadPacket(void* opaque, uint8_t* buf, int buf_size);
};

#endif // MEMORY_PLAYER_H