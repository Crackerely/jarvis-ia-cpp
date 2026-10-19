#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include "assistant_core.h"
#include "jarvis_whisper.h"

namespace py = pybind11;

PYBIND11_MODULE(jarvis, m) {
    m.doc() = "Jarvis C++ Core API con CTranslate2, FFmpeg y libcurl nativo";

    py::class_<WhisperSegment>(m, "WhisperSegment")
        .def_readwrite("text", &WhisperSegment::text);

    py::class_<WhisperInfo>(m, "WhisperInfo")
        .def_readwrite("language", &WhisperInfo::language);

    py::class_<JarvisWhisper>(m, "jarvis_whisper")
        .def(py::init<const std::string&, const std::string&, const std::string&>(), py::arg("model_size") = "tiny", py::arg("device") = "cpu", py::arg("compute_type") = "int8")
        .def("transcribe", &JarvisWhisper::transcribe);

    py::class_<AssistantCore>(m, "AssistantCore")
        .def(py::init<>())
        .def("speak", &AssistantCore::speak)
        .def("control_music", &AssistantCore::controlMusic)
        .def("check_key", &AssistantCore::checkKey)
        .def("query_ollama", &AssistantCore::queryOllama)
        .def("query_ollama_intent", &AssistantCore::queryOllamaIntent)
        .def("record_and_save_wav", &AssistantCore::recordAndSaveWav)
        .def("play_youtube_music", &AssistantCore::playYouTubeMusic)
        .def("play_youtube_music_with_metadata", &AssistantCore::playYouTubeMusicWithMetadata, py::arg("title"), py::arg("artist"), py::arg("url"), py::arg("duration"))
        .def("find_youtube_url", &AssistantCore::findYouTubeUrl)
        .def("get_current_song_title", &AssistantCore::getCurrentSongTitle)
        .def("get_current_song_artist", &AssistantCore::getCurrentSongArtist)
        .def("get_current_song_progress", &AssistantCore::getCurrentSongProgress)
        .def("get_current_song_duration", &AssistantCore::getCurrentSongDuration)
        .def("get_queue_size", &AssistantCore::getQueueSize)
        .def("start_listening", &AssistantCore::startListening)
        .def("stop_listening", &AssistantCore::stopListening);
}