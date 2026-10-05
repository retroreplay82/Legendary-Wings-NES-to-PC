/* The engine keeps its command-line interface. Double-clicking this build
 * supplies the user's local ROM relative to the executable, not the CWD. */
#ifdef main
#undef main
#endif
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

int wings_engine_main(int argc, char **argv);

void wings_beta_register_hooks(void);
int main(int argc, char **argv) {
    wings_beta_register_hooks();
    if (argc != 1) return wings_engine_main(argc, argv);
    char rom[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, rom, sizeof(rom));
    if (!length || length >= sizeof(rom)) return 1;
    char *separator = strrchr(rom, '\\');
    if (!separator) return 1;
    *separator = '\0';
    char directory[MAX_PATH];strcpy(directory,rom);
    const char *suffixes[]={"\\roms\\Legendary Wings (USA).nes","\\..\\roms\\Legendary Wings (USA).nes"};
    bool found=false;
    for(int i=0;i<2;i++) {
        if(strlen(directory)+strlen(suffixes[i])+1>sizeof(rom))continue;
        strcpy(rom,directory);strcat(rom,suffixes[i]);
        if(GetFileAttributesA(rom)!=INVALID_FILE_ATTRIBUTES){found=true;break;}
    }
    if(!found) {
        MessageBoxA(NULL,"Place your Legendary Wings (USA).nes file in the roms folder beside the game, then launch again.",
                    "Legendary Wings - missing ROM",MB_OK|MB_ICONERROR);return 1;
    }
    /* Reset inherited headless test drivers only for an interactive launch. */
    SetEnvironmentVariableA("SDL_VIDEODRIVER", "windows");
    SetEnvironmentVariableA("SDL_AUDIODRIVER", NULL);
    SetEnvironmentVariableA("SDL_RENDER_DRIVER", "software");
    char *arguments[] = {argv[0], rom, "--enhanced-graphics", "--static-water", NULL};
    return wings_engine_main(4, arguments);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    extern int __argc;
    extern char **__argv;
    return main(__argc, __argv);
}
