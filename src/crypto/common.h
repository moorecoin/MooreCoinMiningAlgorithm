// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_crypto_common_h
#define moorecoin_crypto_common_h

#if defined(have_config_h)
#include "moorecoin-config.h"
#endif

#include <stdint.h>

#include "compat/endian.h"

uint16_t static inline readle16(const unsigned char* ptr)
{
    return le16toh(*((uint16_t*)ptr));
}

uint32_t static inline readle32(const unsigned char* ptr)
{
    return le32toh(*((uint32_t*)ptr));
}

uint64_t static inline readle64(const unsigned char* ptr)
{
    return le64toh(*((uint64_t*)ptr));
}

void static inline writele16(unsigned char* ptr, uint16_t x)
{
    *((uint16_t*)ptr) = htole16(x);
}

void static inline writele32(unsigned char* ptr, uint32_t x)
{
    *((uint32_t*)ptr) = htole32(x);
}

void static inline writele64(unsigned char* ptr, uint64_t x)
{
    *((uint64_t*)ptr) = htole64(x);
}

uint32_t static inline readbe32(const unsigned char* ptr)
{
    return be32toh(*((uint32_t*)ptr));
}

uint64_t static inline readbe64(const unsigned char* ptr)
{
    return be64toh(*((uint64_t*)ptr));
}

void static inline writebe32(unsigned char* ptr, uint32_t x)
{
    *((uint32_t*)ptr) = htobe32(x);
}

void static inline writebe64(unsigned char* ptr, uint64_t x)
{
    *((uint64_t*)ptr) = htobe64(x);
}

#endif // moorecoin_crypto_common_h
