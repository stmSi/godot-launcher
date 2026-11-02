#ifndef GL_WINDOWS_IO_H
#define GL_WINDOWS_IO_H
/* SDL and the library use UTF-8 paths. Keep Windows filesystem calls in UTF-16,
 * including user profiles and projects containing non-ASCII characters. */
#include <direct.h>
#include <windows.h>
static bool gl_wide(const char *path, wchar_t *out) {
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out, GL_PATH) > 0;
}
static FILE *gl_win_fopen(const char *path, const char *mode) {
    wchar_t wide[GL_PATH], wide_mode[16];
    if (!gl_wide(path, wide) || !MultiByteToWideChar(CP_UTF8, 0, mode, -1, wide_mode, 16))
        return NULL;
    return _wfopen(wide, wide_mode);
}
static int gl_win_mkdir(const char *path) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) ? _wmkdir(wide) : -1;
}
static int gl_win_stat(const char *path, struct _stat64 *out) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) ? _wstat64(wide, out) : -1;
}
static int gl_win_remove(const char *path) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) ? _wremove(wide) : -1;
}
static bool gl_win_move(const char *source, const char *target, bool replace) {
    wchar_t a[GL_PATH], b[GL_PATH];
    return gl_wide(source, a) && gl_wide(target, b) &&
           MoveFileExW(a, b, MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0));
}
static int gl_win_rename(const char *source, const char *target) {
    return gl_win_move(source, target, false) ? 0 : -1;
}
static bool gl_win_rmdir(const char *path) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) && RemoveDirectoryW(wide);
}
static HANDLE gl_win_find(const char *path, WIN32_FIND_DATAW *out) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) ? FindFirstFileW(wide, out) : INVALID_HANDLE_VALUE;
}
static HANDLE gl_win_lock(const char *path) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) ? CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
                               : INVALID_HANDLE_VALUE;
}
static int gl_win_archive_open(struct archive *a, const char *path) {
    wchar_t wide[GL_PATH];
    return gl_wide(path, wide) ? archive_read_open_filename_w(a, wide, 65536) : ARCHIVE_FATAL;
}
#define fopen gl_win_fopen
#define mkdir_one gl_win_mkdir
#define remove gl_win_remove
#define rename gl_win_rename
#define stat _stat64
#define _stat64(path, out) gl_win_stat(path, out)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#endif
