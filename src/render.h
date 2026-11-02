#ifndef GL_RENDER_H
#define GL_RENDER_H
#include "af/platform/platform_sdl.h"
#include "af/scene/scene.h"
typedef struct GlRenderer GlRenderer;
AfResult gl_window_create(const char *title, int width, int height, AfPlatformWindow *window);
AfResult gl_renderer_create(AfPlatformWindow *window, GlRenderer **out);
AfResult gl_renderer_draw(GlRenderer *renderer, const AfScene *scene);
AfResult gl_renderer_capture_next(GlRenderer *renderer, const char *path);
void gl_renderer_resize(GlRenderer *renderer);
bool gl_renderer_validation_errors(GlRenderer *renderer);
void gl_renderer_destroy(GlRenderer *renderer);
#endif
