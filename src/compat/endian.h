// copyright (c) 2014 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_compat_endian_h
#define moorecoin_compat_endian_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include <stdint.h>

#include "compat/byteswap.h"

#if defined(have_endian_h)
#include <endian.h>
#elif defined(have_sys_endian_h)
#include <sys/endian.h>
#endif

#if defined(words_bigendian)

#if have_decl_htobe16 == 0
inline uint16_t htobe16(uint16_t host_16bits)
{
    return host_16bits;
}
#endif // have_decl_htobe16

#if have_decl_htole16 == 0
inline uint16_t htole16(uint16_t host_16bits)
{
    return bswap_16(host_16bits);
}
#endif // have_decl_htole16

#if have_decl_be16toh == 0
inline uint16_t be16toh(uint16_t big_endian_16bits)
{
    return big_endian_16bits;
}
#endif // have_decl_be16toh

#if have_decl_le16toh == 0
inline uint16_t le16toh(uint16_t little_endian_16bits)
{
    return bswap_16(little_endian_16bits);
}
#endif // have_decl_le16toh

#if have_decl_htobe32 == 0
inline uint32_t htobe32(uint32_t host_32bits)
{
    return host_32bits;
}
#endif // have_decl_htobe32

#if have_decl_htole32 == 0
inline uint32_t htole32(uint32_t host_32bits)
{
    return bswap_32(host_32bits);
}
#endif // have_decl_htole32

#if have_decl_be32toh == 0
inline uint32_t be32toh(uint32_t big_endian_32bits)
{
    return big_endian_32bits;
}
#endif // have_decl_be32toh

#if have_decl_le32toh == 0
inline uint32_t le32toh(uint32_t little_endian_32bits)
{
    return bswap_32(little_endian_32bits);
}
#endif // have_decl_le32toh

#if have_decl_htobe64 == 0
inline uint64_t htobe64(uint64_t host_64bits)
{
    return host_64bits;
}
#endif // have_decl_htobe64

#if have_decl_htole64 == 0
inline uint64_t htole64(uint64_t host_64bits)
{
    return bswap_64(host_64bits);
}
#endif // have_decl_htole64

#if have_decl_be64toh == 0
inline uint64_t be64toh(uint64_t big_endian_64bits)
{
    return big_endian_64bits;
}
#endif // have_decl_be64toh

#if have_decl_le64toh == 0
inline uint64_t le64toh(uint64_t little_endian_64bits)
{
    return bswap_64(little_endian_64bits);
}
#endif // have_decl_le64toh

#else // words_bigendian

#if have_decl_htobe16 == 0
inline uint16_t htobe16(uint16_t host_16bits)
{
    return bswap_16(host_16bits);
}
#endif // have_decl_htobe16

#if have_decl_htole16 == 0
inline uint16_t htole16(uint16_t host_16bits)
{
    return host_16bits;
}
#endif // have_decl_htole16

#if have_decl_be16toh == 0
inline uint16_t be16toh(uint16_t big_endian_16bits)
{
    return bswap_16(big_endian_16bits);
}
#endif // have_decl_be16toh

#if have_decl_le16toh == 0
inline uint16_t le16toh(uint16_t little_endian_16bits)
{
    return little_endian_16bits;
}
#endif // have_decl_le16toh

#if have_decl_htobe32 == 0
inline uint32_t htobe32(uint32_t host_32bits)
{
    return bswap_32(host_32bits);
}
#endif // have_decl_htobe32

#if have_decl_htole32 == 0
inline uint32_t htole32(uint32_t host_32bits)
{
    return host_32bits;
}
#endif // have_decl_htole32

#if have_decl_be32toh == 0
inline uint32_t be32toh(uint32_t big_endian_32bits)
{
    return bswap_32(big_endian_32bits);
}
#endif // have_decl_be32toh

#if have_decl_le32toh == 0
inline uint32_t le32toh(uint32_t little_endian_32bits)
{
    return little_endian_32bits;
}
#endif // have_decl_le32toh

#if have_decl_htobe64 == 0
inline uint64_t htobe64(uint64_t host_64bits)
{
    return bswap_64(host_64bits);
}
#endif // have_decl_htobe64

#if have_decl_htole64 == 0
inline uint64_t htole64(uint64_t host_64bits)
{
    return host_64bits;
}
#endif // have_decl_htole64

#if have_decl_be64toh == 0
inline uint64_t be64toh(uint64_t big_endian_64bits)
{
    return bswap_64(big_endian_64bits);
}
#endif // have_decl_be64toh

#if have_decl_le64toh == 0
inline uint64_t le64toh(uint64_t little_endian_64bits)
{
    return little_endian_64bits;
}
#endif // have_decl_le64toh

#endif // words_bigendian

#endif // moorecoin_compat_endian_h
