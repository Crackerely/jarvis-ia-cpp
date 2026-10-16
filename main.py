import json
import jarvis

# Inicializar el núcleo C++
jarvis_core = jarvis.AssistantCore()

print("[INFO] Jarvis C++ Core iniciado con sockets nativos y FFmpeg streams.")


def hum_thinking(lang):
  """Feedback visual y auditivo de que Jarvis está procesando."""
  hum_text = (
      "estoy en eso maldita puta dame tiempo mi cerebro es lento"
      if lang.startswith("es")
      else "working on it"
  )
  print(f"\n[JARVIS THINKING]: *{hum_text}*")
  jarvis_core.speak(hum_text, lang)


if __name__ == "__main__":
  print(
      "\n[LISTENING...] Escribe 'e' para texto o presiona 'd' para salir/parar."
  )
  while True:
    key = jarvis_core.check_key()
    if key == "e" or key == "E":
      print("\n[TECLADO 'e']: Modo texto activado.")
      jarvis_core.speak("escribe lo que quieres nene", "es")
      user_text = input(
          "\nEscribe tu comando para Jarvis (enter para enviarlo): "
      ).strip()

      if user_text:
        lang = "es"

        # 1. Mostrar label y audio de pensando
        hum_thinking(lang)

        # 2. Petición HTTP nativa mediante sockets de C++
        raw_response = jarvis_core.query_ollama(user_text)

        try:
          # 3. Parsear el JSON para extraer únicamente la respuesta de la IA
          resp_data = json.loads(raw_response)
          answer = resp_data.get("response", "").strip()

          if answer:
            print(f"\n[JARVIS]: {answer}")
            # 4. Decir la respuesta en voz alta con C++
            jarvis_core.speak(answer, lang)
          else:
            print("\n[JARVIS]: (Respuesta vacía)")
            jarvis_core.speak("No obtuve respuesta.", lang)

        except Exception as e:
          print(f"\n[ERROR PARSING JSON]: {e}")
          print(f"[DEBUG RAW]: {raw_response}")
          jarvis_core.speak("Error al procesar la respuesta.", lang)

    elif key == "d" or key == "D":
      print("\n[TECLADO 'd']: Cancelando todo...")
      jarvis_core.control_music("stop")
