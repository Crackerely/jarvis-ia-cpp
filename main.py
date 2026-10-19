#!/usr/bin/python3
import json
import threading
import sys
import jarvis

"""
para revisar el log de tail jarvis_listener.*, ejecutar tail -f /tmp/jarvis_listener.log
durante la ejecución de jarvis.
"""

# Inicializar el núcleo C++
jarvis_core = jarvis.AssistantCore()

print("[INFO] Jarvis C++ Core iniciado 100% nativo con CTranslate2, FFmpeg, e-speak y libcurl.")

def hum_thinking(lang):
    hum_text = (
        "estoy en eso mi negrito, dame tiempo mi cerebro es lento"
        if lang.startswith("es")
        else "working on it"
    )
    print(f"\n[JARVIS THINKING]: *{hum_text}*")
    jarvis_core.speak(hum_text, lang)

def check_current_playback_status():
    """Consulta el estado actual de la LinkedList y el progreso de la música"""
    title = jarvis_core.get_current_song_title()
    artist = jarvis_core.get_current_song_artist()
    progress = jarvis_core.get_current_song_progress()
    duration = jarvis_core.get_current_song_duration()
    queue_size = jarvis_core.get_queue_size()

    if title:
        status_msg = (
            f"Reproduciendo actualmente: {title}, de {artist}. "
            f"Progreso: {int(progress)} segundos transcurridos. "
            f"Hay {queue_size} canciones en espera en la cola."
        )
    else:
        status_msg = "No hay ninguna canción reproduciéndose en este momento."
    
    print(f"\n[ESTADO DE MÚSICA]: {status_msg}")
    jarvis_core.speak(status_msg, "es")

def process_user_command(user_text):
    if not user_text:
        return

    print(f"\n[COMANDO RECIBIDO]: {user_text}")
    lang = "es"

    if "que cancion es" in user_text.lower() or "que estas reproduciendo" in user_text.lower() or "estado de la musica" in user_text.lower():
        check_current_playback_status()
        return

    hum_thinking(lang)

    try:
        raw_intent_response = jarvis_core.query_ollama_intent(user_text)
        intent_data = json.loads(raw_intent_response)
        intent_json_str = intent_data.get("response", "{}").strip()
        
        if intent_json_str.startswith("```"):
            intent_json_str = intent_json_str.split("```")[1]
            if intent_json_str.startswith("json"):
                intent_json_str = intent_json_str[4:].strip()

        action_payload = json.loads(intent_json_str)
        action = action_payload.get("action", "chat")
        command = action_payload.get("command", "").lower()
        
        raw_payload = (
            action_payload.get("query", "")
            or action_payload.get("song", "")
            or action_payload.get("payload", "")
            or action_payload.get("url", "")
        )

        payload = raw_payload.replace("'", "").replace('"', "").strip()

        print(f"\n[DEBUG INTENT]: Acción -> {action} | Command -> {command} | Payload -> {payload}")

        # 1. EVALUAR CONTROL DE REPRODUCCIÓN (Evita caer en chat general)
        if action == "control_playback" or action in ["pause_music", "resume_music", "stop_music"]:
            if command == "pause" or action == "pause_music" or "pausa" in user_text.lower():
                jarvis_core.speak("Pausando audio", lang)
                jarvis_core.control_music("pause")
            elif command == "resume" or action == "resume_music" or "reanuda" in user_text.lower() or "continua" in user_text.lower():
                jarvis_core.speak("Reanudando audio", lang)
                jarvis_core.control_music("resume")
            elif command == "stop" or action == "stop_music" or "detener" in user_text.lower() or "para" in user_text.lower():
                jarvis_core.speak("Deteniendo audio", lang)
                jarvis_core.control_music("stop")
            return

        # 2. REPRODUCCIÓN DE MÚSICA
        elif action == "play_music" and payload:
            clean_payload_lower = payload.lower().strip()
            
            # Guardrail contra comandos de control filtrados como búsqueda
            if clean_payload_lower in ["resume", "resume music", "resume la musica", "continua", "continuar"]:
                jarvis_core.speak("Reanudando audio", lang)
                jarvis_core.control_music("resume")
                return
            elif clean_payload_lower in ["pause", "pause music", "pausa", "ponle pausa", "pausar"]:
                jarvis_core.speak("Pausando audio", lang)
                jarvis_core.control_music("pause")
                return

            jarvis_core.speak("Buscando audio", lang)
            direct_url = jarvis_core.find_youtube_url(payload)
            
            if direct_url:
                jarvis_core.speak("Reproduciendo audio", lang)
                jarvis_core.play_youtube_music_with_metadata(
                    title=payload,
                    artist="YouTube Stream",
                    url=direct_url,
                    duration=0.0
                )
            else:
                jarvis_core.speak("No pude encontrar el audio solicitado", lang)

        elif command == "salir" or command == "exit":
            print("\nCancelando todo...")
            jarvis_core.control_music("stop")
            jarvis_core.stop_listening()
            sys.exit(0)

        # 3. CHAT GENERAL
        else:
            raw_chat_response = jarvis_core.query_ollama(user_text)
            chat_data = json.loads(raw_chat_response)
            answer = chat_data.get("response", "").strip()

            if answer:
                print(f"\n[JARVIS]: {answer}")
                jarvis_core.speak(answer, lang)
            else:
                print("\n[JARVIS]: (Respuesta vacía)")
                jarvis_core.speak("No obtuve respuesta.", lang)

    except Exception as e:
        print(f"\n[ERROR PROCESANDO INTENT/JSON]: {e}")
        raw_response = jarvis_core.query_ollama(user_text)
        try:
            resp_data = json.loads(raw_response)
            answer = resp_data.get("response", "").strip()
            if answer:
                print(f"\n[JARVIS]: {answer}")
                jarvis_core.speak(answer, lang)
        except Exception:
            jarvis_core.speak("Error al procesar la solicitud.", lang)


if __name__ == "__main__":
    print("\n[LISTENING...] Dilo diciendo 'Jarvis activar <comando>', presiona 'e' para texto, 's' para estado de música o 'd' para salir.")
    
    jarvis_core.start_listening(lambda prompt: process_user_command(prompt))

    try:
        while True:
            key = jarvis_core.check_key()
            if key == "e" or key == "E":
                print("\n[TECLADO 'e']: Modo texto activado.")
                jarvis_core.speak("escribe lo que quieres nene", "es")
                user_text = input(
                    "\nEscribe tu comando para Jarvis (enter para enviarlo): "
                ).strip()
                process_user_command(user_text)

            elif key == "s" or key == "S":
                print("\n[TECLADO 's']: Consultando estado de la música...")
                check_current_playback_status()

            elif key == "d" or key == "D":
                print("\n[TECLADO 'd']: Cancelando todo...")
                jarvis_core.control_music("stop")
                jarvis_core.stop_listening()
                break
    except KeyboardInterrupt:
        jarvis_core.control_music("stop")
        jarvis_core.stop_listening()