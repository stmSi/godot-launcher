#include "core.h"
#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <json-c/json.h>
#include <openssl/evp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include "windows_io.h"
#else
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#define mkdir_one(p) mkdir(p, 0700)
#endif

static bool fail(char *error, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(error, 512, format, ap);
    va_end(ap);
    return false;
}
static bool join(char *out, size_t size, const char *a, const char *b) {
    size_t a_size = strlen(a), b_size = strlen(b);
    if (a_size >= size || b_size >= size - a_size || size - a_size - b_size < 2) {
        if (size)
            out[0] = 0;
        return false;
    }
    memcpy(out, a, a_size);
    out[a_size] = '/';
    memcpy(out + a_size + 1, b, b_size + 1);
    return true;
}
static bool exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}
static bool dirs(const char *path) {
    char tmp[GL_PATH];
    if (strlen(path) >= sizeof(tmp))
        return false;
    strcpy(tmp, path);
    for (char *p = tmp + 1; *p; ++p)
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
            if (!exists(tmp) && mkdir_one(tmp) != 0)
                return false;
            *p = c;
        }
    return exists(tmp) || mkdir_one(tmp) == 0;
}
static char *read_file(const char *path, size_t limit) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0 || (uint64_t)n > limit) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *s = malloc((size_t)n + 1);
    if (!s) {
        fclose(f);
        return NULL;
    }
    if (fread(s, 1, (size_t)n, f) != (size_t)n) {
        free(s);
        fclose(f);
        return NULL;
    }
    s[n] = 0;
    fclose(f);
    return s;
}
static bool write_atomic(const char *path, const char *data, char *error) {
    char temp[GL_PATH];
    if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp))
        return fail(error, "Path is too long");
    FILE *f = fopen(temp, "wb");
    if (!f)
        return fail(error, "Cannot write %s", temp);
    bool ok = fwrite(data, 1, strlen(data), f) == strlen(data);
    if (fclose(f) != 0)
        ok = false;
#ifdef _WIN32
    if (ok)
        ok = gl_win_move(temp, path, true);
#else
    if (ok)
        ok = rename(temp, path) == 0;
#endif
    if (!ok) {
        remove(temp);
        return fail(error, "Could not save %s", path);
    }
    return true;
}
static const char *str(json_object *o, const char *key) {
    json_object *v = NULL;
    return json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string)
               ? json_object_get_string(v)
               : "";
}
static void copy(char *out, size_t cap, const char *s) {
    snprintf(out, cap, "%s", s);
}
static bool valid_tag(const char *s) {
    if (!*s || strlen(s) > 90)
        return false;
    for (; *s; ++s)
        if (!isalnum((unsigned char)*s) && *s != '-' && *s != '.')
            return false;
    return true;
}
bool gl_safe_relative(const char *path) {
    if (!path || !*path || *path == '/' || *path == '\\' || strlen(path) > 1000)
        return false;
    const char *start = path;
    unsigned depth = 0;
    for (const char *p = path;; ++p) {
        if (*p == '\\' || *p == ':' || ((unsigned char)*p < 32 && *p))
            return false;
        if (*p == '/' || !*p) {
            size_t n = (size_t)(p - start);
            if (++depth > 48 || (n && (start[n - 1] == '.' || start[n - 1] == ' ')))
                return false;
            if ((n == 2 && start[0] == '.' && start[1] == '.') || (n == 1 && *start == '.'))
                return false;
            char stem[16] = {0};
            size_t k = 0;
            while (k < n && k < sizeof(stem) - 1 && start[k] != '.') {
                stem[k] = (char)toupper((unsigned char)start[k]);
                ++k;
            }
            if (!strcmp(stem, "CON") || !strcmp(stem, "NUL") || !strcmp(stem, "AUX") ||
                !strcmp(stem, "PRN") ||
                (k == 4 && (!strncmp(stem, "COM", 3) || !strncmp(stem, "LPT", 3)) &&
                 stem[3] >= '1' && stem[3] <= '9'))
                return false;
            if (!*p)
                break;
            start = p + 1;
        }
    }
    return true;
}
const char *gl_platform(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}
const char *gl_arch(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "x86_64";
#endif
}
bool gl_init(char *error) {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ||
           fail(error, "Cannot initialize networking");
}
void gl_shutdown(void) {
    curl_global_cleanup();
}
void *gl_library_lock(const char *root, char *error) {
    char path[GL_PATH];
    if (!join(path, sizeof(path), root, ".launcher.lock")) {
        fail(error, "Library path is too long");
        return NULL;
    }
#ifdef _WIN32
    HANDLE handle = gl_win_lock(path);
    if (handle == INVALID_HANDLE_VALUE) {
        fail(error, "This library is open in another launcher, or is not writable");
        return NULL;
    }
    return handle;
#else
    FILE *file = fopen(path, "a+");
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};
    if (!file || fcntl(fileno(file), F_SETLK, &lock) != 0) {
        if (file)
            fclose(file);
        fail(error, "This library is open in another launcher, or is not writable");
        return NULL;
    }
    return file;
