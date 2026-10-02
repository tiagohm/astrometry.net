/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */
#include <string.h>
#if !defined(_WIN32) || defined(__CYGWIN__)
#include <libgen.h>
#endif

#include "fileutils.h"
#include "ioutils.h"
#include "os-features.h"

#if defined(_WIN32) && !defined(__CYGWIN__)

static int an_win_is_absolute(const char* path) {
    if (!path || !path[0])
        return 0;
    if ((path[0] == '/') || (path[0] == '\\'))
        return 1;
    if ((((path[0] >= 'A') && (path[0] <= 'Z')) ||
         ((path[0] >= 'a') && (path[0] <= 'z'))) &&
        (path[1] == ':') &&
        ((path[2] == '/') || (path[2] == '\\')))
        return 1;
    return 0;
}

static int an_ends_icase(const char* str, const char* suffix) {
    size_t n = strlen(str);
    size_t m = strlen(suffix);
    size_t i;

    if (m > n)
        return 0;
    for (i = 0; i < m; i++) {
        unsigned char a = (unsigned char)str[n - m + i];
        unsigned char b = (unsigned char)suffix[i];
        if ((a >= 'A') && (a <= 'Z'))
            a = (unsigned char)(a - 'A' + 'a');
        if ((b >= 'A') && (b <= 'Z'))
            b = (unsigned char)(b - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}

/* Try the path as given, then PATHEXT. An extension already present is not
 * appended a second time. */
static char* an_find_with_pathext(const char* candidate) {
    const char* pathext;
    char* copy;
    char* ext;

    if (file_executable(candidate))
        return strdup(candidate);

    pathext = getenv("PATHEXT");
    if (!pathext || !pathext[0])
        pathext = ".COM;.EXE;.BAT;.CMD";
    copy = strdup(pathext);
    if (!copy)
        return NULL;

    ext = copy;
    while (*ext) {
        char* semi = strchr(ext, ';');
        char* path = NULL;
        if (semi)
            *semi = '\0';
        if (*ext && !an_ends_icase(candidate, ext)) {
            asprintf_safe(&path, "%s%s", candidate, ext);
            if (file_executable(path)) {
                free(copy);
                return path;
            }
            free(path);
        }
        if (!semi)
            break;
        ext = semi + 1;
    }
    free(copy);
    return NULL;
}

char* resolve_path(const char* filename, const char* basedir) {
    char* path;
    char* rtn;

    if (an_win_is_absolute(filename))
        return an_canonicalize_file_name(filename);
    asprintf_safe(&path, "%s/%s", basedir, filename);
    rtn = an_canonicalize_file_name(path);
    free(path);
    return rtn;
}

char* find_executable(const char* progname, const char* sibling) {
    char* sibdir;
    char* path;
    char* found;
    char* pathenv;

    if (an_win_is_absolute(progname)) {
        found = an_find_with_pathext(progname);
        return found ? found : strdup(progname);
    }

    if (strchr(progname, '/') || strchr(progname, '\\')) {
        path = an_canonicalize_file_name(progname);
        if (path) {
            found = an_find_with_pathext(path);
            free(path);
            if (found)
                return found;
        }
    }

    if (sibling && (strchr(sibling, '/') || strchr(sibling, '\\'))) {
        sibdir = dirname_safe(sibling);
        if (sibdir) {
            asprintf_safe(&path, "%s/%s", sibdir, progname);
            free(sibdir);
            found = an_find_with_pathext(path);
            free(path);
            if (found)
                return found;
        }
    }

    pathenv = getenv("PATH");
    if (!pathenv)
        return NULL;
    while (*pathenv) {
        char* semi = strchr(pathenv, ';');
        int len = semi ? (int)(semi - pathenv) : (int)strlen(pathenv);
        int dirlen = len;

        if ((dirlen > 0) &&
            ((pathenv[dirlen - 1] == '/') || (pathenv[dirlen - 1] == '\\')))
            dirlen--;
        if (dirlen <= 0)
            asprintf_safe(&path, "%s", progname);
        else
            asprintf_safe(&path, "%.*s/%s", dirlen, pathenv, progname);
        found = an_find_with_pathext(path);
        free(path);
        if (found)
            return found;
        if (!semi)
            break;
        pathenv = semi + 1;
    }
    return NULL;
}

#else

char* resolve_path(const char* filename, const char* basedir) {
    // we don't use canonicalize_file_name() because it requires the paths
    // to actually exist, while this function should work for output files
    // that don't already exist.
    char* path;
    char* rtn;
    // absolute path?
    if (filename[0] == '/')
        //return strdup(filename);
        return an_canonicalize_file_name(filename);
    asprintf_safe(&path, "%s/%s", basedir, filename);
    //return path;
    rtn = an_canonicalize_file_name(path);
    free(path);
    return rtn;
}

char* find_executable(const char* progname, const char* sibling) {
    char* sib;
    char* sibdir;
    char* path;
    char* pathenv;

    // If it's an absolute path, just return it.
    if (progname[0] == '/')
        return strdup(progname);

    // If it's a relative path, resolve it.
    if (strchr(progname, '/')) {
        path = an_canonicalize_file_name(progname);
        if (path && file_executable(path))
            return path;
        free(path);
    }

    // If "sibling" contains a "/", then check relative to it.
    if (sibling && strchr(sibling, '/')) {
        // dirname() overwrites its arguments, so make a copy...
        sib = strdup(sibling);
        sibdir = strdup(dirname(sib));
        free(sib);

        asprintf_safe(&path, "%s/%s", sibdir, progname);
        free(sibdir);

        if (file_executable(path))
            return path;

        free(path);
    }

    // Search PATH.
    pathenv = getenv("PATH");
    while (1) {
        char* colon;
        int len;
        if (!strlen(pathenv))
            break;
        colon = strchr(pathenv, ':');
        if (colon)
            len = colon - pathenv;
        else
            len = strlen(pathenv);
        if (pathenv[len - 1] == '/')
            len--;
        asprintf_safe(&path, "%.*s/%s", len, pathenv, progname);
        if (file_executable(path))
            return path;
        free(path);
        if (colon)
            pathenv = colon + 1;
        else
            break;
    }

    // Not found.
    return NULL;
}

#endif

char* an_canonicalize_file_name(const char* fn) {
    sl* dirs;
    int i;
    char* result;
    const char* path = fn;
#if defined(_WIN32) && !defined(__CYGWIN__)
    char* owned = strdup(fn);
    size_t k;
    size_t n;

    if (!owned)
        return NULL;
    n = strlen(owned);
    for (k = 0; k < n; k++) {
        if (owned[k] == '\\')
            owned[k] = '/';
    }
    path = owned;
#endif
    // Ugh, special cases.
    if (streq(path, ".") || streq(path, "/")) {
#if defined(_WIN32) && !defined(__CYGWIN__)
        return owned;
#else
        return strdup(fn);
#endif
    }

    dirs = sl_split(NULL, path, "/");
#if defined(_WIN32) && !defined(__CYGWIN__)
    free(owned);
#endif
    for (i=0; i<sl_size(dirs); i++) {
        if (streq(sl_get(dirs, i), "")) {
            // don't remove '/' from beginning of path!
            if (i) {
#if defined(_WIN32) && !defined(__CYGWIN__)
                /* Keep the second slash of a UNC path: "//server/share". */
                if ((i == 1) && streq(sl_get(dirs, 0), ""))
                    continue;
#endif
                sl_remove(dirs, i);
                i--;
            }
        } else if (streq(sl_get(dirs, i), ".")) {
            sl_remove(dirs, i);
            i--;
        } else if (streq(sl_get(dirs, i), "..")) {
            // don't remove ".." at start of path.
            if (!i)
                continue;
            // don't remove chains of '../../../' at the start.
            if (streq(sl_get(dirs, i-1), ".."))
                continue;
#if defined(_WIN32) && !defined(__CYGWIN__)
            /* "C:/../foo" is "C:/foo". Do not drop the drive prefix. */
            {
                const char* prev = sl_get(dirs, i-1);
                if (prev &&
                    (((prev[0] >= 'A') && (prev[0] <= 'Z')) ||
                     ((prev[0] >= 'a') && (prev[0] <= 'z'))) &&
                    (prev[1] == ':') && (prev[2] == '\0')) {
                    char* dotdot = sl_get(dirs, i);
                    sl_remove(dirs, i);
                    free(dotdot);
                    i--;
                    continue;
                }
            }
#endif
            // but do collapse '/../' to '/' at the start.
            if (streq(sl_get(dirs, i-1), "")) {
                sl_remove(dirs, i);
                i--;
            } else {
                sl_remove(dirs, i-1);
                sl_remove(dirs, i-1);
                i -= 2;
            }
        }
    }
    result = sl_join(dirs, "/");
    sl_free2(dirs);
    return result;
}
