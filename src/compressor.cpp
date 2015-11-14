// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "compressor.h"

#include "hash.h"
#include "pubkey.h"
#include "script/standard.h"

bool cscriptcompressor::istokeyid(ckeyid &hash) const
{
    if (script.size() == 25 && script[0] == op_dup && script[1] == op_hash160
                            && script[2] == 20 && script[23] == op_equalverify
                            && script[24] == op_checksig) {
        memcpy(&hash, &script[3], 20);
        return true;
    }
    return false;
}

bool cscriptcompressor::istoscriptid(cscriptid &hash) const
{
    if (script.size() == 23 && script[0] == op_hash160 && script[1] == 20
                            && script[22] == op_equal) {
        memcpy(&hash, &script[2], 20);
        return true;
    }
    return false;
}

bool cscriptcompressor::istopubkey(cpubkey &pubkey) const
{
    if (script.size() == 35 && script[0] == 33 && script[34] == op_checksig
                            && (script[1] == 0x02 || script[1] == 0x03)) {
        pubkey.set(&script[1], &script[34]);
        return true;
    }
    if (script.size() == 67 && script[0] == 65 && script[66] == op_checksig
                            && script[1] == 0x04) {
        pubkey.set(&script[1], &script[66]);
        return pubkey.isfullyvalid(); // if not fully valid, a case that would not be compressible
    }
    return false;
}

bool cscriptcompressor::compress(std::vector<unsigned char> &out) const
{
    ckeyid keyid;
    if (istokeyid(keyid)) {
        out.resize(21);
        out[0] = 0x00;
        memcpy(&out[1], &keyid, 20);
        return true;
    }
    cscriptid scriptid;
    if (istoscriptid(scriptid)) {
        out.resize(21);
        out[0] = 0x01;
        memcpy(&out[1], &scriptid, 20);
        return true;
    }
    cpubkey pubkey;
    if (istopubkey(pubkey)) {
        out.resize(33);
        memcpy(&out[1], &pubkey[1], 32);
        if (pubkey[0] == 0x02 || pubkey[0] == 0x03) {
            out[0] = pubkey[0];
            return true;
        } else if (pubkey[0] == 0x04) {
            out[0] = 0x04 | (pubkey[64] & 0x01);
            return true;
        }
    }
    return false;
}

unsigned int cscriptcompressor::getspecialsize(unsigned int nsize) const
{
    if (nsize == 0 || nsize == 1)
        return 20;
    if (nsize == 2 || nsize == 3 || nsize == 4 || nsize == 5)
        return 32;
    return 0;
}

bool cscriptcompressor::decompress(unsigned int nsize, const std::vector<unsigned char> &in)
{
    switch(nsize) {
    case 0x00:
        script.resize(25);
        script[0] = op_dup;
        script[1] = op_hash160;
        script[2] = 20;
        memcpy(&script[3], &in[0], 20);
        script[23] = op_equalverify;
        script[24] = op_checksig;
        return true;
    case 0x01:
        script.resize(23);
        script[0] = op_hash160;
        script[1] = 20;
        memcpy(&script[2], &in[0], 20);
        script[22] = op_equal;
        return true;
    case 0x02:
    case 0x03:
        script.resize(35);
        script[0] = 33;
        script[1] = nsize;
        memcpy(&script[2], &in[0], 32);
        script[34] = op_checksig;
        return true;
    case 0x04:
    case 0x05:
        unsigned char vch[33] = {};
        vch[0] = nsize - 2;
        memcpy(&vch[1], &in[0], 32);
        cpubkey pubkey(&vch[0], &vch[33]);
        if (!pubkey.decompress())
            return false;
        assert(pubkey.size() == 65);
        script.resize(67);
        script[0] = 65;
        memcpy(&script[1], pubkey.begin(), 65);
        script[66] = op_checksig;
        return true;
    }
    return false;
}

// amount compression:
// * if the amount is 0, output 0
// * first, divide the amount (in base units) by the largest power of 10 possible; call the exponent e (e is max 9)
// * if e<9, the last digit of the resulting number cannot be 0; store it as d, and drop it (divide by 10)
//   * call the result n
//   * output 1 + 10*(9*n + d - 1) + e
// * if e==9, we only know the resulting number is not zero, so output 1 + 10*(n - 1) + 9
// (this is decodable, as d is in [1-9] and e is in [0-9])

uint64_t ctxoutcompressor::compressamount(uint64_t n)
{
    if (n == 0)
        return 0;
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n*9 + d - 1)*10 + e;
    } else {
        return 1 + (n - 1)*10 + 9;
    }
}

uint64_t ctxoutcompressor::decompressamount(uint64_t x)
{
    // x = 0  or  x = 1+10*(9*n + d - 1) + e  or  x = 1+10*(n - 1) + 9
    if (x == 0)
        return 0;
    x--;
    // x = 10*(9*n + d - 1) + e
    int e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        // x = 9*n + d - 1
        int d = (x % 9) + 1;
        x /= 9;
        // x = n
        n = x*10 + d;
    } else {
        n = x+1;
    }
    while (e) {
        n *= 10;
        e--;
    }
    return n;
}