#endif
}
void gl_library_unlock(void *lock) {
    if (!lock)
        return;
#ifdef _WIN32
    CloseHandle(lock);
#else
    fclose(lock);
#endif
}
void gl_install_id(const GlRelease *r, char *out, size_t cap) {
    snprintf(out, cap, "%s-%s", r->tag, r->mono ? "mono" : "standard");
}
int gl_find_install(const GlLibrary *l, const char *id) {
    for (size_t i = 0; i < l->install_count; ++i)
        if (!strcmp(l->installs[i].id, id))
            return (int)i;
    return -1;
}
bool gl_version_newer(const char *left, const char *right) {
    unsigned a[3] = {0}, b[3] = {0};
    sscanf(left, "%u.%u.%u", &a[0], &a[1], &a[2]);
    sscanf(right, "%u.%u.%u", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; ++i)
        if (a[i] != b[i])
            return a[i] > b[i];
    return strstr(left, "stable") && !strstr(right, "stable");
}
static bool remove_tree(const char *path) {
#ifdef _WIN32
    char pattern[GL_PATH];
    join(pattern, sizeof(pattern), path, "*");
    WIN32_FIND_DATAW entry;
    HANDLE h = gl_win_find(pattern, &entry);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char name[1024];
            if (!WideCharToMultiByte(CP_UTF8, 0, entry.cFileName, -1, name, sizeof(name), NULL,
                                     NULL)) {
                FindClose(h);
                return false;
            }
            if (!strcmp(name, ".") || !strcmp(name, ".."))
                continue;
            char p[GL_PATH];
            if (!join(p, sizeof(p), path, name)) {
                FindClose(h);
                return false;
            }
            bool directory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool link = (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            if (directory ? (link ? !gl_win_rmdir(p) : !remove_tree(p)) : remove(p) != 0) {
                FindClose(h);
                return false;
            }
        } while (FindNextFileW(h, &entry));
        FindClose(h);
    }
    return gl_win_rmdir(path) || !exists(path);
#else
    DIR *d = opendir(path);
    if (!d)
        return errno == ENOENT;
    struct dirent *e;
    bool ok = true;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char p[GL_PATH];
        struct stat st;
        if (!join(p, sizeof(p), path, e->d_name) || lstat(p, &st) != 0 ||
            (S_ISDIR(st.st_mode) ? !remove_tree(p) : unlink(p) != 0)) {
            ok = false;
            break;
        }
    }
    closedir(d);
    return ok && rmdir(path) == 0;
