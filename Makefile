CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Iinclude -fPIC $(shell python3 -m pybind11 --includes) $(shell pkg-config --cflags opencv4 2>/dev/null || pkg-config --cflags opencv)
LDFLAGS = -lportaudio -lctranslate2 $(shell pkg-config --libs opencv4 2>/dev/null || pkg-config --libs opencv)

SRCS = assistant_core.cpp jarvis_voice.cpp jarvis_ffmpeg.cpp jarvis_opencv.cpp jarvis_melo.cpp jarvis_whisper.cpp python_bindings.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = jarvis.so

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -shared $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
