#!/usr/bin/python3
import json
import threading
import sys
import jarvis

# Inicializar el núcleo C++
jarvis_core = jarvis.AssistantCore()

print("[INFO] Jarvis C++ Core iniciado 100% nativo con CTranslate2, FFmpeg, e-speak y libcurl.")

def hum_thinking(lang):
    """Feedback visual y auditivo de que Jarvis está procesando."""
    hum_text = (
        "estoy en eso mi negrito, dame tiempo mi cerebro es lento"
        if lang.startswith("es")
        else "working on it"
    )
    print(f"\n[JARVIS THINKING]: *{hum_text}*")
    jarvis_core.speak(hum_text, lang)

def process_user_command(user_text):
    """Procesa cualquier texto (proveniente de teclado o de la escucha por voz)."""
    if not user_text:
        return

    print(f"\n[COMANDO RECIBIDO]: {user_text}")
    lang = "es"
    hum_thinking(lang)

    try:
        # 1. Analizar el intent del usuario usando el enrutador de Ollama
        raw_intent_response = jarvis_core.query_ollama_intent(user_text)
        intent_data = json.loads(raw_intent_response)
        intent_json_str = intent_data.get("response", "{}").strip()
        
        # Limpiar posibles bloques markdown de código devueltos por el LLM
        if intent_json_str.startswith("```"):
            intent_json_str = intent_json_str.split("```")[1]
            if intent_json_str.startswith("json"):
                intent_json_str = intent_json_str[4:].strip()

        action_payload = json.loads(intent_json_str)
        action = action_payload.get("action", "chat")
        command = action_payload.get("command", "")
        
        payload = (
            action_payload.get("query", "")
            or action_payload.get("song", "")
            or action_payload.get("payload", "")
            or action_payload.get("url", "")
        )

        print(f"\n[DEBUG INTENT]: Acción -> {action} | Payload -> {payload}")

        if action == "play_music" and payload:
            jarvis_core.speak("Reproduciendo música", lang)
            jarvis_core.play_youtube_music(payload)

        elif action == "stop_music":
            jarvis_core.speak("Deteniendo música", lang)
            jarvis_core.control_music("stop")

        elif action == "pause_music":
            jarvis_core.speak("Pausando música", lang)
            jarvis_core.control_music("pause")

        elif action == "resume_music":
            jarvis_core.speak("Reanudando música", lang)
            jarvis_core.control_music("resume")

        elif command == "salir" or command == "exit":
            print("\nCancelando todo...")
            jarvis_core.control_music("stop")
            jarvis_core.stop_listening()
            sys.exit(0)

        else:
            # Chat general si no es un comando de música directo
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
    print("\n[LISTENING...] Dilo diciendo 'Jarvis activar <comando>', presiona 'e' para texto o 'd' para salir.")
    
    # Iniciar la escucha pasiva en segundo plano (Modo Alexa) conectada al procesador
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

            elif key == "d" or key == "D":
                print("\n[TECLADO 'd']: Cancelando todo...")
                jarvis_core.control_music("stop")
                jarvis_core.stop_listening()
                break
    except KeyboardInterrupt:
        jarvis_core.control_music("stop")
        jarvis_core.stop_listening()