/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */

#ifndef ASTROMETRY_MMAP_COMPAT_H
#define ASTROMETRY_MMAP_COMPAT_H

#if defined(_WIN32) && !defined(__CYGWIN__)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <windows.h>
#include <io.h>

#ifndef FILE_MAP_EXECUTE
#define FILE_MAP_EXECUTE 0x0020
#endif

#ifndef PAGE_EXECUTE_WRITECOPY
#define PAGE_EXECUTE_WRITECOPY 0x80
#endif

#ifndef ERROR_MAPPED_ALIGNMENT
#define ERROR_MAPPED_ALIGNMENT 1132L
#endif

#ifndef PROT_NONE
#define PROT_NONE 0x0
#endif
#ifndef PROT_READ
#define PROT_READ 0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif
#ifndef PROT_EXEC
#define PROT_EXEC 0x4
#endif

#ifndef MAP_SHARED
#define MAP_SHARED 0x01
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif
#ifndef MAP_FIXED
#define MAP_FIXED 0x10
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#if defined(_MSC_VER)
#define ASTROMETRY_MMAP_INLINE static __inline
#else
#define ASTROMETRY_MMAP_INLINE static inline
#endif

typedef struct astrometry_mmap_record {
    void* returned;
    void* base;
    int virtual_alloc;
    int flush;
    struct astrometry_mmap_record* next;
} astrometry_mmap_record_t;

static astrometry_mmap_record_t* astrometry_mmap_records = NULL;

ASTROMETRY_MMAP_INLINE void astrometry_mmap_set_errno(DWORD error) {
    switch (error) {
    case ERROR_SUCCESS:
        errno = 0;
        break;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        errno = EACCES;
        break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        errno = ENOENT;
        break;
    case ERROR_INVALID_HANDLE:
        errno = EBADF;
        break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        errno = ENOMEM;
        break;
    case ERROR_INVALID_PARAMETER:
    case ERROR_MAPPED_ALIGNMENT:
    default:
        errno = EINVAL;
        break;
    }
}

ASTROMETRY_MMAP_INLINE DWORD astrometry_mmap_granularity(void) {
    static DWORD granularity = 0;

    if (granularity == 0) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        granularity = info.dwAllocationGranularity;
    }
    return granularity;
}

ASTROMETRY_MMAP_INLINE int astrometry_getpagesize(void) {
    return (int)astrometry_mmap_granularity();
}

#define getpagesize astrometry_getpagesize

ASTROMETRY_MMAP_INLINE DWORD astrometry_mmap_file_protect(int prot, int flags) {
    if (prot & PROT_EXEC) {
        if (prot & PROT_WRITE)
            return (flags & MAP_PRIVATE) ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READWRITE;
        return PAGE_EXECUTE_READ;
    }

    if (prot & PROT_WRITE)
        return (flags & MAP_PRIVATE) ? PAGE_WRITECOPY : PAGE_READWRITE;

    return PAGE_READONLY;
}

ASTROMETRY_MMAP_INLINE DWORD astrometry_mmap_view_access(int prot, int flags) {
    DWORD access = 0;

    if ((flags & MAP_PRIVATE) && (prot & PROT_WRITE))
        access |= FILE_MAP_COPY;
    else if (prot & PROT_WRITE)
        access |= FILE_MAP_WRITE;
    else
        access |= FILE_MAP_READ;

    if (prot & PROT_EXEC)
        access |= FILE_MAP_EXECUTE;

    return access;
}

ASTROMETRY_MMAP_INLINE DWORD astrometry_mmap_virtual_protect(int prot) {
    if (prot & PROT_EXEC) {
        if (prot & PROT_WRITE)
            return PAGE_EXECUTE_READWRITE;
        return PAGE_EXECUTE_READ;
    }

    if (prot & PROT_WRITE)
        return PAGE_READWRITE;

    if (prot & PROT_READ)
        return PAGE_READONLY;

    return PAGE_NOACCESS;
}

ASTROMETRY_MMAP_INLINE int astrometry_mmap_add_record(void* returned, void* base,
                                                      int virtual_alloc,
                                                      int flush) {
    astrometry_mmap_record_t* record;

    record = (astrometry_mmap_record_t*)HeapAlloc(GetProcessHeap(), 0,
                                                  sizeof(astrometry_mmap_record_t));
    if (!record) {
        errno = ENOMEM;
        return -1;
    }

    record->returned = returned;
    record->base = base;
    record->virtual_alloc = virtual_alloc;
    record->flush = flush;
    record->next = astrometry_mmap_records;
    astrometry_mmap_records = record;
    return 0;
}

