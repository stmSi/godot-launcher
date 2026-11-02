#ifndef GL_CORE_H
#define GL_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define GL_PATH 4096
#define GL_MAX_RELEASES 128
#define GL_MAX_INSTALLS 128
#define GL_MAX_PROJECTS 128
#define GL_RELEASE_API "https://api.github.com/repos/godotengine/godot-builds/releases?per_page=50"
typedef struct {
    char tag[96], asset[256], url[1024], digest[160], sums_url[1024], date[24], notes_url[512];
    uint64_t size;
    bool preview, mono;
} GlRelease;
typedef struct {
    GlRelease items[GL_MAX_RELEASES];
    size_t count;
} GlCatalog;
typedef struct {
    char id[128], tag[96], executable[GL_PATH];
    bool mono;
} GlInstall;
typedef struct {
    char path[GL_PATH], name[192], engine[128];
} GlProject;
typedef struct {
    char root[GL_PATH], preferred[128];
    GlInstall installs[GL_MAX_INSTALLS];
    size_t install_count;
    GlProject projects[GL_MAX_PROJECTS];
    size_t project_count;
} GlLibrary;
typedef bool (*GlProgress)(void *user, uint64_t done, uint64_t total, const char *phase);
const char *gl_platform(void);
const char *gl_arch(void);
bool gl_init(char *error);
void gl_shutdown(void);
void *gl_library_lock(const char *root, char *error);
void gl_library_unlock(void *lock);
bool gl_library_open(GlLibrary *library, const char *root, char *error);
bool gl_library_save(const GlLibrary *library, char *error);
bool gl_parse_catalog(const char *json, const char *platform, const char *arch, GlCatalog *out,
                      char *error);
bool gl_fetch_catalog(const char *root, GlCatalog *out, GlProgress progress, void *user,
                      char *error);
bool gl_load_catalog(const char *root, GlCatalog *out, char *error);
bool gl_install(GlLibrary *library, const GlRelease *release, GlProgress progress, void *user,
                char *error);
bool gl_install_archive(GlLibrary *library, const GlRelease *release, const char *archive_path,
                        const char *digest, GlProgress progress, void *user, char *error);
bool gl_remove(GlLibrary *library, size_t index, char *error);
bool gl_add_project(GlLibrary *library, const char *path, char *error);
bool gl_safe_relative(const char *path);
bool gl_verify(const char *path, const char *digest, char *error);
bool gl_version_newer(const char *left, const char *right);
void gl_install_id(const GlRelease *release, char *out, size_t capacity);
int gl_find_install(const GlLibrary *library, const char *id);
#endif
