/*
 * Codex!
 */

#ifndef ASTROMETRY_NET_COMPAT_H
#define ASTROMETRY_NET_COMPAT_H

#include <stdint.h>

#if defined(_WIN32) && !defined(__CYGWIN__)

static inline uint16_t astrometry_bswap16(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline uint32_t astrometry_bswap32(uint32_t v) {
    return ((v & 0x000000ffu) << 24) |
        ((v & 0x0000ff00u) << 8) |
        ((v & 0x00ff0000u) >> 8) |
        ((v & 0xff000000u) >> 24);
}

static inline uint16_t astrometry_htons(uint16_t v) {
    return astrometry_bswap16(v);
}

static inline uint16_t astrometry_ntohs(uint16_t v) {
    return astrometry_bswap16(v);
}

static inline uint32_t astrometry_htonl(uint32_t v) {
    return astrometry_bswap32(v);
}

static inline uint32_t astrometry_ntohl(uint32_t v) {
    return astrometry_bswap32(v);
}

#ifndef htons
#define htons astrometry_htons
#endif
#ifndef ntohs
#define ntohs astrometry_ntohs
#endif
#ifndef htonl
#define htonl astrometry_htonl
#endif
#ifndef ntohl
#define ntohl astrometry_ntohl
#endif

#else

#include <arpa/inet.h>
#include <netinet/in.h>

#endif

#endif
