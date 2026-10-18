#include "memory_player.h"
#include <iostream>
#include <thread>
#include <curl/curl.h>
#include <portaudio.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libswresample/swresample.h>
}

MemoryMusicPlayer::MemoryMusicPlayer() {
    avformat_network_init();
    Pa_Initialize();
}

MemoryMusicPlayer::~MemoryMusicPlayer() {
    Pa_Terminate();
    avformat_network_deinit();
}

size_t MemoryMusicPlayer::curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    AudioBuffer* buffer = static_cast<AudioBuffer*>(userp);
    buffer->append(static_cast<uint8_t*>(contents), totalSize);
    return totalSize;
}

int MemoryMusicPlayer::ffmpegReadPacket(void* opaque, uint8_t* buf, int buf_size) {
    AudioBuffer* buffer = static_cast<AudioBuffer*>(opaque);
    size_t bytesRead = buffer->read(buf, buf_size);
    if (bytesRead == 0) {
        return AVERROR_EOF;
    }
    return static_cast<int>(bytesRead);
}

bool MemoryMusicPlayer::playStream(const std::string& audioUrl) {
    // Asegurar que el buffer esté limpio antes de empezar una nueva descarga
    buffer_.clear();

    // 1. Iniciar la descarga en segundo plano mediante un thread independiente
    std::thread downloadThread([this, audioUrl]() {
        CURL* curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, audioUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer_);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
        buffer_.set_finished();
    });

    // 2. Configurar el buffer de E/S personalizado de FFmpeg (AVIOContext)
    const int avioBufferSize = 4096;
    unsigned char* avioBuffer = static_cast<unsigned char*>(av_malloc(avioBufferSize));
    AVIOContext* pIoContext = avio_alloc_context(
        avioBuffer, avioBufferSize, 0, &buffer_, 
        &MemoryMusicPlayer::ffmpegReadPacket, nullptr, nullptr
    );

    if (!pIoContext) {
        downloadThread.join();
        buffer_.clear();
        return false;
    }

    AVFormatContext* pFormatContext = avformat_alloc_context();
    pFormatContext->pb = pIoContext;

    if (avformat_open_input(&pFormatContext, nullptr, nullptr, nullptr) < 0) {
        std::cerr << "[Error] No se pudo abrir el stream a partir de la memoria." << std::endl;
        avformat_free_context(pFormatContext);
        avio_context_free(&pIoContext);
        downloadThread.join();
        buffer_.clear();
        return false;
    }

    if (avformat_find_stream_info(pFormatContext, nullptr) < 0) {
        avformat_close_input(&pFormatContext);
        downloadThread.join();
        buffer_.clear();
        return false;
    }

    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < pFormatContext->nb_streams; i++) {
        if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = (int)i;
            break;
        }
    }

    if (audioStreamIdx == -1) {
        avformat_close_input(&pFormatContext);
        downloadThread.join();
        buffer_.clear();
        return false;
    }

    AVCodecParameters* pCodecParams = pFormatContext->streams[audioStreamIdx]->codecpar;
    const AVCodec* pCodec = avcodec_find_decoder(pCodecParams->codec_id);
    AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pCodecParams);
    avcodec_open2(pCodecCtx, pCodec, nullptr);

    // 3. Inicializar PortAudio para la reproducción en tiempo real
    PaStream* paStream = nullptr;
    Pa_OpenDefaultStream(&paStream, 0, pCodecCtx->ch_layout.nb_channels, paFloat32, 
                         pCodecCtx->sample_rate ? pCodecCtx->sample_rate : 44100, 1024, nullptr, nullptr);
    Pa_StartStream(paStream);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    SwrContext* swrCtx = nullptr;
    AVChannelLayout out_ch_layout = {};
    av_channel_layout_default(&out_ch_layout, pCodecCtx->ch_layout.nb_channels);
    swr_alloc_set_opts2(&swrCtx, &out_ch_layout, AV_SAMPLE_FMT_FLT, pCodecCtx->sample_rate,
                        &pCodecCtx->ch_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, nullptr);
    swr_init(swrCtx);

    // 4. Bucle principal de decodificación y reproducción simultánea
    while (av_read_frame(pFormatContext, packet) >= 0) {
        if (packet->stream_index == audioStreamIdx) {
            int ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret >= 0) {
                while (avcodec_receive_frame(pCodecCtx, frame) >= 0) {
                    float* outBuffer = nullptr;
                    av_samples_alloc(reinterpret_cast<uint8_t**>(&outBuffer), nullptr, 
                                     pCodecCtx->ch_layout.nb_channels, frame->nb_samples, AV_SAMPLE_FMT_FLT, 0);
                    
                    swr_convert(swrCtx, reinterpret_cast<uint8_t**>(&outBuffer), frame->nb_samples,
                                const_cast<const uint8_t**>(frame->data), frame->nb_samples);

                    Pa_WriteStream(paStream, outBuffer, frame->nb_samples);
                    av_freep(&outBuffer);
                }
            }
        }
        av_packet_unref(packet);
    }

    // 5. Limpieza y liberación absoluta de recursos y memoria RAM
    Pa_StopStream(paStream);
    Pa_CloseStream(paStream);
    swr_free(&swrCtx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatContext); // Esto también libera el avioContext y su buffer interno asignado con av_malloc
    
    downloadThread.join();

    // Liberar explícitamente el buffer de datos descargados de la canción reproducida
    buffer_.clear();

    std::cout << "[MemoryMusicPlayer SUCCESS] Reproducción finalizada y memoria RAM liberada." << std::endl;
    return true;
}