#endif
}
bool gl_library_save(const GlLibrary *l, char *error) {
    json_object *o = json_object_new_object(), *a = json_object_new_array(),
                *p = json_object_new_array();
    json_object_object_add(o, "preferred", json_object_new_string(l->preferred));
    for (size_t i = 0; i < l->install_count; ++i) {
        const GlInstall *v = &l->installs[i];
        json_object *j = json_object_new_object();
        json_object_object_add(j, "id", json_object_new_string(v->id));
        json_object_object_add(j, "tag", json_object_new_string(v->tag));
        json_object_object_add(j, "executable", json_object_new_string(v->executable));
        json_object_object_add(j, "mono", json_object_new_boolean(v->mono));
        json_object_array_add(a, j);
    }
    for (size_t i = 0; i < l->project_count; ++i) {
        json_object *j = json_object_new_object();
        json_object_object_add(j, "path", json_object_new_string(l->projects[i].path));
        json_object_object_add(j, "name", json_object_new_string(l->projects[i].name));
        json_object_object_add(j, "engine", json_object_new_string(l->projects[i].engine));
        json_object_array_add(p, j);
    }
    json_object_object_add(o, "installs", a);
    json_object_object_add(o, "projects", p);
    char path[GL_PATH];
    join(path, sizeof(path), l->root, "library.json");
    bool ok = write_atomic(path, json_object_to_json_string_ext(o, JSON_C_TO_STRING_PRETTY), error);
    json_object_put(o);
    return ok;
}
bool gl_library_open(GlLibrary *l, const char *root, char *error) {
    memset(l, 0, sizeof(*l));
    if (!root || strlen(root) > GL_PATH - 300)
        return fail(error, "Invalid data directory");
    copy(l->root, sizeof(l->root), root);
    if (!dirs(root))
        return fail(error, "Cannot create data directory: %s", root);
    char path[GL_PATH];
    join(path, sizeof(path), root, "library.json");
    if (!exists(path))
        return true;
    char *s = read_file(path, 4 * 1024 * 1024);
    json_object *o = s ? json_tokener_parse(s) : NULL;
    free(s);
    if (!o || !json_object_is_type(o, json_type_object)) {
        if (o)
            json_object_put(o);
        return fail(error, "library.json is unreadable; preserve it before repairing");
    }
    copy(l->preferred, sizeof(l->preferred), str(o, "preferred"));
    json_object *a = NULL;
    if (json_object_object_get_ex(o, "installs", &a) && json_object_is_type(a, json_type_array))
        for (size_t i = 0; i < json_object_array_length(a) && l->install_count < GL_MAX_INSTALLS;
             ++i) {
            json_object *j = json_object_array_get_idx(a, i);
            GlInstall v = {0};
            copy(v.id, sizeof(v.id), str(j, "id"));
            copy(v.tag, sizeof(v.tag), str(j, "tag"));
            copy(v.executable, sizeof(v.executable), str(j, "executable"));
            json_object *m = NULL;
            json_object_object_get_ex(j, "mono", &m);
            v.mono = json_object_get_boolean(m);
            if (valid_tag(v.tag) && gl_safe_relative(v.id) && !strchr(v.id, '/') &&
                gl_safe_relative(v.executable))
                l->installs[l->install_count++] = v;
        }
    if (json_object_object_get_ex(o, "projects", &a) && json_object_is_type(a, json_type_array))
        for (size_t i = 0; i < json_object_array_length(a) && l->project_count < GL_MAX_PROJECTS;
             ++i) {
            json_object *j = json_object_array_get_idx(a, i);
            GlProject *p = &l->projects[l->project_count++];
            copy(p->path, sizeof(p->path), str(j, "path"));
            copy(p->name, sizeof(p->name), str(j, "name"));
            copy(p->engine, sizeof(p->engine), str(j, "engine"));
        }
    json_object_put(o);
    return true;
}
static bool match_asset(const char *name, const char *tag, bool mono, const char *platform,
                        const char *arch) {
    char expected[256], suffix[80];
    if (!strcmp(platform, "linux"))
        snprintf(suffix, sizeof(suffix), mono ? "mono_linux_%s.zip" : "linux.%s.zip", arch);
    else if (!strcmp(platform, "windows"))
        snprintf(suffix, sizeof(suffix), mono ? "mono_%s.zip" : "%s.exe.zip",
                 !strcmp(arch, "arm64") ? "windows_arm64" : "win64");
    else
        snprintf(suffix, sizeof(suffix), "%smacos.universal.zip", mono ? "mono_" : "");
    snprintf(expected, sizeof(expected), "Godot_v%s_%s", tag, suffix);
    return !strcmp(name, expected);
}
bool gl_parse_catalog(const char *s, const char *platform, const char *arch, GlCatalog *out,
                      char *error) {
    out->count = 0;
    json_object *o = json_tokener_parse(s);
    if (!o || !json_object_is_type(o, json_type_array)) {
        if (o)
            json_object_put(o);
        return fail(error, "Invalid release catalog (GitHub may be rate limiting requests)");
    }
    for (size_t i = 0; i < json_object_array_length(o); ++i) {
        json_object *j = json_object_array_get_idx(o, i), *a = NULL, *v = NULL;
        const char *tag = str(j, "tag_name");
        if (!valid_tag(tag) || tag[0] != '4')
            continue;
        json_object_object_get_ex(j, "draft", &v);
        if (json_object_get_boolean(v))
            continue;
        if (!json_object_object_get_ex(j, "assets", &a) || !json_object_is_type(a, json_type_array))
            continue;
        const char *sums = "";
        for (size_t k = 0; k < json_object_array_length(a); ++k) {
            json_object *asset = json_object_array_get_idx(a, k);
            if (!strcmp(str(asset, "name"), "SHA512-SUMS.txt"))
                sums = str(asset, "browser_download_url");
        }
        for (size_t k = 0; k < json_object_array_length(a) && out->count < GL_MAX_RELEASES; ++k) {
            json_object *asset = json_object_array_get_idx(a, k);
            const char *name = str(asset, "name");
            bool mono = strstr(name, "_mono_") != NULL;
            if (!match_asset(name, tag, mono, platform, arch))
                continue;
            GlRelease *r = &out->items[out->count++];
            memset(r, 0, sizeof(*r));
            copy(r->tag, sizeof(r->tag), tag);
            copy(r->asset, sizeof(r->asset), name);
            copy(r->url, sizeof(r->url), str(asset, "browser_download_url"));
            copy(r->digest, sizeof(r->digest), str(asset, "digest"));
            copy(r->sums_url, sizeof(r->sums_url), sums);
            copy(r->date, sizeof(r->date), str(j, "published_at"));
            copy(r->notes_url, sizeof(r->notes_url), str(j, "html_url"));
            json_object_object_get_ex(j, "prerelease", &v);
            r->preview = json_object_get_boolean(v);
            r->mono = mono;
            json_object_object_get_ex(asset, "size", &v);
            r->size = json_object_get_uint64(v);
        }
    }
    json_object_put(o);
    return out->count > 0 || fail(error, "No compatible Godot 4 builds found");
}
typedef struct {
    FILE *file;
    char *memory;
    size_t size, limit;
    GlProgress progress;
    void *user;
} Transfer;
static size_t receive(void *data, size_t size, size_t count, void *user) {
    Transfer *t = user;
    if (size && count > SIZE_MAX / size)
        return 0;
    size_t n = size * count;
    if (n > t->limit - t->size)
        return 0;
    if (t->file) {
        size_t written = fwrite(data, 1, n, t->file);
        t->size += written;
        return written;
    }
    char *m = realloc(t->memory, t->size + n + 1);
    if (!m)
        return 0;
    t->memory = m;
    memcpy(m + t->size, data, n);
    t->size += n;
    m[t->size] = 0;
    return n;
}
static int transfer_progress(void *u, curl_off_t total, curl_off_t done, curl_off_t up_total,
                             curl_off_t up_done) {
    (void)up_total;
    (void)up_done;
    Transfer *t = u;
    return t->progress && !t->progress(t->user, (uint64_t)done, (uint64_t)total, "Downloading") ? 1
                                                                                                : 0;
}
static bool request(const char *url, Transfer *t, char *error) {
    if (strncmp(url, "https://", 8))
        return fail(error, "Only HTTPS downloads are allowed");
    CURL *c = curl_easy_init();
    if (!c)
        return fail(error, "Cannot initialize download");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(c, CURLOPT_USERAGENT, "Arcflow-Godot-Launcher/0.1");
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 128L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, t);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, transfer_progress);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, t);
    CURLcode result = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(c);
    if (result == CURLE_ABORTED_BY_CALLBACK)
        return fail(error, "Cancelled");
    if (result != CURLE_OK)
        return fail(error, "Download failed (HTTP %ld): %s", status, curl_easy_strerror(result));
    return true;
}
bool gl_fetch_catalog(const char *root, GlCatalog *out, GlProgress progress, void *user,
                      char *error) {
    Transfer t = {.limit = 24 * 1024 * 1024, .progress = progress, .user = user};
    bool ok = request(GL_RELEASE_API, &t, error);
    if (ok)
        ok = t.memory && gl_parse_catalog(t.memory, gl_platform(), gl_arch(), out, error);
    if (ok) {
        char path[GL_PATH];
        join(path, sizeof(path), root, "releases.json");
        ok = write_atomic(path, t.memory, error);
    }
    free(t.memory);
    return ok;
}
bool gl_load_catalog(const char *root, GlCatalog *out, char *error) {
    char path[GL_PATH];
    join(path, sizeof(path), root, "releases.json");
    char *s = read_file(path, 24 * 1024 * 1024);
    if (!s)
        return fail(error, "Connect to load the release catalog");
    bool ok = gl_parse_catalog(s, gl_platform(), gl_arch(), out, error);
    free(s);
    return ok;
}
bool gl_verify(const char *path, const char *digest, char *error) {
    const EVP_MD *algorithm;
    const char *expected;
    if (!strncmp(digest, "sha256:", 7)) {
        algorithm = EVP_sha256();
        expected = digest + 7;
    } else if (!strncmp(digest, "sha512:", 7)) {
        algorithm = EVP_sha512();
        expected = digest + 7;
    } else
        return fail(error, "Release has no supported checksum");
    size_t size = (size_t)EVP_MD_get_size(algorithm);
    if (strlen(expected) != size * 2)
        return fail(error, "Malformed checksum");
    FILE *f = fopen(path, "rb");
    if (!f)
        return fail(error, "Cannot read downloaded archive");
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    bool ok = ctx && EVP_DigestInit_ex(ctx, algorithm, NULL) == 1;
    unsigned char buffer[65536], hash[EVP_MAX_MD_SIZE];
    size_t n;
    while (ok && (n = fread(buffer, 1, sizeof(buffer), f)) > 0)
        ok = EVP_DigestUpdate(ctx, buffer, n) == 1;
    if (ferror(f))
        ok = false;
    unsigned int written = 0;
    if (ok)
        ok = EVP_DigestFinal_ex(ctx, hash, &written) == 1;
    fclose(f);
    EVP_MD_CTX_free(ctx);
    if (!ok)
        return fail(error, "Checksum calculation failed");
    char hex[EVP_MAX_MD_SIZE * 2 + 1];
    for (unsigned int i = 0; i < written; ++i)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    return !strcmp(hex, expected) || fail(error, "Checksum mismatch; installation was rejected");
}
static bool editor_file(const char *path, const GlRelease *r) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
#ifdef __APPLE__
    (void)r;
    return !strcmp(base, "Godot") && strstr(path, ".app/Contents/MacOS/");
