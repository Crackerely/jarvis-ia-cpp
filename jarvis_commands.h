#ifndef JARVIS_COMMANDS_H
#define JARVIS_COMMANDS_H

typedef struct {
    const char *command;
    const char *trigger_command = "jarvis activar";
} JarvisCommandsStruct;

export const char JARVIS_DEFINED_COMMANDS[] = {
    "play_music",
    "view_movie"
};

class JarvisCommands {
private:
    hilos
public:
    explicit JarvisCommands::JarvisCommands();
};

#endif