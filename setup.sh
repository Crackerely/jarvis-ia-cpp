#!/bin/bash
set -e

echo "=================================================="
echo "  Jarvis C++ CachyOS Hardware-Aware Setup (/opt/) "
echo "=================================================="

# Verificar privilegios de superusuario para pacman
SUDO=""
if [ "$EUID" -ne 0 ]; then
    if command -v sudo &> /dev/null; then
        SUDO="sudo"
    else
        echo "[-] Error: Este script requiere privilegios de root o sudo para instalar paquetes."
        exit 1
    fi
fi

# --- 1. AISLAMIENTO EN /opt/jarvis ---
TARGET_DIR="/opt/jarvis"
echo "[+] Configurando el aislamiento del proyecto en ${TARGET_DIR}..."

$SUDO mkdir -p "$TARGET_DIR"

# Si no estamos ejecutando ya desde /opt/jarvis, copiamos los archivos actuales
if [ "$PWD" != "$TARGET_DIR" ]; then
    echo "[+] Copiando archivos del proyecto a ${TARGET_DIR}..."
    $SUDO cp -r ./* "$TARGET_DIR"/
fi

# Asegurar que el usuario actual sea dueño de /opt/jarvis para compilar sin sudo
CURRENT_USER=$(whoami)
$SUDO chown -R "$CURRENT_USER:$CURRENT_USER" "$TARGET_DIR"

# Movernos al directorio de trabajo oficial en /opt/jarvis
cd "$TARGET_DIR"

# --- 2. INSTALACIÓN DE DEPENDENCIAS CON PACMAN ---
echo "[+] Actualizando repositorios y sincronizando paquetes en CachyOS..."
$SUDO pacman -Syu --noconfirm

echo "[+] Instalando herramientas de compilación y librerías de sistema..."
$SUDO pacman -S --needed --noconfirm \
    base-devel \
    cmake \
    pkgconf \
    portaudio \
    curl \
    ffmpeg \
    espeak-ng \
    python \
    python-pip \
    pybind11 \
    git \
    zstd \
    lshw \
    json-c

# --- 3. DETECCIÓN DE HARDWARE (NVIDIA / CUDA) ---
echo "[+] Detectando hardware del sistema..."
USE_CUDA="OFF"
WITH_MKL="OFF"

if command -v nvidia-smi &> /dev/null || $SUDO lshw | grep -i nvidia &> /dev/null; then
    echo "[*] ¡GPU NVIDIA detectada!"
    if [ -d "/usr/local/cuda" ] || command -v nvcc &> /dev/null; then
        echo "[+] CUDA Toolkit encontrado. Habilitando soporte CUDA para CTranslate2."
        USE_CUDA="ON"
    else
        echo "[!] GPU NVIDIA detectada, pero el Toolkit de CUDA no está instalado. Continuando en modo CPU."
    fi
else
    echo "[*] No se detectó GPU NVIDIA. Usando modo exclusivo CPU."
fi

# --- 4. CTRANSLATE2 ---
if ldconfig -p | grep -q ctranslate2 || [ -f /usr/local/lib/libctranslate2.so ] || [ -f /usr/lib/libctranslate2.so ]; then
    echo "[+] CTranslate2 ya está instalado en el sistema."
else
    echo "[!] CTranslate2 no encontrado. Clonando y compilando desde la fuente..."
    rm -rf /tmp/CTranslate2
    git clone --recursive https://github.com/OpenNMT/CTranslate2.git /tmp/CTranslate2
    cd /tmp/CTranslate2
    
    # Compilación corregida con la política de versión mínima de CMake para versiones modernas
    mkdir -p build && cd build
    cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DWITH_CUDA="$USE_CUDA" -DWITH_MKL="$WITH_MKL" -DOPENMP_RUNTIME=COMP
    make -j$(nproc)
    $SUDO make install
    $SUDO ldconfig
    cd "$TARGET_DIR"
    echo "[+] ¡CTranslate2 instalado con éxito!"
fi

# --- 5. COPIA DE CABECERAS LOCALES ---
echo "[+] Copiando cabeceras locales de include/* a /usr/include..."
if [ -d "include" ]; then
    $SUDO cp -r include/* /usr/include/
    echo "[+] Cabeceras copiadas correctamente."
else
    echo "[!] Advertencia: No se encontró el directorio 'include'."
fi

# --- 6. OLLAMA Y MODELO LLAMA3.2 ---
echo "[+] Configurando Ollama..."
if command -v ollama &> /dev/null; then
    echo "[+] Ollama ya está instalado."
else
    echo "[+] Instalando Ollama mediante script oficial..."
    curl -fsSL https://ollama.com/install.sh | sh
fi

echo "[+] Iniciando daemon de Ollama en segundo plano..."
pkill ollama || true
sleep 2
nohup ollama serve > /tmp/ollama.log 2>&1 &

echo "[+] Esperando a que el servidor local de Ollama responda..."
until curl -s http://127.0.0.1:11434 > /dev/null; do
    sleep 1
done
echo "[+] ¡Servidor de Ollama activo en el puerto 11434!"

echo "[+] Verificando modelo llama3.2..."
if ollama list | grep -q "llama3.2"; then
    echo "[+] El modelo llama3.2 ya se encuentra disponible."
else
    echo "[+] Descargando modelo llama3.2 (puede tardar unos minutos)..."
    ollama pull llama3.2
fi

echo "=================================================="
echo "  ¡Configuración completa en /opt/jarvis!         "
echo "  Ahora puedes ejecutar: make clean && make all   "
echo "=================================================="