ASTROMETRY_MMAP_INLINE void* astrometry_mmap(void* addr, size_t len, int prot,
                                             int flags, int fd, off_t offset) {
    uint64_t raw_offset;
    uint64_t aligned_offset;
    size_t delta;
    size_t maplen;
    DWORD granularity;
    DWORD protect;
    DWORD access;
    DWORD offset_high;
    DWORD offset_low;
    DWORD map_error;
    HANDLE file;
    HANDLE mapping;
    void* base;
    void* returned;

    if (len == 0 || offset < 0 || (flags & MAP_FIXED)) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    if (flags & MAP_ANONYMOUS) {
        base = VirtualAlloc(addr, len, MEM_RESERVE | MEM_COMMIT,
                            astrometry_mmap_virtual_protect(prot));
        if (!base) {
            astrometry_mmap_set_errno(GetLastError());
            return MAP_FAILED;
        }
        if (astrometry_mmap_add_record(base, base, 1, 0)) {
            VirtualFree(base, 0, MEM_RELEASE);
            return MAP_FAILED;
        }
        return base;
    }

    file = (HANDLE)_get_osfhandle(fd);
    if (file == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return MAP_FAILED;
    }

    raw_offset = (uint64_t)offset;
    granularity = astrometry_mmap_granularity();
    aligned_offset = raw_offset - (raw_offset % granularity);
    delta = (size_t)(raw_offset - aligned_offset);
    if (delta > ((size_t)-1) - len) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    maplen = len + delta;

    protect = astrometry_mmap_file_protect(prot, flags);
    mapping = CreateFileMapping(file, NULL, protect, 0, 0, NULL);
    if (!mapping) {
        astrometry_mmap_set_errno(GetLastError());
        return MAP_FAILED;
    }

    access = astrometry_mmap_view_access(prot, flags);
    offset_high = (DWORD)(aligned_offset >> 32);
    offset_low = (DWORD)(aligned_offset & 0xffffffffu);

    if (addr && delta == 0)
        base = MapViewOfFileEx(mapping, access, offset_high, offset_low, maplen, addr);
    else
        base = MapViewOfFile(mapping, access, offset_high, offset_low, maplen);

    if (!base && addr && delta == 0)
        base = MapViewOfFile(mapping, access, offset_high, offset_low, maplen);

    map_error = base ? ERROR_SUCCESS : GetLastError();
    CloseHandle(mapping);

    if (!base) {
        astrometry_mmap_set_errno(map_error);
        return MAP_FAILED;
    }

    returned = ((char*)base) + delta;
    if (astrometry_mmap_add_record(returned, base, 0,
                                   (flags & MAP_SHARED) && (prot & PROT_WRITE))) {
        UnmapViewOfFile(base);
        return MAP_FAILED;
    }

    return returned;
}

ASTROMETRY_MMAP_INLINE int astrometry_munmap(void* addr, size_t len) {
    astrometry_mmap_record_t* record;
    astrometry_mmap_record_t* prev;
    DWORD flush_error;
    DWORD unmap_error;
    int flush_ok;
    int ok;

    (void)len;

    prev = NULL;
    record = astrometry_mmap_records;
    while (record) {
        if (record->returned == addr) {
            if (prev)
                prev->next = record->next;
            else
                astrometry_mmap_records = record->next;

            flush_ok = 1;
            flush_error = ERROR_SUCCESS;
            if (!record->virtual_alloc && record->flush)
                flush_ok = FlushViewOfFile(record->base, 0);
            if (!flush_ok)
                flush_error = GetLastError();

            if (record->virtual_alloc)
                ok = VirtualFree(record->base, 0, MEM_RELEASE);
            else
                ok = UnmapViewOfFile(record->base);

            unmap_error = (!flush_ok) ? flush_error :
                (ok ? ERROR_SUCCESS : GetLastError());
            HeapFree(GetProcessHeap(), 0, record);
            if (!flush_ok || !ok) {
                astrometry_mmap_set_errno(unmap_error);
                return -1;
            }
            return 0;
        }

        prev = record;
        record = record->next;
    }

    if (UnmapViewOfFile(addr))
        return 0;

    astrometry_mmap_set_errno(GetLastError());
    return -1;
}

#define mmap astrometry_mmap
#define munmap astrometry_munmap

#else

#include <sys/mman.h>

#endif

#endif
