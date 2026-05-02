/*
 * Codex!
 */

#ifndef ASTROMETRY_TIME_COMPAT_H
#define ASTROMETRY_TIME_COMPAT_H

#include <time.h>

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdint.h>
#include <windows.h>

#ifndef RUSAGE_SELF
#define RUSAGE_SELF 0
#endif

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
};

static inline void astrometry_filetime_to_timeval(FILETIME ft,
                                                  struct timeval* tv) {
    ULARGE_INTEGER uli;
    uint64_t ticks;

    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    ticks = uli.QuadPart;

    if (ticks >= 116444736000000000ULL)
        ticks -= 116444736000000000ULL;
    else
        ticks = 0;

    tv->tv_sec = (long)(ticks / 10000000ULL);
    tv->tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
}

static inline int gettimeofday(struct timeval* tv, void* tz) {
    FILETIME ft;

    (void)tz;
    if (!tv)
        return -1;

    GetSystemTimeAsFileTime(&ft);
    astrometry_filetime_to_timeval(ft, tv);
    return 0;
}

static inline int getrusage(int who, struct rusage* usage) {
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;

    (void)who;
    if (!usage)
        return -1;

    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit_time,
                         &kernel, &user))
        return -1;

    astrometry_filetime_to_timeval(user, &usage->ru_utime);
    astrometry_filetime_to_timeval(kernel, &usage->ru_stime);
    usage->ru_maxrss = 0;
    return 0;
}

static inline struct tm* localtime_r(const time_t* timer, struct tm* result) {
    struct tm* tmp;

    if (!timer || !result)
        return NULL;
#if defined(_MSC_VER)
    return localtime_s(result, timer) ? NULL : result;
#else
    tmp = localtime(timer);
    if (!tmp)
        return NULL;
    *result = *tmp;
    return result;
#endif
}

static inline struct tm* gmtime_r(const time_t* timer, struct tm* result) {
    struct tm* tmp;

    if (!timer || !result)
        return NULL;
#if defined(_MSC_VER)
    return gmtime_s(result, timer) ? NULL : result;
#else
    tmp = gmtime(timer);
    if (!tmp)
        return NULL;
    *result = *tmp;
    return result;
#endif
}

#else

#include <sys/time.h>
#include <sys/resource.h>

#endif

#endif
