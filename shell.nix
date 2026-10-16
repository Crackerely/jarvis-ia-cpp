{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Compilación y herramientas base
    stdenv
    gcc
    gnumake
    pkg-config

    # Motores, multimedia y herramientas de red
    opencv
    espeak-ng
    tree
    mpv
    ffmpeg
    gdb
    portaudio
    alsa-lib
    ctranslate2
    yt-dlp # Herramienta CLI y núcleo de yt-dlp

    # Python y dependencias del ecosistema
    (python3.withPackages (ps: with ps; [
      pyaudio
      numpy
      pybind11
      ctranslate2
      yt-dlp # API de yt-dlp para Python si se requiere
    ]))
  ];

  shellHook = ''
    echo "¡Entorno Jarvis hiperoptimizado con CTranslate2, MeloTTS y yt-dlp activo!"
    export LD_LIBRARY_PATH="${pkgs.portaudio}/lib:${pkgs.ctranslate2}/lib:${pkgs.opencv}/lib:${pkgs.stdenv.cc.libc.out}/lib:$LD_LIBRARY_PATH"
  '';
}
