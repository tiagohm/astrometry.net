/*
 * This file is part of the Astrometry.net suite.
 * Licensed under a 3-clause BSD style license - see LICENSE
 */

#ifndef ASTROMETRY_TIME_COMPAT_H
#define ASTROMETRY_TIME_COMPAT_H

#include <time.h>

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <errno.h>
#include <stdint.h>
#include <windows.h>

#if !defined(_MSC_VER)
#include <sys/time.h>
#endif

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

#ifndef RUSAGE_SELF
#define RUSAGE_SELF 0
#endif

#ifndef ASTROMETRY_HAVE_RUSAGE
#define ASTROMETRY_HAVE_RUSAGE
struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
};
#endif

/* GetProcessTimes durations are 100 ns ticks, not FILETIME wall-clock
 * values. Subtracting the Unix epoch here yields zero CPU time. */
static inline void astrometry_filetime_duration_to_timeval(FILETIME ft,
                                                           struct timeval* tv) {
    ULARGE_INTEGER uli;
    uint64_t ticks;

    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    ticks = uli.QuadPart;
    tv->tv_sec = (long)(ticks / 10000000ULL);
    tv->tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
}

#if defined(_MSC_VER)
static inline int astrometry_gettimeofday(struct timeval* tv, void* tz) {
    FILETIME ft;
    ULARGE_INTEGER uli;
    uint64_t ticks;

    (void)tz;
    if (!tv) {
        errno = EINVAL;
        return -1;
    }

    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    ticks = uli.QuadPart;
    if (ticks >= 116444736000000000ULL)
        ticks -= 116444736000000000ULL;
    else
        ticks = 0;
    tv->tv_sec = (long)(ticks / 10000000ULL);
    tv->tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
    return 0;
}
#ifndef gettimeofday
#define gettimeofday astrometry_gettimeofday
#endif
#endif

#ifndef getrusage
static inline int astrometry_getrusage(int who, struct rusage* usage) {
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;

    (void)who;
    if (!usage) {
        errno = EINVAL;
        return -1;
    }
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit_time,
                         &kernel, &user)) {
        errno = EINVAL;
        return -1;
    }
    astrometry_filetime_duration_to_timeval(user, &usage->ru_utime);
    astrometry_filetime_duration_to_timeval(kernel, &usage->ru_stime);
    usage->ru_maxrss = 0;
    return 0;
}
#define getrusage astrometry_getrusage
#endif

/* MinGW already defines these as forceinline when _POSIX_THREAD_SAFE_FUNCTIONS
 * is set, which -D_GNU_SOURCE does. A second definition will not compile. */
#if !defined(localtime_r) && !defined(_POSIX_THREAD_SAFE_FUNCTIONS)
static inline struct tm* astrometry_localtime_r(const time_t* timer,
                                                struct tm* result) {
    if (!timer || !result)
        return NULL;
    return localtime_s(result, timer) ? NULL : result;
}
#define localtime_r astrometry_localtime_r
#endif

#if !defined(gmtime_r) && !defined(_POSIX_THREAD_SAFE_FUNCTIONS)
static inline struct tm* astrometry_gmtime_r(const time_t* timer,
                                             struct tm* result) {
    if (!timer || !result)
        return NULL;
    return gmtime_s(result, timer) ? NULL : result;
}
#define gmtime_r astrometry_gmtime_r
#endif

/* windows.h defines ERROR as the GDI constant 0. Put the logger back if
 * errors.h was included first. */
#ifdef AN_ERRORS_H
#ifdef ERROR
#undef ERROR
#endif
#define ERROR(fmt, ...) report_error(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define SYSERROR(fmt, ...) do { report_errno(); report_error(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); } while(0)
#endif

#else

#include <sys/time.h>
#include <sys/resource.h>

#endif

#endif