#else
    char expected[256];
#ifdef _WIN32
    snprintf(expected, sizeof(expected), "Godot_v%s_%s%s.exe", r->tag, r->mono ? "mono_" : "",
             !strcmp(gl_arch(), "arm64") ? "windows_arm64" : "win64");
#else
    snprintf(expected, sizeof(expected), "Godot_v%s_%slinux.%s", r->tag, r->mono ? "mono_" : "",
             gl_arch());
#endif
    return !strcmp(base, expected);
#endif
}
bool gl_install_archive(GlLibrary *l, const GlRelease *r, const char *archive_path,
                        const char *digest, GlProgress progress, void *user, char *error) {
    if (!valid_tag(r->tag) || l->install_count >= GL_MAX_INSTALLS)
        return fail(error, "Invalid release or library is full");
    char id[128], engines[GL_PATH], stage[GL_PATH], final[GL_PATH];
    gl_install_id(r, id, sizeof(id));
    if (gl_find_install(l, id) >= 0)
        return fail(error, "This engine is already installed");
    join(engines, sizeof(engines), l->root, "engines");
    join(final, sizeof(final), engines, id);
    char stage_id[140];
    snprintf(stage_id, sizeof(stage_id), ".%s.staging", id);
    join(stage, sizeof(stage), engines, stage_id);
    if (exists(final))
        return fail(error, "An engine directory already exists; preserve it before retrying");
    if (progress && !progress(user, 0, 0, "Verifying checksum"))
        return fail(error, "Cancelled");
    if (!gl_verify(archive_path, digest, error))
        return false;
    if (!dirs(engines) || !remove_tree(stage) || !dirs(stage))
        return fail(error, "Cannot create installation staging directory");
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);
#ifdef _WIN32
    bool ok = gl_win_archive_open(a, archive_path) == ARCHIVE_OK;
