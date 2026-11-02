#include "app.h"
#include "af/ui/draw.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Thread *thread;
    atomic_bool done, cancel;
    atomic_uint_fast64_t bytes, total;
    atomic_int phase;
    int kind;
    bool success;
    char error[512];
    GlRelease release;
    GlLibrary library;
    GlCatalog catalog;
} Job;
typedef struct {
    GlLibrary *library;
    GlCatalog catalog;
    Job *job;
    AfPlatformWindow window;
    GlRenderer *renderer;
    AfAllocator allocator;
    AfTextEngine *text_engine;
    void *fonts[2];
    AfScene scene;
    AfPathData paths;
    AfUiContext ui;
    AfGlyphPlacement glyphs[1024];
    AfTextLine lines[4];
    AfStyle styles[128];
    AfStyleData style_data;
    float width, height, scale, scroll, max_scroll;
    float pointer_x, pointer_y;
    bool clicked, pressed, hand, search_focus, keyboard_activate;
    int page, channel, flavor, modal, focus, widget_count;
    char search[192], notice[512];
    bool error_notice;
    Uint64 notice_time;
    Uint32 dialog_event;
    double job_started;
    SDL_Cursor *arrow_cursor, *hand_cursor;
} App;
typedef struct {
    float x, y, w, h;
} Rect;
static AfColor color(unsigned value) {
    return (AfColor){((value >> 16) & 255) / 255.0f, ((value >> 8) & 255) / 255.0f,
                     (value & 255) / 255.0f, 1};
}
static const unsigned BG = 0x10141C, SIDE = 0x141A24, PANEL = 0x19212D, LINE = 0x293443,
                      INK = 0xEDF3F8, MUTED = 0x92A3B8, ACCENT = 0x80E5C4;
