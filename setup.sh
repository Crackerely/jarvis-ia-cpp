#!/bin/bash
set -e

echo "=========================================="
echo "  Jarvis C++ Hardware-Aware Setup Script"
echo "=========================================="

# Check for root or sudo privileges
SUDO=""
if [ "$EUID" -ne 0 ]; then
    if command -v sudo &> /dev/null; then
        SUDO="sudo"
    else
        echo "[-] Error: This script requires root privileges or sudo to install packages."
        exit 1
    fi
fi

# --- Hardware Detection ---
echo "[+] Detecting system hardware..."
USE_CUDA="OFF"
WITH_MKL="OFF"

if command -v nvidia-smi &> /dev/null || lspci | grep -i nvidia &> /dev/null; then
    echo "[*] NVIDIA GPU detected!"
    if [ -d "/usr/local/cuda" ] || command -v nvcc &> /dev/null; then
        echo "[+] CUDA Toolkit found. Enabling CUDA support for CTranslate2."
        USE_CUDA="ON"
    else
        echo "[!] NVIDIA GPU found, but CUDA Toolkit is not installed. Continuing with CPU-only mode."
    fi
else
    echo "[*] No NVIDIA GPU detected. Using CPU-only mode."
fi

echo "[+] Updating package lists..."
$SUDO apt-get update

echo "[+] Installing build tools and required system libraries..."
$SUDO apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libportaudio2 \
    portaudio19-dev \
    libcurl4-openssl-dev \
    python3-dev \
    python3-pip \
    python3-venv \
    git

echo "[+] Installing Python pybind11 package..."
python3 -m pip install --user pybind11

# Check for CTranslate2 library
if ldconfig -p | grep -q ctranslate2 || [ -f /usr/local/lib/libctranslate2.so ] || [ -f /usr/lib/libctranslate2.so ]; then
    echo "[+] CTranslate2 is already installed."
else
    echo "[!] CTranslate2 library not found on the system."
    read -p "[?] Would you like to automatically clone, build, and install CTranslate2 from source? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "[+] Cloning and building CTranslate2 with CUDA=$USE_CUDA, OpenMP=COMP..."
        rm -rf /tmp/CTranslate2
        git clone --recursive https://github.com/OpenNMT/CTranslate2.git /tmp/CTranslate2
        cd /tmp/CTranslate2
        mkdir -p build && cd build
        
        cmake -DWITH_CUDA="$USE_CUDA" -DWITH_MKL="$WITH_MKL" -DOPENMP_RUNTIME=COMP ..
        make -j$(nproc)
        $SUDO make install
        $SUDO ldconfig
        cd - > /dev/null
        echo "[+] CTranslate2 installed successfully!"
    else
        echo "[!] Skipping CTranslate2 installation. Please ensure it is available before running 'make'."
    fi
fi

# --- Copiar cabeceras locales a /usr/include ---
echo "[+] Copiando cabeceras locales de include/* a /usr/include..."
if [ -d "include" ]; then
    $SUDO cp -r include/* /usr/include/
    echo "[+] Cabeceras instaladas correctamente en /usr/include."
else
    echo "[!] Advertencia: No se encontró el directorio 'include'."
fi

# --- Verificación e Instalación de espeak-ng desde el código fuente ---
if ldconfig -p | grep -q espeak-ng || [ -f /usr/local/lib/libespeak-ng.so ] || [ -f /usr/lib/libespeak-ng.so ]; then
    echo "[+] libespeak-ng ya está instalado."
else
    echo "[!] libespeak-ng no encontrado en el sistema. Clonando y compilando desde la fuente..."
    rm -rf /tmp/espeak-ng
    git clone --recursive https://github.com/espeak-ng/espeak-ng.git /tmp/espeak-ng
    cd /tmp/espeak-ng
    
    # Compilación e instalación nativa con autotools
    ./autogen.sh
    ./configure
    make -j$(nproc)
    $SUDO make install
    $SUDO ldconfig
    cd - > /dev/null
    echo "[+] libespeak-ng instalado y enlazado correctamente."
fi

# --- Instalación y Configuración de Ollama (Daemon persistente) ---
echo "[+] Configurando Ollama..."
if command -v ollama &> /dev/null; then
    echo "[+] Ollama ya está instalado en el sistema."
else
    echo "[+] Instalando Ollama mediante script oficial..."
    curl -fsSL https://ollama.com/install.sh | sh
fi

# Reiniciar y dejar corriendo Ollama en segundo plano como daemon persistente
echo "[+] Iniciando daemon de Ollama (logs detallados en /tmp/ollama.log)..."
pkill ollama || true
sleep 2
nohup ollama serve > /tmp/ollama.log 2>&1 &

# Esperar activamente a que el servidor de Ollama esté listo y respondiendo peticiones
echo "[+] Esperando a que el servidor local de Ollama responda..."
until curl -s http://127.0.0.1:11434 > /dev/null; do
    sleep 1
done
echo "[+] ¡Servidor de Ollama activo y escuchando en el puerto 11434!"

# Verificar y descargar el modelo por defecto 'llama3.2' si no existe
echo "[+] Verificando modelo por defecto (llama3.2)..."
if ollama list | grep -q "llama3.2"; then
    echo "[+] El modelo llama3.2 ya está disponible localmente."
else
    echo "[+] Descargando modelo llama3.2 con ollama pull (esto puede tardar unos minutos)..."
    ollama pull llama3.2
fi

if [ "$USE_CUDA" == "ON" ]; then
    echo "[*] Ollama configurado como daemon persistente con soporte para GPU NVIDIA (CUDA)."
else
    echo "[*] Ollama configurado como daemon persistente en modo CPU."
fi

echo "=========================================="
echo "  Setup complete! You can now run 'make'. "
echo "=========================================="