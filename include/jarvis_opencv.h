#ifndef JARVIS_OPENCV_H
#define JARVIS_OPENCV_H

#include <string>
#include <opencv2/opencv.hpp>

class JarvisOpenCV {
public:
    JarvisOpenCV();
    ~JarvisOpenCV();

    // Captura una foto desde la cámara por defecto usando OpenCV puro
    bool captureAndSaveSnapshot(const std::string& filename);
};

#endif
