CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Iinclude -fPIC $(shell python3 -m pybind11 --includes)
LDFLAGS = -lportaudio -lctranslate2 -lcurl -lespeak-ng

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = builds

# Obtiene todos los headers automáticamente por si integras nuevos a futuro
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# Obtiene dinámicamente todos los .cpp de src/ (incluyendo assistant_core.cpp, jarvis_state.cpp, python_bindings.cpp, etc.)
ALL_SRCS = $(wildcard $(SRC_DIR)/*.cpp)

# Genera los objetos a partir de todos los archivos fuente encontrados en src/
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(ALL_SRCS))

TARGET_SO = jarvis.so

all: $(BUILD_DIR) $(TARGET_SO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Regla para compilar la librería compartida de Python (jarvis.so)
$(TARGET_SO): $(OBJS)
	$(CXX) -shared $(OBJS) -o $(TARGET_SO) $(LDFLAGS)

# Regla genérica para compilar .cpp a .o (recompila automáticamente si cambia algún header .h)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET_SO)