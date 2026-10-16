#include "include/jarvis_opencv.h"
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>

JarvisOpenCV::JarvisOpenCV() {}
JarvisOpenCV::~JarvisOpenCV() {}

bool JarvisOpenCV::captureAndSaveSnapshot(const std::string& filename) {
    // Abrir la cámara por defecto (índice 0)
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "[OpenCV ERROR] No se pudo abrir la cámara." << std::endl;
        return false;
    }

    cv::Mat frame;
    // Capturar un frame
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "[OpenCV ERROR] Frame vacío capturado." << std::endl;
        return false;
    }

    // Guardar imagen en disco con OpenCV
    bool success = cv::imwrite(filename, frame);
    cap.release(); // Liberar recursos de la cámara
    return success;
}
