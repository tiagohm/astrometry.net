/*
 * Codex!
 */

#ifndef ASTROMETRY_PROCESS_COMPAT_H
#define ASTROMETRY_PROCESS_COMPAT_H

#include <signal.h>

#ifndef SIGTERM
#define SIGTERM 15
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifndef WIFEXITED
#define WIFEXITED(status) (1)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(status) (status)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (0)
#endif
#ifndef WTERMSIG
#define WTERMSIG(status) (0)
#endif

#else

#include <sys/wait.h>

#endif

#endif
