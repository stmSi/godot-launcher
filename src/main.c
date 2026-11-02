#include "app.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    char *preference_path = SDL_GetPrefPath("Arcflow", "GodotLauncher");
    const char *root = preference_path, *capture = NULL, *install_tag = NULL;
    bool offline = false, list = false, mono = false;
    unsigned frames = 0;
    int page = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--data-dir") && i + 1 < argc)
            root = argv[++i];
        else if (!strcmp(argv[i], "--offline"))
            offline = true;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--capture") && i + 1 < argc)
            capture = argv[++i];
        else if (!strcmp(argv[i], "--page") && i + 1 < argc)
            page = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--list"))
            list = true;
        else if (!strcmp(argv[i], "--install") && i + 1 < argc)
            install_tag = argv[++i];
        else if (!strcmp(argv[i], "--mono"))
            mono = true;
        else if (!strcmp(argv[i], "--help")) {
            puts("Godot Launcher — powered by Arcflow\n\nUsage: godot-launcher [options]\n  "
                 "--data-dir PATH    Separate engine and project library\n  --offline          Use "
                 "the cached release catalog\n  --list             Print compatible releases\n  "
                 "--install TAG      Install a release without opening the UI\n  --mono            "
                 " Select the .NET build with --install\n  --frames N         Close after N "
                 "rendered frames\n  --capture PATH     Save the final rendered frame as PNG\n  "
                 "--page 0..3        Engines, projects, releases, settings");
            SDL_free(preference_path);
            return 0;
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            SDL_free(preference_path);
            return 1;
        }
    }
    char error[512] = {0};
    GlLibrary *library = calloc(1, sizeof(*library));
    if (!library || !root || !gl_init(error) || !gl_library_open(library, root, error)) {
        fprintf(stderr, "%s\n", *error ? error : "Cannot initialize launcher");
        if (!list && !install_tag)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Godot Launcher",
                                     *error ? error : "Cannot initialize launcher", NULL);
        free(library);
        SDL_free(preference_path);
        return 1;
    }
    void *lock = gl_library_lock(root, error);
    if (!lock) {
        fprintf(stderr, "%s\n", error);
        if (!list && !install_tag)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Godot Launcher", error, NULL);
        gl_shutdown();
        free(library);
        SDL_free(preference_path);
        return 1;
    }
    int result = 0;
    if (list || install_tag) {
        GlCatalog *catalog = calloc(1, sizeof(*catalog));
        bool ok = catalog && (offline ? gl_load_catalog(root, catalog, error)
                                      : gl_fetch_catalog(root, catalog, NULL, NULL, error));
        if (ok && list)
            for (size_t i = 0; i < catalog->count; ++i)
                printf("%-22s %-9s %6.1f MB\n", catalog->items[i].tag,
                       catalog->items[i].mono ? ".NET" : "Standard",
                       (double)catalog->items[i].size / 1048576.0);
        if (ok && install_tag) {
            ok = false;
            for (size_t i = 0; i < catalog->count; ++i)
                if (!strcmp(catalog->items[i].tag, install_tag) && catalog->items[i].mono == mono) {
                    ok = gl_install(library, &catalog->items[i], NULL, NULL, error);
                    break;
                }
            if (!ok && !*error)
                snprintf(error, sizeof(error), "Release was not found: %s", install_tag);
            if (ok)
                puts("Engine installed and verified.");
        }
        if (!ok) {
            fprintf(stderr, "%s\n", error);
            result = 1;
        }
        free(catalog);
    } else
        result = gl_app(library, offline, frames, capture, page);
    gl_library_unlock(lock);
    gl_shutdown();
    free(library);
    SDL_free(preference_path);
    return result;
}
