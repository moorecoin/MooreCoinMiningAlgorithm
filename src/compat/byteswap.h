// copyright (c) 2014 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_compat_byteswap_h
#define moorecoin_compat_byteswap_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include <stdint.h>

#if defined(have_byteswap_h)
#include <byteswap.h>
#endif

#if have_decl_bswap_16 == 0
inline uint16_t bswap_16(uint16_t x)
{
    return (x >> 8) | ((x & 0x00ff) << 8);
}
#endif // have_decl_bswap16

#if have_decl_bswap_32 == 0
inline uint32_t bswap_32(uint32_t x)
{
    return (((x & 0xff000000u) >> 24) | ((x & 0x00ff0000u) >>  8) |
            ((x & 0x0000ff00u) <<  8) | ((x & 0x000000ffu) << 24));
}
#endif // have_decl_bswap32

#if have_decl_bswap_64 == 0
inline uint64_t bswap_64(uint64_t x)
{
     return (((x & 0xff00000000000000ull) >> 56)
          | ((x & 0x00ff000000000000ull) >> 40)
          | ((x & 0x0000ff0000000000ull) >> 24)
          | ((x & 0x000000ff00000000ull) >> 8)
          | ((x & 0x00000000ff000000ull) << 8)
          | ((x & 0x0000000000ff0000ull) << 24)
          | ((x & 0x000000000000ff00ull) << 40)
          | ((x & 0x00000000000000ffull) << 56));
}
#endif // have_decl_bswap64

#endif // moorecoin_compat_byteswap_h
