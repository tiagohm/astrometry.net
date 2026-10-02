/*
 * This file is part of the Astrometry.net suite.
 * Licensed under a 3-clause BSD style license - see LICENSE
 */

#ifndef ASTROMETRY_PROCESS_COMPAT_H
#define ASTROMETRY_PROCESS_COMPAT_H

#include <signal.h>

#ifndef SIGTERM
#define SIGTERM 15
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)

/* shell_system() returns the process exit code, or an NTSTATUS when the
 * console handler kills the child. STATUS_CONTROL_C_EXIT is 0xC000013A and
 * STATUS_CONTROL_BREAK_EXIT is 0xC000013B. */
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (((unsigned long)(status) - 0xC000013Aul) <= 1ul)
#endif
#ifndef WTERMSIG
#define WTERMSIG(status) (SIGTERM)
#endif
#ifndef WIFEXITED
#define WIFEXITED(status) (!WIFSIGNALED(status))
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (status)
#endif

#else

#include <sys/wait.h>

#endif

#endif