#else
    bool ok = archive_read_open_filename(a, archive_path, 65536) == ARCHIVE_OK;
#endif
    char executable[GL_PATH] = {0};
    uint64_t total = 0;
    struct archive_entry *entry;
    int status = ARCHIVE_OK;
    unsigned entries = 0;
    while (ok && (status = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        mode_t type = archive_entry_filetype(entry);
        if (!gl_safe_relative(name) || archive_entry_symlink(entry) ||
            archive_entry_hardlink(entry) || (type != AE_IFREG && type != AE_IFDIR) ||
            ++entries > 50000) {
            ok = fail(error, "Unsafe archive entry rejected");
            break;
        }
        char target[GL_PATH];
        if (!join(target, sizeof(target), stage, name)) {
            ok = fail(error, "Archive path is too long");
            break;
        }
        if (type == AE_IFDIR) {
            if (!dirs(target))
                ok = fail(error, "Cannot create archive directory");
            continue;
        }
        char parent[GL_PATH];
        copy(parent, sizeof(parent), target);
        char *slash = strrchr(parent, '/');
        if (slash)
            *slash = 0;
        if (!dirs(parent)) {
            ok = fail(error, "Cannot create engine directory");
            break;
        }
        FILE *f = fopen(target, "wb");
        if (!f) {
            ok = fail(error, "Cannot write engine file");
            break;
        }
        char buffer[65536];
        la_ssize_t n;
        while ((n = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
            total += (uint64_t)n;
            if (total > UINT64_C(6) * 1024 * 1024 * 1024 ||
                fwrite(buffer, 1, (size_t)n, f) != (size_t)n) {
                ok = fail(error, "Archive is too large or storage is full");
                break;
            }
            if (progress && !progress(user, total, 0, "Extracting engine")) {
                ok = fail(error, "Cancelled");
                break;
            }
        }
        if (n < 0)
            ok = fail(error, "Damaged archive");
        if (fclose(f) != 0)
            ok = fail(error, "Cannot finish writing engine file");
#ifndef _WIN32
        if (chmod(target, (archive_entry_perm(entry) & 0111) ? 0700 : 0600) != 0)
            ok = fail(error, "Cannot set engine permissions");
#endif
        if (editor_file(name, r)) {
            copy(executable, sizeof(executable), name);
#ifndef _WIN32
            if (chmod(target, 0700) != 0)
                ok = fail(error, "Cannot make engine executable");
#endif
        }
    }
    if (status != ARCHIVE_EOF && ok)
        ok = fail(error, "Cannot extract archive: %s",
                  archive_error_string(a) ? archive_error_string(a) : "invalid ZIP");
    archive_read_free(a);
    if (ok && !*executable)
        ok = fail(error, "Editor executable was not found in this archive");
    if (ok && progress && !progress(user, 0, 0, "Finishing installation"))
        ok = fail(error, "Cancelled");
    if (ok && rename(stage, final) != 0)
        ok = fail(error, "Cannot commit installation");
    if (!ok) {
        remove_tree(stage);
        return false;
    }
    GlInstall *v = &l->installs[l->install_count++];
    memset(v, 0, sizeof(*v));
    copy(v->id, sizeof(v->id), id);
    copy(v->tag, sizeof(v->tag), r->tag);
    copy(v->executable, sizeof(v->executable), executable);
    v->mono = r->mono;
    char old_preferred[128];
    copy(old_preferred, sizeof(old_preferred), l->preferred);
    if (!*l->preferred)
        copy(l->preferred, sizeof(l->preferred), id);
    if (!gl_library_save(l, error)) {
        --l->install_count;
        copy(l->preferred, sizeof(l->preferred), old_preferred);
        remove_tree(final);
        return false;
    }
    return true;
}
bool gl_install(GlLibrary *l, const GlRelease *r, GlProgress progress, void *user, char *error) {
    char digest[160];
    copy(digest, sizeof(digest), r->digest);
    if (!*digest) {
        Transfer sums = {.limit = 1024 * 1024, .progress = progress, .user = user};
        if (!request(r->sums_url, &sums, error)) {
            free(sums.memory);
            return false;
        }
        if (sums.memory)
            for (char *line = strtok(sums.memory, "\n"); line; line = strtok(NULL, "\n")) {
                char hash[129], name[256];
                if (sscanf(line, "%128s %255s", hash, name) == 2 && strlen(hash) == 128 &&
                    !strcmp(name[0] == '*' ? name + 1 : name, r->asset))
                    snprintf(digest, sizeof(digest), "sha512:%s", hash);
            }
        free(sums.memory);
    }
    if (!*digest)
        return fail(error, "No checksum published for this release");
    char cache[GL_PATH], zip[GL_PATH];
    join(cache, sizeof(cache), l->root, "downloads");
    if (!dirs(cache))
        return fail(error, "Cannot create download directory");
    join(zip, sizeof(zip), cache, "engine.zip.part");
    FILE *f = fopen(zip, "wb");
    if (!f)
        return fail(error, "Cannot write download");
    Transfer t = {
        .file = f, .limit = (size_t)2 * 1024 * 1024 * 1024, .progress = progress, .user = user};
    bool ok = request(r->url, &t, error);
    if (fclose(f) != 0)
        ok = fail(error, "Cannot finish download");
    if (ok)
        ok = gl_install_archive(l, r, zip, digest, progress, user, error);
    remove(zip);
    return ok;
}
bool gl_remove(GlLibrary *l, size_t index, char *error) {
    if (index >= l->install_count)
        return fail(error, "Invalid engine");
    const char *id = l->installs[index].id;
    if (!gl_safe_relative(id) || strchr(id, '/'))
        return fail(error, "Invalid engine path");
    char base[GL_PATH], path[GL_PATH], trash[GL_PATH];
    join(base, sizeof(base), l->root, "engines");
    join(path, sizeof(path), base, id);
    char trash_id[160];
    snprintf(trash_id, sizeof(trash_id), ".%.127s.removing", id);
    join(trash, sizeof(trash), base, trash_id);
    if (!remove_tree(trash) || rename(path, trash) != 0)
        return fail(error, "Cannot remove engine; it may still be running");
    GlInstall old = l->installs[index];
    char preferred[128];
    copy(preferred, sizeof(preferred), l->preferred);
    memmove(&l->installs[index], &l->installs[index + 1],
            (l->install_count - index - 1) * sizeof(GlInstall));
    --l->install_count;
    if (!strcmp(l->preferred, old.id))
        copy(l->preferred, sizeof(l->preferred), l->install_count ? l->installs[0].id : "");
    if (!gl_library_save(l, error)) {
        memmove(&l->installs[index + 1], &l->installs[index],
                (l->install_count - index) * sizeof(GlInstall));
        l->installs[index] = old;
        ++l->install_count;
        copy(l->preferred, sizeof(l->preferred), preferred);
        rename(trash, path);
        return false;
    }
    return remove_tree(trash) ||
           fail(error, "Engine unregistered; some files are still in %s", trash);
}
bool gl_add_project(GlLibrary *l, const char *path, char *error) {
    size_t n = strlen(path);
    if (n < 13 || n >= GL_PATH || strcmp(path + n - 13, "project.godot"))
        return fail(error, "Choose a project.godot file");
    for (size_t i = 0; i < l->project_count; ++i)
        if (!strcmp(path, l->projects[i].path))
            return true;
    if (l->project_count >= GL_MAX_PROJECTS)
        return fail(error, "Project library is full");
    char *content = read_file(path, 4 * 1024 * 1024);
    if (!content)
        return fail(error, "Cannot read project file");
    GlProject *p = &l->projects[l->project_count++];
    memset(p, 0, sizeof(*p));
    copy(p->path, sizeof(p->path), path);
    copy(p->engine, sizeof(p->engine), l->preferred);
    copy(p->name, sizeof(p->name), "Godot project");
    const char *name = strstr(content, "config/name=\"");
    if (name) {
        name += 13;
        const char *end = strchr(name, '"');
        if (end) {
            size_t len = (size_t)(end - name);
            if (len >= sizeof(p->name))
                len = sizeof(p->name) - 1;
            memcpy(p->name, name, len);
            p->name[len] = 0;
        }
    }
    free(content);
    if (!gl_library_save(l, error)) {
        --l->project_count;
        return false;
    }
    return true;
}
