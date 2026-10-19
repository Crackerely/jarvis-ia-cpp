#include "memory_player.h"
#include <iostream>
#include <curl/curl.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libswresample/swresample.h>
}

#include <portaudio.h>

MemoryMusicPlayer::MemoryMusicPlayer() 
    : head_(nullptr), tail_(nullptr), currentSong_(nullptr),
      isRunning_(true), isPlayingCurrent_(false), abortCurrent_(false), currentProgressSeconds_(0.0) {
    playerThread_ = std::thread(&MemoryMusicPlayer::playerLoop, this);
}

MemoryMusicPlayer::~MemoryMusicPlayer() {
    stop();
}

void MemoryMusicPlayer::stop() {
    isRunning_ = false;
    abortCurrent_ = true;
    buffer_.set_finished();
    queueCv_.notify_all();
    if (playerThread_.joinable()) {
        playerThread_.join();
    }

    std::lock_guard<std::mutex> lock(queueMtx_);
    SongNode* current = head_;
    while (current != nullptr) {
        SongNode* next = current->next;
        delete current;
        current = next;
    }
    head_ = tail_ = currentSong_ = nullptr;
}

void MemoryMusicPlayer::addSong(const std::string& title, const std::string& artist, const std::string& url, double duration) {
    if (url.empty()) return;

    SongNode* newNode = new SongNode(title, artist, url, duration);

    {
        std::lock_guard<std::mutex> lock(queueMtx_);
        if (head_ == nullptr) {
            head_ = tail_ = newNode;
        } else {
            tail_->next = newNode;
            tail_ = newNode;
        }
        std::cout << "[MemoryMusicPlayer LinkedList] Canción encolada: " << title << " - " << artist << std::endl;
    }
    queueCv_.notify_one();
}

void MemoryMusicPlayer::playNow(const std::string& title, const std::string& artist, const std::string& url, double duration) {
    if (url.empty()) return;

    SongNode* newNode = new SongNode(title, artist, url, duration);

    {
        std::lock_guard<std::mutex> lock(queueMtx_);
        SongNode* current = head_;
        while (current != nullptr) {
            SongNode* next = current->next;
            delete current;
            current = next;
        }
        head_ = tail_ = newNode;
    }
    
    abortCurrent_ = true;
    buffer_.set_finished();
    queueCv_.notify_all();
}

bool MemoryMusicPlayer::isPlayingOrQueued() {
    std::lock_guard<std::mutex> lock(queueMtx_);
    return isPlayingCurrent_ || (head_ != nullptr);
}

int MemoryMusicPlayer::getQueueSize() {
    std::lock_guard<std::mutex> lock(queueMtx_);
    int count = 0;
    SongNode* current = head_;
    while (current != nullptr) {
        count++;
        current = current->next;
    }
    return count;
}

std::string MemoryMusicPlayer::getCurrentSongTitle() {
    std::lock_guard<std::mutex> lock(queueMtx_);
    return currentSong_ ? currentSong_->title : "";
}

std::string MemoryMusicPlayer::getCurrentSongArtist() {
    std::lock_guard<std::mutex> lock(queueMtx_);
    return currentSong_ ? currentSong_->artist : "";
}

double MemoryMusicPlayer::getCurrentSongProgress() {
    std::lock_guard<std::mutex> lock(queueMtx_);
    return currentProgressSeconds_;
}

double MemoryMusicPlayer::getCurrentSongDuration() {
    std::lock_guard<std::mutex> lock(queueMtx_);
    return currentSong_ ? currentSong_->durationSeconds : 0.0;
}

void MemoryMusicPlayer::playerLoop() {
    while (isRunning_) {
        SongNode* songToPlay = nullptr;

        {
            std::unique_lock<std::mutex> lock(queueMtx_);
            queueCv_.wait(lock, [this]() {
                return head_ != nullptr || !isRunning_;
            });

            if (!isRunning_) break;

            if (head_ != nullptr) {
                songToPlay = head_;
                head_ = head_->next;
                if (head_ == nullptr) {
                    tail_ = nullptr;
                }
                currentSong_ = songToPlay;
            }
        }

        if (songToPlay != nullptr) {
            isPlayingCurrent_ = true;
            abortCurrent_ = false;
            
            {
                std::lock_guard<std::mutex> lock(queueMtx_);
                currentProgressSeconds_ = 0.0;
            }

            playStreamInternal(songToPlay);

            delete songToPlay;
            {
                std::lock_guard<std::mutex> lock(queueMtx_);
                currentSong_ = nullptr;
            }
            isPlayingCurrent_ = false;
        }
    }
}