static void surface(App *a, Rect r, unsigned fill, float radius, int layer) {
    af_ui_add_surface(&a->ui, r.x, r.y, r.w, r.h, color(fill), radius, layer);
}
static void text(App *a, float x, float y, const char *s, float size, unsigned fill, bool bold,
                 float max) {
    a->ui.font_id = bold ? 2 : 1;
    af_ui_add_text(&a->ui, x, y, s, (size - 9) / 4, max, color(fill), 5);
}
static void line(App *a, float x, float y, float x2, float y2, unsigned fill, float width,
                 int layer) {
    af_ui_add_line(&a->ui, (AfVec2){x, y}, (AfVec2){x2, y2}, width, color(fill), layer);
}
static void circle(App *a, float x, float y, float radius, unsigned fill, int layer) {
    af_ui_add_shape(&a->ui, AF_NODE_ELLIPSE,
                    (AfAffine2){.m00 = 1, .m11 = 1, .tx = x - radius, .ty = y - radius},
                    (AfVec2){radius * 2, radius * 2}, color(fill), 0, layer);
}
/* Small vector icons share the framework scene with text and widgets. */
static void icon(App *a, int kind, float x, float y, float s, unsigned fill, int layer) {
    float u = s / 24;
    float t = 1.6f * u;
#define L(x1, y1, x2, y2)                                                                          \
    line(a, x + (x1) * u, y + (y1) * u, x + (x2) * u, y + (y2) * u, fill, t, layer)
    if (kind == 0) { /* engine */
        surface(a, (Rect){x + 3 * u, y + 7 * u, 18 * u, 14 * u}, fill, 4 * u, layer);
        surface(a, (Rect){x + 5 * u, y + 3 * u, 4 * u, 8 * u}, fill, 1 * u, layer);
        surface(a, (Rect){x + 15 * u, y + 3 * u, 4 * u, 8 * u}, fill, 1 * u, layer);
        circle(a, x + 8 * u, y + 12 * u, 2 * u, SIDE, layer + 1);
        circle(a, x + 16 * u, y + 12 * u, 2 * u, SIDE, layer + 1);
        line(a, x + 8 * u, y + 17 * u, x + 16 * u, y + 17 * u, SIDE, t, layer + 1);
    } else if (kind == 1) {
        L(3, 7, 10, 7);
        L(10, 7, 12, 10);
        L(12, 10, 21, 10);
        L(21, 10, 21, 20);
        L(21, 20, 3, 20);
        L(3, 20, 3, 7);
    } else if (kind == 2) {
        L(12, 3, 12, 16);
        L(7, 11, 12, 16);
        L(17, 11, 12, 16);
        L(4, 17, 4, 21);
        L(4, 21, 20, 21);
        L(20, 21, 20, 17);
    } else if (kind == 3) {
        circle(a, x + 12 * u, y + 12 * u, 8 * u, fill, layer);
        circle(a, x + 12 * u, y + 12 * u, 5 * u, SIDE, layer + 1);
        for (int i = 0; i < 8; ++i) {
            float v = (float)i * 0.785398f;
            L(12 + 7 * cosf(v), 12 + 7 * sinf(v), 12 + 11 * cosf(v), 12 + 11 * sinf(v));
        }
    } else if (kind == 4) {
        L(12, 4, 12, 20);
        L(4, 12, 20, 12);
    } else if (kind == 5) {
        L(8, 5, 17, 12);
        L(17, 12, 8, 19);
        L(8, 19, 8, 5);
    } else if (kind == 6) {
        L(5, 5, 19, 19);
        L(19, 5, 5, 19);
    } else if (kind == 7) {
        L(4, 12, 10, 18);
        L(10, 18, 20, 5);
    } else if (kind == 8) {
        circle(a, x + 10 * u, y + 10 * u, 6 * u, fill, layer);
        circle(a, x + 10 * u, y + 10 * u, 4.5f * u, BG, layer + 1);
        L(15, 15, 21, 21);
    } else if (kind == 9) {
        L(5, 5, 19, 5);
        L(19, 5, 19, 19);
        L(19, 19, 5, 19);
        L(5, 19, 5, 5);
        L(9, 1, 9, 5);
        L(15, 1, 15, 5);
        L(9, 19, 9, 23);
        L(15, 19, 15, 23);
    } else {
        L(4, 8, 4, 4);
        L(4, 4, 10, 4);
        L(4, 4, 8, 9);
        L(5, 17, 10, 20);
        L(10, 20, 17, 18);
        L(17, 18, 20, 12);
        L(20, 12, 17, 6);
    }
#undef L
}
static bool hot(App *a, Rect r) {
    return a->modal < 0 && a->pointer_x >= r.x && a->pointer_x <= r.x + r.w &&
           a->pointer_y >= r.y && a->pointer_y <= r.y + r.h;
}
static bool button(App *a, Rect r, const char *label, int kind, bool enabled) {
    enabled = enabled && a->modal < 0;
    int index = ++a->widget_count;
    bool hover = hot(a, r) && enabled, focused = a->focus == index && enabled;
    unsigned fill = kind == 1 ? ACCENT : kind == 2 ? 0x442830 : 0x222E3C;
    if (!enabled)
        fill = 0x1B2430;
    if (hover)
        a->hand = true;
    if (focused)
        surface(a, (Rect){r.x - 2, r.y - 2, r.w + 4, r.h + 4}, ACCENT, 9, 2);
    af_ui_add_button_rounded(&a->ui, r.x, r.y, r.w, r.h, color(fill), 7, 3);
    unsigned fg = !enabled ? 0x58697C : kind == 1 ? 0x10372E : kind == 2 ? 0xFFABB4 : INK;
    text(a, r.x + 14, r.y + (r.h - 16) / 2 - 1, label, 14, fg, true, r.w - 26);
    if (enabled && ((hover && a->clicked) || (focused && a->keyboard_activate))) {
        a->clicked = false;
        a->keyboard_activate = false;
        a->focus = index;
        return true;
    }
    return false;
}
static void notice(App *a, const char *s, bool error) {
    snprintf(a->notice, sizeof(a->notice), "%s", s);
    a->error_notice = error;
    a->notice_time = SDL_GetTicks();
}
static void change_page(App *a, int page) {
    if (a->page != page)
        a->search[0] = 0;
    a->page = page;
    a->scroll = 0;
    a->focus = 0;
    a->search_focus = false;
    SDL_StopTextInput(a->window.window);
}
static bool progress(void *user, uint64_t bytes, uint64_t total, const char *phase) {
    Job *j = user;
    atomic_store(&j->bytes, bytes);
    atomic_store(&j->total, total);
    int p = !strcmp(phase, "Verifying checksum")       ? 1
            : !strcmp(phase, "Extracting engine")      ? 2
            : !strcmp(phase, "Finishing installation") ? 3
                                                       : 0;
    atomic_store(&j->phase, p);
    return !atomic_load(&j->cancel);
}
static int worker(void *user) {
    Job *j = user;
    if (j->kind == 0)
        j->success = gl_fetch_catalog(j->library.root, &j->catalog, progress, j, j->error);
    else
        j->success = gl_install(&j->library, &j->release, progress, j, j->error);
    atomic_store_explicit(&j->done, true, memory_order_release);
    return 0;
}
static void start_job(App *a, const GlRelease *r) {
    if (a->job)
        return;
    Job *j = calloc(1, sizeof(*j));
    if (!j) {
        notice(a, "Not enough memory to start download", true);
        return;
    }
    atomic_init(&j->done, false);
    atomic_init(&j->cancel, false);
    atomic_init(&j->bytes, 0);
    atomic_init(&j->total, 0);
    atomic_init(&j->phase, 0);
    j->kind = r ? 1 : 0;
    j->library = *a->library;
    if (r)
        j->release = *r;
    j->thread = SDL_CreateThread(worker, "godot-transfer", j);
    if (!j->thread) {
        notice(a, SDL_GetError(), true);
        free(j);
        return;
    }
    a->job = j;
    a->job_started = (double)SDL_GetTicks() / 1000.0;
}
static void poll_job(App *a) {
    Job *j = a->job;
    if (!j || !atomic_load_explicit(&j->done, memory_order_acquire))
        return;
    SDL_WaitThread(j->thread, NULL);
    if (j->success) {
        if (j->kind == 0) {
            a->catalog = j->catalog;
            notice(a, "Release catalog is up to date", false);
        } else {
            *a->library = j->library;
            notice(a, "Engine verified and installed. Ready to launch.", false);
        }
    } else
        notice(a, j->error, true);
    free(j);
    a->job = NULL;
}
static const GlRelease *latest(App *a, bool mono) {
    const GlRelease *best = NULL;
    for (size_t i = 0; i < a->catalog.count; ++i) {
        const GlRelease *r = &a->catalog.items[i];
        if (!r->preview && r->mono == mono && (!best || gl_version_newer(r->tag, best->tag)))
            best = r;
    }
    return best;
}
static bool spawn_process(App *a, const char *const *args) {
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    SDL_Process *process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process) {
        notice(a, SDL_GetError(), true);
        return false;
    }
    SDL_DestroyProcess(process);
    return true;
}
static void launch(App *a, size_t index, const char *project) {
    if (index >= a->library->install_count)
        return;
    const GlInstall *v = &a->library->installs[index];
    char executable[GL_PATH], directory[GL_PATH];
    if (snprintf(executable, sizeof(executable), "%s/engines/%s/%s", a->library->root, v->id,
                 v->executable) >= (int)sizeof(executable)) {
        notice(a, "Engine path is too long", true);
        return;
    }
    const char *args[6] = {executable, "--project-manager", NULL, NULL, NULL, NULL};
    if (project) {
        snprintf(directory, sizeof(directory), "%s", project);
        char *slash = strrchr(directory, '/');
        char *back = strrchr(directory, '\\');
        if (back && (!slash || back > slash))
            slash = back;
        if (slash)
            *slash = 0;
        args[1] = "--editor";
        args[2] = "--path";
        args[3] = directory;
    }
    if (!spawn_process(a, args))
        return;
    notice(a, project ? "Opening project in Godot" : "Opening Godot Project Manager", false);
}
static void dialog_callback(void *user, const char *const *files, int filter) {
    (void)filter;
    Uint32 event_type = (Uint32)(uintptr_t)user;
    SDL_Event e = {0};
    e.type = event_type;
    if (files && files[0])
        e.user.data1 = SDL_strdup(files[0]);
    if (!SDL_PushEvent(&e))
        SDL_free(e.user.data1);
}
static void import_project(App *a) {
    static const SDL_DialogFileFilter filters[] = {{"Godot project", "godot"}};
    SDL_ShowOpenFileDialog(dialog_callback, (void *)(uintptr_t)a->dialog_event, a->window.window,
                           filters, 1, NULL, false);
}
static void pill(App *a, float x, float y, const char *label, unsigned fg, unsigned bg, float w) {
    surface(a, (Rect){x, y, w, 25}, bg, 5, 2);
    text(a, x + 9, y + 4, label, 12, fg, true, w - 16);
}
static void sidebar(App *a) {
    surface(a, (Rect){0, 0, 224, a->height}, SIDE, 0, 0);
    line(a, 223, 0, 223, a->height, LINE, 1, 1);
    surface(a, (Rect){24, 27, 40, 40}, 0x28483F, 10, 1);
    icon(a, 0, 31, 34, 26, ACCENT, 2);
    text(a, 77, 28, "GODOT", 20, INK, true, 126);
    text(a, 78, 53, "LAUNCHER", 10.5f, MUTED, true, 125);
    text(a, 26, 116, "WORKSPACE", 10.5f, 0x697F98, true, 180);
    const char *labels[] = {"My engines", "Projects", "Releases", "Settings"};
    for (int i = 0; i < 4; ++i) {
        float y = 145 + (float)i * 52;
        Rect r = {14, y, 196, 43};
        bool active = a->page == i;
        if (active) {
            surface(a, r, 0x24362F, 8, 1);
            surface(a, (Rect){14, y + 11, 3, 21}, ACCENT, 2, 2);
        }
        icon(a, i, 29, y + 11, 21, active ? ACCENT : MUTED, 2);
        text(a, 64, y + 12, labels[i], 14, active ? ACCENT : MUTED, active, 128);
        int index = ++a->widget_count;
        if (a->focus == index)
            line(a, 65, y + 36, 187, y + 36, ACCENT, 1, 4);
        if (hot(a, r)) {
            a->hand = true;
            if (a->clicked) {
                a->clicked = false;
                change_page(a, i);
            }
        }
        if (a->modal < 0 && a->focus == index && a->keyboard_activate) {
            a->keyboard_activate = false;
            change_page(a, i);
        }
    }
    float bottom = a->height - 158;
    surface(a, (Rect){18, bottom, 188, 70}, 0x1B2532, 8, 1);
    circle(a, 35, bottom + 24, 3, ACCENT, 2);
    text(a, 46, bottom + 15, "YOUR WORKSPACE", 10.5f, MUTED, true, 150);
    char info[100];
    snprintf(info, sizeof(info), "%zu engines  /  %zu projects", a->library->install_count,
             a->library->project_count);
    text(a, 30, bottom + 40, info, 12, INK, false, 164);
    line(a, 22, a->height - 65, 202, a->height - 65, LINE, 1, 1);
    text(a, 27, a->height - 44, "Built with Arcflow", 12, MUTED, false, 170);
    text(a, 27, a->height - 25, "Native. Lightweight. Yours.", 10.5f, 0x62768E, false, 180);
}
static void header(App *a) {
    float left = 258;
    const char *pages[] = {"My engines", "Projects", "Releases", "Settings"};
    text(a, left, 29, "Workspace", 12, MUTED, false, 110);
    text(a, left + 91, 29, "/", 12, 0x506079, false, 20);
    text(a, left + 110, 29, pages[a->page], 12, INK, false, 200);
    char platform[80];
    snprintf(platform, sizeof(platform), "%s / %s", gl_platform(), gl_arch());
    text(a, a->width - 190, 29, platform, 11.5f, MUTED, false, 160);
    line(a, 224, 66, a->width, 66, LINE, 1, 1);
}
static void hero(App *a, float left, float width) {
    Rect r = {left, 96, width, 216};
    surface(a, r, 0x1B3035, 13, 1);
    /* Orbit geometry, drawn as vectors rather than a decorative bitmap. */
    if (width > 620) {
        float cx = left + width - 125, cy = 203;
        for (int ring = 0; ring < 3; ++ring) {
            float radius = 53 + (float)ring * 30;
            for (int j = 0; j < 48; ++j) {
                float t = (float)j * .1309f, t2 = t + .09f;
                line(a, cx + cosf(t) * radius, cy + sinf(t) * radius * .64f, cx + cosf(t2) * radius,
                     cy + sinf(t2) * radius * .64f, 0x30504F, 1, 2);
            }
        }
        surface(a, (Rect){cx - 37, cy - 37, 74, 74}, 0x2C514C, 17, 2);
        icon(a, 0, cx - 25, cy - 27, 50, ACCENT, 3);
        circle(a, cx + 72, cy - 39, 5, ACCENT, 3);
        circle(a, cx - 91, cy + 19, 4, 0x69A2C4, 3);
    }
    text(a, left + 25, 116, "A LITTLE LESS SETUP. A LOT MORE CREATING.", 10.5f, ACCENT, true,
         width - 280);
    text(a, left + 25, 146, "Make room for your next idea.", 27, INK, true, width - 220);
    text(a, left + 25, 191, "Every Godot version. One place to build.", 14, 0xAEC7CC, false,
         width - 240);
    const GlRelease *r_latest = latest(a, false);
    char label[128];
    snprintf(label, sizeof(label), r_latest ? "Get Godot %s" : "Explore Godot releases",
             r_latest ? r_latest->tag : "");
    if (button(a, (Rect){left + 25, 244, 240, 41}, label, 1, true))
        change_page(a, 2);
    if (r_latest)
        pill(a, left + 281, 253, "LATEST STABLE", ACCENT, 0x26453F, 117);
}
static void empty(App *a, float y, const char *title, const char *description, const char *action,
                  int target) {
    float left = 258, width = a->width - left - 34, cx = left + width / 2;
    surface(a, (Rect){left, y, width, 209}, PANEL, 11, 1);
    surface(a, (Rect){cx - 24, y + 22, 48, 48}, 0x263746, 12, 2);
    icon(a, target == 1 ? 1 : 0, cx - 14, y + 32, 28, 0x7DB4CE, 3);
    text(a, cx - 150, y + 85, title, 19, INK, true, 330);
    text(a, cx - 240, y + 118, description, 13, MUTED, false, 490);
    if (button(a, (Rect){cx - 94, y + 154, 188, 35}, action, 0, true)) {
        if (target == 1)
            import_project(a);
        else if (a->page == 2 && target == 2)
            start_job(a, NULL);
        else
            change_page(a, target);
    }
}
static void engines(App *a) {
    float left = 258, width = a->width - left - 34;
    hero(a, left, width);
    text(a, left, 342, "Installed engines", 21, INK, true, 300);
    char count[64];
    snprintf(count, sizeof(count), "%zu installed", a->library->install_count);
    text(a, left + 192, 349, count, 12, MUTED, false, 140);
    if (button(a, (Rect){a->width - 179, 337, 145, 36}, "+ Add engine", 0, true))
        change_page(a, 2);
    if (!a->library->install_count) {
        empty(a, 393, "Your toolkit starts here",
              "Install an engine to launch projects and manage versions.", "Browse releases", 2);
        a->max_scroll = 0;
        return;
    }
    float start = 394;
    float bottom = a->height - (a->job ? 132 : 82);
    a->max_scroll = fmaxf(0, (float)a->library->install_count * 128 - (bottom - start));
    for (size_t i = 0; i < a->library->install_count; ++i) {
        float y = start + (float)i * 128 - a->scroll;
        if (y < start - 2 || y + 115 > bottom)
            continue;
        GlInstall *v = &a->library->installs[i];
        surface(a, (Rect){left, y, width, 113}, PANEL, 10, 1);
        surface(a, (Rect){left + 18, y + 20, 52, 52}, v->mono ? 0x37304D : 0x263F4C, 11, 2);
        icon(a, 0, left + 29, y + 29, 30, v->mono ? 0xC5ABF5 : 0x81B8DC, 3);
        char title[130];
        snprintf(title, sizeof(title), "Godot %s", v->tag);
        text(a, left + 87, y + 19, title, 19, INK, true, width - 360);
        text(a, left + 88, y + 48, v->mono ? ".NET / C# + GDScript" : "Standard / GDScript", 12,
             MUTED, false, 240);
        bool preferred = !strcmp(a->library->preferred, v->id);
        if (preferred)
            pill(a, left + 88, y + 75, "DEFAULT", ACCENT, 0x243E36, 76);
        else if (button(a, (Rect){left + 87, y + 75, 122, 26}, "Set default", 0, !a->job)) {
            snprintf(a->library->preferred, sizeof(a->library->preferred), "%s", v->id);
            char error[512];
            if (!gl_library_save(a->library, error))
                notice(a, error, true);
        }
        if (button(a, (Rect){left + width - 151, y + 20, 130, 37}, "Launch Godot", 1, true))
            launch(a, i, NULL);
        const GlRelease *r = latest(a, v->mono);
        if (r && gl_version_newer(r->tag, v->tag)) {
            char id[128];
            gl_install_id(r, id, sizeof(id));
            bool installed = gl_find_install(a->library, id) >= 0;
            if (button(a, (Rect){left + width - 266, y + 20, 105, 37},
                       installed ? "Up to date" : "Update", 0, !a->job && !installed))
                start_job(a, r);
        }
        if (button(a, (Rect){left + width - 151, y + 70, 130, 27}, "Remove", 2, !a->job))
            a->modal = (int)i;
    }
}
static void search_field(App *a, Rect r) {
    surface(a, r, a->search_focus ? 0x243344 : PANEL, 7, 1);
    icon(a, 8, r.x + 11, r.y + 9, 18, MUTED, 2);
    text(a, r.x + 38, r.y + 10, *a->search ? a->search : "Search by name or version...", 13,
         *a->search ? INK : MUTED, false, r.w - 62);
    if (a->search_focus) {
        size_t len = strlen(a->search);
        float caret = r.x + 38 + fminf((float)len * 7, r.w - 68);
        if ((SDL_GetTicks() / 500) % 2 == 0)
            line(a, caret, r.y + 10, caret, r.y + 26, ACCENT, 1, 3);
    }
    if (*a->search) {
        Rect clear = {r.x + r.w - 32, r.y, 32, r.h};
        icon(a, 6, clear.x + 9, clear.y + 13, 13, MUTED, 3);
        if (hot(a, clear)) {
            a->hand = true;
            if (a->clicked) {
                a->search[0] = 0;
                a->scroll = 0;
                a->clicked = false;
            }
        }
    }
    if (hot(a, r) && a->clicked) {
        a->clicked = false;
        a->search_focus = true;
        SDL_StartTextInput(a->window.window);
    }
}
static bool contains(const char *s, const char *query) {
    if (!*query)
        return true;
    size_t n = strlen(query);
    for (; *s; ++s) {
        bool match = true;
        for (size_t i = 0; i < n; ++i) {
            char c = s[i];
            if (!c || SDL_tolower((unsigned char)c) != SDL_tolower((unsigned char)query[i])) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}
static void releases(App *a) {
    float left = 258, width = a->width - left - 34;
    text(a, left, 95, "Find your next engine.", 29, INK, true, width - 180);
    text(a, left, 140, "Official Godot builds, ready when you are.", 14, MUTED, false, width - 160);
    if (button(a, (Rect){a->width - 160, 103, 126, 37},
               a->job && a->job->kind == 0 ? "Refreshing..." : "Refresh", 0, !a->job))
        start_job(a, NULL);
    search_field(a, (Rect){left, 185, width - 320, 39});
    const char *channels[] = {"Stable", "Preview", "All builds"};
    for (int i = 0; i < 3; ++i) {
        float x = left + width - 304 + (float)i * 99;
        if (button(a, (Rect){x, 185, 94, 39}, channels[i], a->channel == i ? 1 : 0, true)) {
            a->channel = i;
            a->scroll = 0;
        }
    }
    const char *flavors[] = {"Standard", ".NET / C#", "All editions"};
    for (int i = 0; i < 3; ++i)
        if (button(a, (Rect){left + (float)i * 126, 242, 116, 32}, flavors[i],
                   a->flavor == i ? 1 : 0, true)) {
            a->flavor = i;
            a->scroll = 0;
        }
    text(a, left + 15, 301, "VERSION / EDITION", 10.5f, MUTED, true, 260);
    text(a, left + width - 337, 301, "RELEASED", 10.5f, MUTED, true, 110);
    text(a, left + width - 225, 301, "SIZE", 10.5f, MUTED, true, 70);
    line(a, left, 325, left + width, 325, LINE, 1, 1);
    size_t visible[GL_MAX_RELEASES], count = 0;
    for (size_t i = 0; i < a->catalog.count; ++i) {
        const GlRelease *r = &a->catalog.items[i];
        if ((a->channel == 0 && r->preview) || (a->channel == 1 && !r->preview) ||
            (a->flavor == 0 && r->mono) || (a->flavor == 1 && !r->mono) ||
            !contains(r->tag, a->search))
            continue;
        visible[count++] = i;
    }
    float bottom = a->height - (a->job ? 132 : 82);
    a->max_scroll = fmaxf(0, (float)count * 85 - (bottom - 338));
    for (size_t row = 0; row < count; ++row) {
        float y = 338 + (float)row * 85 - a->scroll;
        if (y < 336 || y + 76 > bottom)
            continue;
        const GlRelease *r = &a->catalog.items[visible[row]];
        surface(a, (Rect){left, y, width, 76}, PANEL, 8, 1);
        surface(a, (Rect){left + 13, y + 17, 41, 41}, r->mono ? 0x37304D : 0x233C49, 9, 2);
        icon(a, 0, left + 21, y + 24, 25, r->mono ? 0xC5ABF5 : 0x81B8DC, 3);
        text(a, left + 68, y + 12, r->tag, 17, INK, true, width - 410);
        text(a, left + 69, y + 42, r->mono ? ".NET / C# + GDScript" : "Standard / GDScript", 12,
             MUTED, false, 220);
        char date[12];
        snprintf(date, sizeof(date), "%.10s", r->date);
        text(a, left + width - 337, y + 29, date, 12, MUTED, false, 105);
        char size[32];
        snprintf(size, sizeof(size), "%.0f MB", (double)r->size / 1048576);
        text(a, left + width - 225, y + 29, size, 12, MUTED, false, 74);
        char id[128];
        gl_install_id(r, id, sizeof(id));
        bool installed = gl_find_install(a->library, id) >= 0;
        if (button(a, (Rect){left + width - 132, y + 19, 115, 37},
                   installed ? "Installed" : "Install", installed ? 0 : 1, !installed && !a->job))
            start_job(a, r);
        Rect notes = {left + 65, y + 8, width - 420, 27};
        if (hot(a, notes)) {
            a->hand = true;
            if (a->clicked) {
                a->clicked = false;
                SDL_OpenURL(r->notes_url);
            }
        }
    }
    if (!count) {
        empty(a, 354, a->catalog.count ? "No matching builds" : "Let's fill your toolkit",
              a->catalog.count
                  ? "Try another channel, edition, or search."
                  : "Refresh to load official Godot releases. A connection is required.",
              "Refresh releases", 2);
    }
    char info[130];
    snprintf(info, sizeof(info), "%zu compatible builds  /  %s %s", count, gl_platform(),
             gl_arch());
    text(a, left, a->height - (a->job ? 155 : 69), info, 11, MUTED, false, width);
}
static void projects(App *a) {
    float left = 258, width = a->width - left - 34;
    text(a, left, 95, "Pick up where you left off.", 29, INK, true, width - 190);
    text(a, left, 140, "Your ideas, with the right engine close by.", 14, MUTED, false,
         width - 170);
    if (button(a, (Rect){a->width - 196, 102, 162, 38}, "+ Import project", 1, !a->job))
        import_project(a);
    search_field(a, (Rect){left, 183, width, 39});
    if (!a->library->project_count) {
        empty(a, 260, "Bring your projects along",
              "Import project.godot, or drop it anywhere in this window.", "Import a project", 1);
        a->max_scroll = 0;
        return;
    }
    size_t count = 0;
    float bottom = a->height - (a->job ? 132 : 82);
    for (size_t i = 0; i < a->library->project_count; ++i) {
        GlProject *p = &a->library->projects[i];
        if (!contains(p->name, a->search) && !contains(p->path, a->search))
            continue;
        float y = 251 + (float)count++ * 115 - a->scroll;
        if (y < 249 || y + 104 > bottom)
            continue;
        surface(a, (Rect){left, y, width, 103}, PANEL, 10, 1);
        surface(a, (Rect){left + 17, y + 20, 48, 48}, 0x2A354B, 11, 2);
        icon(a, 1, left + 28, y + 30, 26, 0xAAC4F3, 3);
        text(a, left + 82, y + 16, p->name, 18, INK, true, width - 290);
        text(a, left + 83, y + 47, p->path, 11.5f, MUTED, false, width - 250);
        int engine = gl_find_install(a->library, p->engine);
        if (engine < 0)
            engine = gl_find_install(a->library, a->library->preferred);
        char label[160];
        snprintf(label, sizeof(label), "Engine: %s",
                 engine >= 0 ? a->library->installs[engine].tag : "none installed");
        if (button(a, (Rect){left + 82, y + 72, 230, 24}, label, 0,
                   !a->job && a->library->install_count > 0)) {
            size_t next = engine >= 0 ? ((size_t)engine + 1) % a->library->install_count : 0;
            snprintf(p->engine, sizeof(p->engine), "%s", a->library->installs[next].id);
            char e[512];
            if (!gl_library_save(a->library, e))
                notice(a, e, true);
        }
        if (button(a, (Rect){left + width - 135, y + 19, 116, 36}, "Open editor", 1, engine >= 0))
            launch(a, (size_t)engine, p->path);
        if (button(a, (Rect){left + width - 135, y + 65, 116, 25}, "Forget project", 0, !a->job)) {
            GlProject old = *p;
            memmove(p, p + 1, (a->library->project_count - i - 1) * sizeof(*p));
            --a->library->project_count;
            char e[512];
            if (!gl_library_save(a->library, e)) {
                memmove(p + 1, p, (a->library->project_count - i) * sizeof(*p));
                *p = old;
                ++a->library->project_count;
                notice(a, e, true);
            }
            break;
        }
    }
    a->max_scroll = fmaxf(0, (float)count * 115 - (bottom - 251));
}
static void settings(App *a) {
    float left = 258, width = a->width - left - 34;
    text(a, left, 95, "A workspace that feels like yours.", 27, INK, true, width);
    text(a, left, 140, "Simple defaults. Everything in its place.", 14, MUTED, false, width);
    surface(a, (Rect){left, 194, width, 156}, PANEL, 11, 1);
    icon(a, 1, left + 21, 216, 22, ACCENT, 3);
    text(a, left + 58, 215, "Library location", 18, INK, true, width - 100);
    text(a, left + 24, 259, a->library->root, 12, MUTED, false, width - 48);
    if (button(a, (Rect){left + 24, 298, 144, 32}, "Open folder", 0, true)) {
#ifdef _WIN32
        const char *args[] = {"explorer.exe", a->library->root, NULL};
#elif defined(__APPLE__)
        const char *args[] = {"/usr/bin/open", a->library->root, NULL};
#else
        const char *args[] = {"xdg-open", a->library->root, NULL};
#endif
        spawn_process(a, args);
    }
    text(a, left + 183, 307, "Use --data-dir to keep a separate library.", 12, MUTED, false,
         width - 207);
    surface(a, (Rect){left, 370, width, 141}, PANEL, 11, 1);
    text(a, left + 24, 391, "Built for focus", 18, INK, true, width - 48);
    text(a, left + 24, 428, "Native C17 / Arcflow UI / native vector rendering", 13, MUTED, false,
         width - 48);
    text(a, left + 24, 463, "SHA-256 / SHA-512 verification and side-by-side engine updates.", 12,
         0xA8C9BF, false, width - 48);
    surface(a, (Rect){left, 532, width, 114}, PANEL, 11, 1);
    text(a, left + 24, 553, "Godot Launcher 0.1.0", 17, INK, true, width - 48);
    text(a, left + 24, 588,
         "Independent community tool. Downloads come from official Godot releases.", 12, MUTED,
         false, width - 48);
    text(a, left, 675,
         "Keyboard: Tab to focus / Enter to activate / Ctrl+F to search / Esc to close", 11, MUTED,
         false, width);
    a->max_scroll = 0;
}
static void transfer_panel(App *a) {
    if (!a->job)
        return;
    Job *j = a->job;
    float left = 244, width = a->width - 264, y = a->height - 123;
    surface(a, (Rect){left, y, width, 99}, 0x25342F, 11, 15);
    uint32_t first = a->scene.count;
    uint64_t bytes = atomic_load(&j->bytes), total = atomic_load(&j->total);
    int phase = atomic_load(&j->phase);
    const char *phases[] = {"Downloading", "Verifying checksum", "Extracting engine",
                            "Finishing installation"};
    char title[180];
    snprintf(title, sizeof(title), j->kind ? "%s / Godot %s" : "Refreshing official releases",
             phases[phase], j->release.tag);
    text(a, left + 19, y + 12, title, 14, INK, true, width - 150);
    float fraction = total ? (float)((double)bytes / (double)total) : .25f;
    if (fraction > 1)
        fraction = 1;
    surface(a, (Rect){left + 19, y + 44, width - 161, 5}, 0x40534B, 3, 16);
    surface(a, (Rect){left + 19, y + 44, (width - 161) * fraction, 5}, ACCENT, 3, 17);
    double elapsed = (double)SDL_GetTicks() / 1000.0 - a->job_started;
    char detail[160];
    if (total)
        snprintf(detail, sizeof(detail), "%.1f / %.1f MB  /  %.1f MB/s", (double)bytes / 1048576,
                 (double)total / 1048576, elapsed > 0 ? (double)bytes / 1048576 / elapsed : 0);
    else
        snprintf(detail, sizeof(detail), "%s",
                 phase ? "Preparing a verified engine installation"
                       : "Connecting securely to GitHub...");
    text(a, left + 19, y + 61, detail, 11.5f, 0xB0CBBE, false, width - 160);
    if (button(a, (Rect){left + width - 122, y + 30, 102, 35},
               atomic_load(&j->cancel) ? "Cancelling" : "Cancel", 0, !atomic_load(&j->cancel)))
        atomic_store(&j->cancel, true);
    for (uint32_t i = first; i < a->scene.count; ++i)
        a->scene.nodes[i].layer_order += 15;
}
static void modal(App *a) {
    if (a->modal < 0)
        return;
    AfColor overlay = {0.02f, 0.03f, 0.05f, .82f};
    af_ui_add_rect(&a->ui, 0, 0, a->width, a->height, overlay, 20);
    float x = (a->width - 490) / 2, y = (a->height - 230) / 2;
    surface(a, (Rect){x, y, 490, 230}, 0x24303F, 13, 21);
    /* Put modal text and controls above the overlay using a temporary node pass. */
    uint32_t first = a->scene.count;
    int index = a->modal;
    a->modal = -1;
    if (a->focus <= a->widget_count)
        a->focus = a->widget_count + 1;
    text(a, x + 25, y + 25, "Remove this engine?", 23, INK, true, 440);
    text(a, x + 25, y + 71, "Only this managed engine installation will be removed.", 13, MUTED,
         false, 440);
    text(a, x + 25, y + 101, "Your projects and other versions stay in your library.", 13, MUTED,
         false, 440);
    bool cancel = button(a, (Rect){x + 25, y + 164, 200, 39}, "Keep engine", 0, true);
    bool remove = button(a, (Rect){x + 245, y + 164, 220, 39}, "Remove engine", 2, true);
    for (uint32_t i = first; i < a->scene.count; ++i)
        a->scene.nodes[i].layer_order += 24;
    if (remove) {
        char error[512];
        if (!gl_remove(a->library, (size_t)index, error))
            notice(a, error, true);
        else
            notice(a, "Engine removed. Your projects are untouched.", false);
        a->scroll = 0;
    } else if (!cancel)
        a->modal = index;
}
static bool load_fonts(App *a) {
    af_allocator_init_default(&a->allocator);
    if (af_text_engine_create(&a->allocator, &a->text_engine) != AF_RESULT_SUCCESS)
        return false;
    const char *names[] = {"OpenSans-Regular.ttf", "OpenSans-SemiBold.ttf"};
    for (int i = 0; i < 2; ++i) {
        char path[GL_PATH];
        snprintf(path, sizeof(path), "%s/assets/fonts/%s", SDL_GetBasePath(), names[i]);
        size_t font_size = 0;
        a->fonts[i] = SDL_LoadFile(path, &font_size);
        if (!a->fonts[i]) {
            snprintf(path, sizeof(path), "%s/fonts/%s", GL_ASSET_DIR, names[i]);
            a->fonts[i] = SDL_LoadFile(path, &font_size);
        }
        if (!a->fonts[i] || font_size > 4 * 1024 * 1024 ||
            af_text_font_load_memory(a->text_engine, (uint64_t)i + 1, a->fonts[i], font_size, 0) !=
                AF_RESULT_SUCCESS)
            return false;
    }
    return true;
}
int gl_app(GlLibrary *library, bool offline, unsigned frames, const char *capture, int start_page) {
    App *a = calloc(1, sizeof(*a));
    if (!a)
        return 1;
    a->library = library;
    a->modal = -1;
    a->page = start_page >= 0 && start_page < 4 ? start_page : 0;
    a->paths = (AfPathData){.paths = calloc(AF_MAX_PATHS, sizeof(AfPath)),
                            .path_capacity = AF_MAX_PATHS,
                            .commands = calloc(AF_MAX_PATH_COMMANDS, sizeof(AfPathCommand)),
                            .command_capacity = AF_MAX_PATH_COMMANDS};
    AfSceneNode *nodes = calloc(AF_MAX_NODES, sizeof(*nodes));
    AfResult result = AF_RESULT_OUT_OF_MEMORY;
    if (!nodes || !a->paths.paths || !a->paths.commands)
        goto cleanup;
    af_scene_init(&a->scene, nodes, AF_MAX_NODES);
    a->scene.paths = &a->paths;
    a->style_data = (AfStyleData){.styles = a->styles, .capacity = 128};
    a->scene.styles = &a->style_data;
    if (!load_fonts(a)) {
        fprintf(stderr, "Could not load launcher fonts\n");
        goto cleanup;
    }
    result = gl_window_create("Godot Launcher — Arcflow", 1240, 840, &a->window);
    if (result != AF_RESULT_SUCCESS)
        goto cleanup;
    char icon_path[GL_PATH];
    snprintf(icon_path, sizeof(icon_path), "%s/assets/icon.bmp", SDL_GetBasePath());
    SDL_Surface *window_icon = SDL_LoadBMP(icon_path);
    if (window_icon) {
        SDL_SetWindowIcon(a->window.window, window_icon);
        SDL_DestroySurface(window_icon);
    }
    SDL_SetWindowMinimumSize(a->window.window, 1000, 760);
    SDL_SetAppMetadata("Godot Launcher", "0.1.0", "io.arcflow.GodotLauncher");
    result = gl_renderer_create(&a->window, &a->renderer);
    if (result != AF_RESULT_SUCCESS)
        goto cleanup;
    result = af_ui_init(&a->ui, &a->scene, &a->paths);
    if (result != AF_RESULT_SUCCESS)
        goto cleanup;
    a->ui.text_engine = a->text_engine;
    a->ui.glyphs = a->glyphs;
    a->ui.glyph_capacity = 1024;
    a->ui.lines = a->lines;
    a->ui.line_capacity = 4;
    a->ui.surface_styles = &a->style_data;
    a->arrow_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    a->hand_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    a->dialog_event = SDL_RegisterEvents(1);
    char error[512] = {0};
    gl_load_catalog(library->root, &a->catalog, error);
    if (!offline)
        start_job(a, NULL);
    bool running = true;
    unsigned frame = 0;
    Uint64 previous = SDL_GetTicksNS();
    while (running) {
        poll_job(a);
        a->clicked = false;
        a->keyboard_activate = false;
        SDL_Event e;
        AfPlatformWindowMetrics metrics;
        result = af_platform_window_metrics(&a->window, &metrics);
        if (result != AF_RESULT_SUCCESS)
            break;
        a->scale = (float)metrics.pixel_width / (float)metrics.logical_width;
        a->width = (float)metrics.logical_width;
        a->height = (float)metrics.logical_height;
        int previous_widgets = a->widget_count;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                gl_renderer_resize(a->renderer);
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
                a->clicked = true;
                a->pointer_x = e.button.x;
                a->pointer_y = e.button.y;
            } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                a->scroll = fmaxf(0, fminf(a->max_scroll, a->scroll - e.wheel.y * 45));
            } else if (e.type == SDL_EVENT_DROP_FILE) {
                if (!a->job) {
                    if (gl_add_project(library, e.drop.data, error)) {
                        notice(a, "Project added to your library", false);
                        change_page(a, 1);
                    } else
                        notice(a, error, true);
                }
            } else if (e.type == a->dialog_event) {
                if (e.user.data1) {
                    if (!a->job) {
                        if (gl_add_project(library, e.user.data1, error)) {
                            notice(a, "Project added to your library", false);
                            change_page(a, 1);
                        } else
                            notice(a, error, true);
                    }
                    SDL_free(e.user.data1);
                }
            } else if (e.type == SDL_EVENT_TEXT_INPUT && a->search_focus && a->modal < 0) {
                size_t n = strlen(a->search), add = strlen(e.text.text);
                if (n + add < sizeof(a->search)) {
                    memcpy(a->search + n, e.text.text, add + 1);
                    a->scroll = 0;
                }
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_ESCAPE) {
                    if (a->modal >= 0)
                        a->modal = -1;
                    else if (a->search_focus) {
                        a->search_focus = false;
                        SDL_StopTextInput(a->window.window);
                    } else
                        a->focus = 0;
                } else if (e.key.key == SDLK_TAB) {
                    a->focus += (e.key.mod & SDL_KMOD_SHIFT) ? -1 : 1;
                    int first_focus = a->modal >= 0 ? previous_widgets - 1 : 1;
                    if (a->focus < first_focus)
                        a->focus = previous_widgets;
                    if (a->focus > previous_widgets)
                        a->focus = first_focus;
                    a->search_focus = false;
                    SDL_StopTextInput(a->window.window);
                } else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) {
                    if (!a->search_focus)
                        a->keyboard_activate = true;
                } else if (e.key.key == SDLK_BACKSPACE && a->search_focus) {
                    size_t n = strlen(a->search);
                    if (n) {
                        do {
                            --n;
                        } while (n > 0 && ((unsigned char)a->search[n] & 0xC0) == 0x80);
                        a->search[n] = 0;
                        a->scroll = 0;
                    }
                } else if (a->modal < 0 && e.key.key == SDLK_F &&
                           (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
                    if (a->page != 1 && a->page != 2)
                        change_page(a, 2);
                    a->search_focus = true;
                    SDL_StartTextInput(a->window.window);
                } else if (a->modal < 0 && e.key.key == SDLK_V &&
                           (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) && a->search_focus) {
                    char *paste = SDL_GetClipboardText();
                    if (paste) {
                        snprintf(a->search, sizeof(a->search), "%s", paste);
                        SDL_free(paste);
                        a->scroll = 0;
                    }
                } else if (e.key.key == SDLK_PAGEDOWN)
                    a->scroll = fminf(a->max_scroll, a->scroll + 300);
                else if (e.key.key == SDLK_PAGEUP)
                    a->scroll = fmaxf(0, a->scroll - 300);
            }
        }
        if (!running)
            break;
        SDL_MouseButtonFlags buttons = SDL_GetMouseState(&a->pointer_x, &a->pointer_y);
        a->pressed = (buttons & SDL_BUTTON_LMASK) != 0;
        Uint64 now = SDL_GetTicksNS();
        float dt = (float)((double)(now - previous) / 1e9);
        previous = now;
        a->scene.count = 0;
        a->paths.path_count = 0;
        a->paths.command_count = 0;
        a->style_data.count = 0;
        a->hand = false;
        a->widget_count = 0;
        af_ui_begin_frame(&a->ui, dt, (AfVec2){a->pointer_x, a->pointer_y}, a->pressed);
        surface(a, (Rect){0, 0, a->width, a->height}, BG, 0, 0);
        sidebar(a);
        header(a);
        switch (a->page) {
        case 0:
            engines(a);
            break;
        case 1:
            projects(a);
            break;
        case 2:
            releases(a);
            break;
        default:
            settings(a);
            break;
        }
        a->scroll = fminf(a->scroll, a->max_scroll);
        if (a->max_scroll > 0) {
            float h = a->height - 450;
            float thumb = fmaxf(35, h * h / (h + a->max_scroll));
            surface(
                a, (Rect){a->width - 14, 338 + (h - thumb) * (a->scroll / a->max_scroll), 4, thumb},
                0x405269, 2, 8);
        }
        transfer_panel(a);
        if (*a->notice && SDL_GetTicks() - a->notice_time < 9000) {
            float y = a->height - (a->job ? 170 : 52);
            surface(a, (Rect){258, y, a->width - 292, 35}, a->error_notice ? 0x4C2C35 : 0x233C35, 7,
                    10);
            uint32_t n = a->scene.count;
            text(a, 271, y + 9, a->notice, 12, a->error_notice ? 0xFFB6C3 : 0xB4EAD7, false,
                 a->width - 320);
            for (uint32_t i = n; i < a->scene.count; ++i)
                a->scene.nodes[i].layer_order = 11;
        }
        modal(a);
        SDL_SetCursor(a->hand ? a->hand_cursor : a->arrow_cursor);
        if (a->ui.overflowed) {
            fprintf(stderr, "Arcflow UI capacity exceeded\n");
            result = AF_RESULT_LIMIT_EXCEEDED;
            break;
        }
        /* UI coordinates remain logical; Vulkan renders in physical pixels. */
        for (uint32_t i = 0; i < a->scene.count; ++i) {
            AfAffine2 *t = &a->scene.nodes[i].world_transform;
            t->m00 *= a->scale;
            t->m01 *= a->scale;
            t->m10 *= a->scale;
            t->m11 *= a->scale;
            t->tx *= a->scale;
            t->ty *= a->scale;
        }
        if (capture && frames && frame + 1 == frames)
            result = gl_renderer_capture_next(a->renderer, capture);
        if (result == AF_RESULT_SUCCESS)
            result = gl_renderer_draw(a->renderer, &a->scene);
        if (result != AF_RESULT_SUCCESS)
            break;
        if (frames && ++frame >= frames)
            break;
        SDL_Delay(8);
    }
    if (gl_renderer_validation_errors(a->renderer))
        result = AF_RESULT_VULKAN_ERROR;
cleanup:
    if (a->job) {
        atomic_store(&a->job->cancel, true);
        SDL_WaitThread(a->job->thread, NULL);
        free(a->job);
    }
    SDL_DestroyCursor(a->arrow_cursor);
    SDL_DestroyCursor(a->hand_cursor);
    gl_renderer_destroy(a->renderer);
    af_platform_window_destroy(&a->window);
    af_text_engine_destroy(a->text_engine);
    for (int i = 0; i < 2; ++i)
        SDL_free(a->fonts[i]);
    free(nodes);
    free(a->paths.paths);
    free(a->paths.commands);
    free(a);
    if (result != AF_RESULT_SUCCESS)
        fprintf(stderr, "Launcher: %s\n", af_result_string(result));
    return result == AF_RESULT_SUCCESS ? 0 : 1;
}
