#include "core.h"
#include <archive.h>
#include <archive_entry.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #x, error);               \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static char error[512];
static void write_zip(const char *path, const char *name, bool symlink) {
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    CHECK(archive_write_open_filename(a, path) == ARCHIVE_OK);
    struct archive_entry *e = archive_entry_new();
    archive_entry_set_pathname(e, name);
    archive_entry_set_filetype(e, symlink ? AE_IFLNK : AE_IFREG);
    archive_entry_set_perm(e, 0755);
    if (symlink)
        archive_entry_set_symlink(e, "/tmp/outside");
    else
        archive_entry_set_size(e, 3);
    CHECK(archive_write_header(a, e) == ARCHIVE_OK);
    if (!symlink)
        CHECK(archive_write_data(a, "abc", 3) == 3);
    archive_entry_free(e);
    CHECK(archive_write_close(a) == ARCHIVE_OK);
    archive_write_free(a);
}
static void zip_digest(const char *path, char *out) {
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    CHECK(ctx != NULL);
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    unsigned char buffer[4096], hash[32];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)))
        EVP_DigestUpdate(ctx, buffer, n);
    unsigned len;
    CHECK(EVP_DigestFinal_ex(ctx, hash, &len) == 1);
    fclose(f);
    EVP_MD_CTX_free(ctx);
    strcpy(out, "sha256:");
    for (unsigned i = 0; i < len; ++i)
        snprintf(out + 7 + i * 2, 3, "%02x", hash[i]);
}
static bool cancel_extract(void *user, uint64_t done, uint64_t total, const char *phase) {
    (void)user;
    (void)done;
    (void)total;
    return strcmp(phase, "Extracting engine") != 0;
}
int main(void) {
    CHECK(gl_init(error));
    CHECK(!gl_safe_relative("../escape"));
    CHECK(!gl_safe_relative("/absolute"));
    CHECK(!gl_safe_relative("C:/absolute"));
    CHECK(!gl_safe_relative("folder/../escape"));
    CHECK(!gl_safe_relative("folder\\escape"));
    CHECK(!gl_safe_relative(".. /escape"));
    CHECK(!gl_safe_relative("folder/../escape"));
    CHECK(!gl_safe_relative("NUL.txt"));
    CHECK(!gl_safe_relative("folder/com1.exe"));
    CHECK(!gl_safe_relative("folder./escape"));
    CHECK(gl_safe_relative("Godot.app/Contents/MacOS/Godot"));
    CHECK(gl_version_newer("4.10-stable", "4.9.2-stable"));
    CHECK(gl_version_newer("4.7.2-stable", "4.7.1-stable"));
    CHECK(!gl_version_newer("4.7-stable", "4.7.2-stable"));
    CHECK(gl_version_newer("4.8-stable", "4.8-beta2"));
    const char *fixture = "[{\"tag_name\":\"4.7.2-stable\",\"prerelease\":false,\"published_at\":"
                          "\"2026-08-18\",\"assets\":["
                          "{\"name\":\"Godot_v4.7.2-stable_linux.x86_64.zip\",\"size\":123,"
                          "\"digest\":\"sha256:abc\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_mono_linux_x86_64.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_linux.arm64.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_linux.x86_64.debugsymbols.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_win64.exe.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_mono_win64.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_windows_arm64.exe.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_macos.universal.zip\"},"
                          "{\"name\":\"Godot_v4.7.2-stable_mono_macos.universal.zip\"}]}]";
    GlCatalog *catalog = calloc(1, sizeof(*catalog));
    CHECK(catalog != NULL);
    CHECK(gl_parse_catalog(fixture, "linux", "x86_64", catalog, error));
    CHECK(catalog->count == 2);
    CHECK(catalog->items[1].mono);
    CHECK(catalog->items[0].size == 123);
    CHECK(gl_parse_catalog(fixture, "linux", "arm64", catalog, error));
    CHECK(catalog->count == 1);
    CHECK(gl_parse_catalog(fixture, "windows", "x86_64", catalog, error));
    CHECK(catalog->count == 2);
    CHECK(gl_parse_catalog(fixture, "windows", "arm64", catalog, error));
    CHECK(catalog->count == 1);
    CHECK(gl_parse_catalog(fixture, "macos", "arm64", catalog, error));
    CHECK(catalog->count == 2);
    CHECK(!gl_parse_catalog("{\"message\":\"API rate limit exceeded\"}", "linux", "x86_64", catalog,
                            error));
    free(catalog);
    char root[GL_PATH];
    const char *tmp = getenv("TMPDIR");
    if (!tmp)
        tmp = getenv("TEMP");
    if (!tmp)
        tmp = "/tmp";
    snprintf(root, sizeof(root), "%s/arcflow-launcher-test-%ld-%d", tmp, (long)time(NULL),
             getpid());
    GlLibrary *library = calloc(1, sizeof(*library));
    CHECK(library != NULL);
    CHECK(gl_library_open(library, root, error));
    char file[GL_PATH + 32];
    snprintf(file, sizeof(file), "%s/abc", root);
    FILE *f = fopen(file, "wb");
    CHECK(f != NULL);
    CHECK(fwrite("abc", 1, 3, f) == 3);
    CHECK(fclose(f) == 0);
    const char *known = "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    CHECK(gl_verify(file, known, error));
    CHECK(!gl_verify(
        file, "sha256:0000000000000000000000000000000000000000000000000000000000000000", error));
    CHECK(!gl_verify(file, "sha256:bad", error));
    GlRelease release = {0};
    strcpy(release.tag, "4.7.2-stable");
    char executable[256];
#if defined(__APPLE__)
    strcpy(executable, "Godot.app/Contents/MacOS/Godot");
    strcpy(release.asset, "Godot_v4.7.2-stable_macos.universal.zip");
#elif defined(_WIN32)
    snprintf(executable, sizeof(executable), "Godot_v4.7.2-stable_%s.exe",
             !strcmp(gl_arch(), "arm64") ? "windows_arm64" : "win64");
    snprintf(release.asset, sizeof(release.asset), "%.240s.zip", executable);
#else
    snprintf(executable, sizeof(executable), "Godot_v4.7.2-stable_linux.%s", gl_arch());
    snprintf(release.asset, sizeof(release.asset), "%.240s.zip", executable);
#endif
    char zip[GL_PATH + 32], digest[160];
    snprintf(zip, sizeof(zip), "%s/engine.zip", root);
    write_zip(zip, executable, false);
    zip_digest(zip, digest);
    CHECK(!gl_install_archive(library, &release, zip, known, NULL, NULL, error));
    CHECK(library->install_count == 0);
    CHECK(!gl_install_archive(library, &release, zip, digest, cancel_extract, NULL, error));
    CHECK(library->install_count == 0);
    CHECK(!strcmp(error, "Cancelled"));
    CHECK(gl_install_archive(library, &release, zip, digest, NULL, NULL, error));
    CHECK(library->install_count == 1);
    CHECK(!strcmp(library->preferred, "4.7.2-stable-standard"));
    CHECK(!gl_install_archive(library, &release, zip, digest, NULL, NULL, error));
    CHECK(library->install_count == 1);
    GlLibrary *reloaded = calloc(1, sizeof(*reloaded));
    CHECK(reloaded != NULL);
    CHECK(gl_library_open(reloaded, root, error));
    CHECK(reloaded->install_count == 1);
    CHECK(!strcmp(reloaded->installs[0].executable, executable));
    free(reloaded);
    char project[GL_PATH + 32];
    snprintf(project, sizeof(project), "%s/project.godot", root);
    f = fopen(project, "wb");
    CHECK(f != NULL);
    fputs("[application]\nconfig/name=\"My Test Project\"\n", f);
    fclose(f);
    CHECK(gl_add_project(library, project, error));
    CHECK(library->project_count == 1);
    CHECK(!strcmp(library->projects[0].name, "My Test Project"));
    CHECK(gl_add_project(library, project, error));
    CHECK(library->project_count == 1);
    CHECK(gl_remove(library, 0, error));
    CHECK(library->install_count == 0);
    CHECK(library->project_count == 1);
    CHECK(!*library->preferred);
    f = fopen(project, "rb");
    CHECK(f != NULL);
    fclose(f);
    write_zip(zip, "../escape", false);
    zip_digest(zip, digest);
    CHECK(!gl_install_archive(library, &release, zip, digest, NULL, NULL, error));
    CHECK(strstr(error, "Unsafe") != NULL);
    write_zip(zip, executable, true);
    zip_digest(zip, digest);
    CHECK(!gl_install_archive(library, &release, zip, digest, NULL, NULL, error));
    CHECK(library->install_count == 0);
    remove(zip);
    remove(file);
    remove(project);
    char state[GL_PATH + 32];
    snprintf(state, sizeof(state), "%s/library.json", root);
    remove(state);
    snprintf(state, sizeof(state), "%s/engines", root);
#ifdef _WIN32
    _rmdir(state);
    _rmdir(root);
#else
    rmdir(state);
    rmdir(root);
#endif
    free(library);
    gl_shutdown();
    printf("%u checks passed\n", checks);
    return 0;
}