size_t MemoryMusicPlayer::curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    AudioBuffer* buf = static_cast<AudioBuffer*>(userp);
    size_t totalSize = size * nmemb;
    buf->append(static_cast<const uint8_t*>(contents), totalSize);
    return totalSize;
}

int MemoryMusicPlayer::ffmpegReadPacket(void* opaque, uint8_t* buf, int buf_size) {
    AudioBuffer* audioBuf = static_cast<AudioBuffer*>(opaque);
    size_t bytesRead = audioBuf->read(buf, buf_size);
    if (bytesRead == 0) {
        return AVERROR_EOF;
    }
    return static_cast<int>(bytesRead);
}

bool MemoryMusicPlayer::playStreamInternal(SongNode* song) {
    buffer_.clear();

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, song->url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode* curlResPtr = new CURLcode(CURLE_OK);
    std::thread downloadThread([curl, curlResPtr]() {
        *curlResPtr = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    });

    const int bufferSize = 4096;
    unsigned char* avioBuffer = static_cast<unsigned char*>(av_malloc(bufferSize));
    if (!avioBuffer) {
        buffer_.set_finished();
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    AVIOContext* avioCtx = avio_alloc_context(avioBuffer, bufferSize, 0, &buffer_, ffmpegReadPacket, nullptr, nullptr);
    if (!avioCtx) {
        av_freep(&avioBuffer);
        buffer_.set_finished();
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    AVFormatContext* fmtCtx = avformat_alloc_context();
    if (!fmtCtx) {
        avio_context_free(&avioCtx);
        buffer_.set_finished();
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    fmtCtx->pb = avioCtx;

    if (avformat_open_input(&fmtCtx, nullptr, nullptr, nullptr) < 0) {
        avformat_free_context(fmtCtx);
        avio_context_free(&avioCtx);
        buffer_.set_finished();
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
            break;
        }
    }

    if (audioStreamIdx == -1) {
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    AVCodecParameters* codecpar = fmtCtx->streams[audioStreamIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        avio_context_free(&avioCtx);
        downloadThread.join();
        delete curlResPtr;
        return false;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecpar);
    avcodec_open2(codecCtx, codec, nullptr);

    PaStream* stream = nullptr;
    Pa_OpenDefaultStream(&stream, 0, codecCtx->ch_layout.nb_channels, paInt16, codecCtx->sample_rate, 512, nullptr, nullptr);
    Pa_StartStream(stream);

    SwrContext* swrCtx = nullptr;
    swr_alloc_set_opts2(&swrCtx,
        &codecCtx->ch_layout, AV_SAMPLE_FMT_S16, codecCtx->sample_rate,
        &codecCtx->ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
        0, nullptr);
    swr_init(swrCtx);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int sampleRate = codecCtx->sample_rate;

    while (isRunning_ && !abortCurrent_ && av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index == audioStreamIdx) {
            int ret = avcodec_send_packet(codecCtx, pkt);
            if (ret >= 0) {
                while (isRunning_ && !abortCurrent_ && avcodec_receive_frame(codecCtx, frame) >= 0) {
                    int outLinesize = 0;
                    uint8_t* outBuffer = nullptr;
                    int maxDstSamples = av_rescale_rnd(swr_get_delay(swrCtx, sampleRate) + frame->nb_samples, sampleRate, sampleRate, AV_ROUND_UP);
                    
                    av_samples_alloc(&outBuffer, &outLinesize, codecCtx->ch_layout.nb_channels, maxDstSamples, AV_SAMPLE_FMT_S16, 0);
                    int convertedSamples = swr_convert(swrCtx, &outBuffer, maxDstSamples, (const uint8_t**)frame->data, frame->nb_samples);

                    if (convertedSamples > 0) {
                        Pa_WriteStream(stream, outBuffer, convertedSamples);
                        
                        // Actualización de progreso protegida por mutex
                        {
                            std::lock_guard<std::mutex> lock(queueMtx_);
                            currentProgressSeconds_ += static_cast<double>(convertedSamples) / sampleRate;
                        }
                    }
                    if (outBuffer) {
                        av_freep(&outBuffer);
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    buffer_.set_finished();
    downloadThread.join();

    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
    avio_context_free(&avioCtx);
    swr_free(&swrCtx);
    delete curlResPtr;

    return true;
}