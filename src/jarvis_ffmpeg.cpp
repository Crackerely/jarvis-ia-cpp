#include "jarvis_ffmpeg.h"
#include "jarvis_downloader.h"
#include <iostream>
#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

JarvisFFmpeg::JarvisFFmpeg() {}

JarvisFFmpeg::~JarvisFFmpeg() {}

std::string JarvisFFmpeg::getDirectStreamUrl(const std::string& searchQuery) {
    JarvisDownloader downloader;
    std::string directAudioUrl = downloader.getAudioUrl(searchQuery);

    if (directAudioUrl.empty()) {
        std::cerr << "[JarvisFFmpeg ERROR] JarvisDownloader no pudo proveer una URL válida." << std::endl;
    }

    return directAudioUrl;
}

bool JarvisFFmpeg::streamAudio(const std::string& url) {
    if (url.empty()) {
        std::cerr << "[JarvisFFmpeg ERROR] URL vacía proporcionada para streaming." << std::endl;
        return false;
    }

    std::cout << "[JarvisFFmpeg] Descargando y reproduciendo audio en memoria (Buffer dinámico)..." << std::endl;
    
    bool success = memoryPlayer.playStream(url);
    
    if (!success) {
        std::cerr << "[JarvisFFmpeg ERROR] Falló el proceso de streaming en memoria." << std::endl;
        return false;
    }

    return true;
}

bool JarvisFFmpeg::savePcmToWav(const std::vector<int16_t>& pcmData, const std::string& filename, int sampleRate) {
    if (pcmData.empty()) return false;

    AVFormatContext* fmtCtx = nullptr;
    if (avformat_alloc_output_context2(&fmtCtx, nullptr, "wav", filename.c_str()) < 0) {
        std::cerr << "[JarvisFFmpeg ERROR] No se pudo crear el contexto de salida FFmpeg para WAV." << std::endl;
        return false;
    }

    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        std::cerr << "[JarvisFFmpeg ERROR] No se pudo crear el stream de audio en FFmpeg." << std::endl;
        avformat_free_context(fmtCtx);
        return false;
    }

    stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    stream->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;
    stream->codecpar->format = AV_SAMPLE_FMT_S16;
    stream->codecpar->ch_layout.nb_channels = 1;
    stream->codecpar->sample_rate = sampleRate;
    stream->codecpar->bits_per_coded_sample = 16;
    stream->codecpar->block_align = 2;
    stream->codecpar->bit_rate = sampleRate * 16 * 1;

    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmtCtx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "[JarvisFFmpeg ERROR] No se pudo abrir el archivo WAV con AVIO: " << filename << std::endl;
            avformat_free_context(fmtCtx);
            return false;
        }
    }

    if (avformat_write_header(fmtCtx, nullptr) < 0) {
        std::cerr << "[JarvisFFmpeg ERROR] No se pudo escribir la cabecera WAV con FFmpeg." << std::endl;
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        return false;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        return false;
    }

    packet->data = reinterpret_cast<uint8_t*>(const_cast<int16_t*>(pcmData.data()));
    packet->size = pcmData.size() * sizeof(int16_t);
    packet->stream_index = stream->index;
    
    // Establecer timestamps válidos para evitar advertencias de FFmpeg
    packet->pts = 0;
    packet->dts = 0;

    int ret = av_write_frame(fmtCtx, packet);
    av_packet_free(&packet);

    av_write_trailer(fmtCtx);

    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&fmtCtx->pb);
    }
    avformat_free_context(fmtCtx);

    return (ret >= 0);
}