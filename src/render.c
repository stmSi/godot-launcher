#include "render.h"
#include <stdlib.h>
#include <string.h>
#ifndef __APPLE__
#include "af/vulkan.h"
struct GlRenderer {
    AfVkRenderer *vk;
};
AfResult gl_window_create(const char *title, int width, int height, AfPlatformWindow *window) {
    return af_platform_window_create(title, width, height, window);
}
AfResult gl_renderer_create(AfPlatformWindow *window, GlRenderer **out) {
    GlRenderer *r = calloc(1, sizeof(*r));
    if (!r)
        return AF_RESULT_OUT_OF_MEMORY;
    AfResult result = af_vk_renderer_create(window, &r->vk);
    if (result != AF_RESULT_SUCCESS) {
        free(r);
        return result;
    }
    *out = r;
    return result;
}
AfResult gl_renderer_draw(GlRenderer *r, const AfScene *scene) {
    return af_vk_renderer_draw(r->vk, scene);
}
AfResult gl_renderer_capture_next(GlRenderer *r, const char *path) {
    return af_vk_renderer_capture_next(r->vk, path);
}
void gl_renderer_resize(GlRenderer *r) {
    af_vk_renderer_request_resize(r->vk);
}
bool gl_renderer_validation_errors(GlRenderer *r) {
    return r && af_vk_renderer_has_validation_errors(r->vk);
}
void gl_renderer_destroy(GlRenderer *r) {
    if (r) {
        af_vk_renderer_destroy(r->vk);
        free(r);
    }
}
#else
/* Quartz renders AF UI scene paths directly on macOS. The framework's Vulkan
 * renderer currently requires desktop Vulkan 1.3 rather than MoltenVK. */
#include "af/core/png.h"
#include "af/path/path.h"
#include <CoreGraphics/CoreGraphics.h>
struct GlRenderer {
    AfPlatformWindow *window;
    char *capture;
};
AfResult gl_window_create(const char *title, int width, int height, AfPlatformWindow *window) {
    memset(window, 0, sizeof(*window));
    if (!SDL_Init(SDL_INIT_VIDEO))
        return AF_RESULT_IO_ERROR;
    window->sdl_initialized = true;
    window->window = SDL_CreateWindow(title, width, height,
                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    return window->window ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
}
AfResult gl_renderer_create(AfPlatformWindow *window, GlRenderer **out) {
    GlRenderer *r = calloc(1, sizeof(*r));
    if (!r)
        return AF_RESULT_OUT_OF_MEMORY;
    r->window = window;
    *out = r;
    return AF_RESULT_SUCCESS;
}
AfResult gl_renderer_capture_next(GlRenderer *r, const char *path) {
    free(r->capture);
    r->capture = SDL_strdup(path);
    return r->capture ? AF_RESULT_SUCCESS : AF_RESULT_OUT_OF_MEMORY;
}
void gl_renderer_resize(GlRenderer *r) {
    (void)r;
}
bool gl_renderer_validation_errors(GlRenderer *r) {
    (void)r;
    return false;
}
void gl_renderer_destroy(GlRenderer *r) {
    if (r) {
        SDL_free(r->capture);
        free(r);
    }
}
static void draw_path(CGContextRef context, const AfPathData *data, const AfPath *path) {
    CGContextBeginPath(context);
    for (uint32_t i = 0; i < path->command_count; ++i) {
        const AfPathCommand *c = &data->commands[path->command_start + i];
        switch (c->type) {
        case AF_PATH_MOVE:
            CGContextMoveToPoint(context, c->point.x, c->point.y);
            break;
        case AF_PATH_LINE:
            CGContextAddLineToPoint(context, c->point.x, c->point.y);
            break;
        case AF_PATH_CUBIC:
            CGContextAddCurveToPoint(context, c->control1.x, c->control1.y, c->control2.x,
                                     c->control2.y, c->point.x, c->point.y);
            break;
        case AF_PATH_CLOSE:
            CGContextClosePath(context);
            break;
        }
    }
    if (path->fill_rule == AF_PATH_FILL_EVEN_ODD)
        CGContextEOFillPath(context);
    else
        CGContextFillPath(context);
}
typedef struct {
    uint32_t index;
    int32_t layer;
} Item;
static int order(const void *left, const void *right) {
    const Item *a = left, *b = right;
    return a->layer != b->layer ? (a->layer > b->layer ? 1 : -1)
                                : (a->index > b->index) - (a->index < b->index);
}
AfResult gl_renderer_draw(GlRenderer *r, const AfScene *scene) {
    SDL_Surface *surface = SDL_GetWindowSurface(r->window->window);
    if (!surface)
        return AF_RESULT_IO_ERROR;
    size_t stride = (size_t)surface->w * 4;
    unsigned char *pixels = calloc((size_t)surface->h, stride);
    if (!pixels)
        return AF_RESULT_OUT_OF_MEMORY;
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef c =
        CGBitmapContextCreate(pixels, (size_t)surface->w, (size_t)surface->h, 8, stride, space,
                              kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if (!c) {
        free(pixels);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    CGContextTranslateCTM(c, 0, surface->h);
    CGContextScaleCTM(c, 1, -1);
    CGContextSetShouldAntialias(c, true);
    Item items[AF_MAX_NODES];
    if (scene->count > AF_MAX_NODES) {
        CGContextRelease(c);
        free(pixels);
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t i = 0; i < scene->count; ++i)
        items[i] = (Item){i, scene->nodes[i].layer_order};
    qsort(items, scene->count, sizeof(*items), order);
    for (uint32_t i = 0; i < scene->count; ++i) {
        const AfSceneNode *n = &scene->nodes[items[i].index];
        if (!n->world_visible)
            continue;
        CGContextSaveGState(c);
        AfAffine2 t = n->world_transform;
        CGContextConcatCTM(c, CGAffineTransformMake(t.m00, t.m10, t.m01, t.m11, t.tx, t.ty));
        CGContextSetRGBFillColor(c, n->fill.r, n->fill.g, n->fill.b, n->fill.a * n->world_opacity);
        CGRect rect = CGRectMake(0, 0, n->bounds.x, n->bounds.y);
        if (n->type == AF_NODE_PATH) {
            const AfPath *p = af_path_find_node_const(scene->paths, n->id);
            if (p)
                draw_path(c, scene->paths, p);
        } else if (n->type == AF_NODE_ELLIPSE)
            CGContextFillEllipseInRect(c, rect);
        else if (n->type == AF_NODE_ROUNDED_RECTANGLE) {
            CGPathRef p =
                CGPathCreateWithRoundedRect(rect, n->corner_radius, n->corner_radius, NULL);
            CGContextAddPath(c, p);
            CGContextFillPath(c);
            CGPathRelease(p);
        } else if (n->type == AF_NODE_RECTANGLE)
            CGContextFillRect(c, rect);
        CGContextRestoreGState(c);
    }
    CGContextRelease(c);
    AfResult result = AF_RESULT_SUCCESS;
    if (r->capture) {
        result = af_png_write_rgba8(r->capture, pixels, (uint32_t)surface->w, (uint32_t)surface->h,
                                    (uint32_t)stride, AF_PIXEL_ORDER_RGBA);
        SDL_free(r->capture);
        r->capture = NULL;
    }
    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);
    bool ok = SDL_ConvertPixels(surface->w, surface->h, SDL_PIXELFORMAT_RGBA32, pixels, (int)stride,
                                surface->format, surface->pixels, surface->pitch);
    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
    free(pixels);
    if (!ok || !SDL_UpdateWindowSurface(r->window->window))
        return AF_RESULT_IO_ERROR;
    return result;
}
#endif
