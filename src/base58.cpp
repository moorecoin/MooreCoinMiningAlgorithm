// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"

#include "hash.h"
#include "uint256.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

/** all alphanumeric characters except for "0", "i", "o", and "l" */
static const char* pszbase58 = "123456789abcdefghjklmnpqrstuvwxyzabcdefghijkmnopqrstuvwxyz";

bool decodebase58(const char* psz, std::vector<unsigned char>& vch)
{
    // skip leading spaces.
    while (*psz && isspace(*psz))
        psz++;
    // skip and count leading '1's.
    int zeroes = 0;
    while (*psz == '1') {
        zeroes++;
        psz++;
    }
    // allocate enough space in big-endian base256 representation.
    std::vector<unsigned char> b256(strlen(psz) * 733 / 1000 + 1); // log(58) / log(256), rounded up.
    // process the characters.
    while (*psz && !isspace(*psz)) {
        // decode base58 character
        const char* ch = strchr(pszbase58, *psz);
        if (ch == null)
            return false;
        // apply "b256 = b256 * 58 + ch".
        int carry = ch - pszbase58;
        for (std::vector<unsigned char>::reverse_iterator it = b256.rbegin(); it != b256.rend(); it++) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        psz++;
    }
    // skip trailing spaces.
    while (isspace(*psz))
        psz++;
    if (*psz != 0)
        return false;
    // skip leading zeroes in b256.
    std::vector<unsigned char>::iterator it = b256.begin();
    while (it != b256.end() && *it == 0)
        it++;
    // copy result into output vector.
    vch.reserve(zeroes + (b256.end() - it));
    vch.assign(zeroes, 0x00);
    while (it != b256.end())
        vch.push_back(*(it++));
    return true;
}

std::string encodebase58(const unsigned char* pbegin, const unsigned char* pend)
{
    // skip & count leading zeroes.
    int zeroes = 0;
    while (pbegin != pend && *pbegin == 0) {
        pbegin++;
        zeroes++;
    }
    // allocate enough space in big-endian base58 representation.
    std::vector<unsigned char> b58((pend - pbegin) * 138 / 100 + 1); // log(256) / log(58), rounded up.
    // process the bytes.
    while (pbegin != pend) {
        int carry = *pbegin;
        // apply "b58 = b58 * 256 + ch".
        for (std::vector<unsigned char>::reverse_iterator it = b58.rbegin(); it != b58.rend(); it++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
        assert(carry == 0);
        pbegin++;
    }
    // skip leading zeroes in base58 result.
    std::vector<unsigned char>::iterator it = b58.begin();
    while (it != b58.end() && *it == 0)
        it++;
    // translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58.end() - it));
    str.assign(zeroes, '1');
    while (it != b58.end())
        str += pszbase58[*(it++)];
    return str;
}

std::string encodebase58(const std::vector<unsigned char>& vch)
{
    return encodebase58(&vch[0], &vch[0] + vch.size());
}

bool decodebase58(const std::string& str, std::vector<unsigned char>& vchret)
{
    return decodebase58(str.c_str(), vchret);
}

std::string encodebase58check(const std::vector<unsigned char>& vchin)
{
    // add 4-byte hash check to the end
    std::vector<unsigned char> vch(vchin);
    uint256 hash = hash(vch.begin(), vch.end());
    vch.insert(vch.end(), (unsigned char*)&hash, (unsigned char*)&hash + 4);
    return encodebase58(vch);
}

bool decodebase58check(const char* psz, std::vector<unsigned char>& vchret)
{
    if (!decodebase58(psz, vchret) ||
        (vchret.size() < 4)) {
        vchret.clear();
        return false;
    }
    // re-calculate the checksum, insure it matches the included 4-byte checksum
    uint256 hash = hash(vchret.begin(), vchret.end() - 4);
    if (memcmp(&hash, &vchret.end()[-4], 4) != 0) {
        vchret.clear();
        return false;
    }
    vchret.resize(vchret.size() - 4);
    return true;
}

bool decodebase58check(const std::string& str, std::vector<unsigned char>& vchret)
{
    return decodebase58check(str.c_str(), vchret);
}

cbase58data::cbase58data()
{
    vchversion.clear();
    vchdata.clear();
}

void cbase58data::setdata(const std::vector<unsigned char>& vchversionin, const void* pdata, size_t nsize)
{
    vchversion = vchversionin;
    vchdata.resize(nsize);
    if (!vchdata.empty())
        memcpy(&vchdata[0], pdata, nsize);
}

void cbase58data::setdata(const std::vector<unsigned char>& vchversionin, const unsigned char* pbegin, const unsigned char* pend)
{
    setdata(vchversionin, (void*)pbegin, pend - pbegin);
}

bool cbase58data::setstring(const char* psz, unsigned int nversionbytes)
{
    std::vector<unsigned char> vchtemp;
    bool rc58 = decodebase58check(psz, vchtemp);
    if ((!rc58) || (vchtemp.size() < nversionbytes)) {
        vchdata.clear();
        vchversion.clear();
        return false;
    }
    vchversion.assign(vchtemp.begin(), vchtemp.begin() + nversionbytes);
    vchdata.resize(vchtemp.size() - nversionbytes);
    if (!vchdata.empty())
        memcpy(&vchdata[0], &vchtemp[nversionbytes], vchdata.size());
    memory_cleanse(&vchtemp[0], vchdata.size());
    return true;
}

bool cbase58data::setstring(const std::string& str)
{
    return setstring(str.c_str());
}

std::string cbase58data::tostring() const
{
    std::vector<unsigned char> vch = vchversion;
    vch.insert(vch.end(), vchdata.begin(), vchdata.end());
    return encodebase58check(vch);
}

int cbase58data::compareto(const cbase58data& b58) const
{
    if (vchversion < b58.vchversion)
        return -1;
    if (vchversion > b58.vchversion)
        return 1;
    if (vchdata < b58.vchdata)
        return -1;
    if (vchdata > b58.vchdata)
        return 1;
    return 0;
}

namespace
{
class cmoorecoinaddressvisitor : public boost::static_visitor<bool>
{
private:
    cmoorecoinaddress* addr;

public:
    cmoorecoinaddressvisitor(cmoorecoinaddress* addrin) : addr(addrin) {}

    bool operator()(const ckeyid& id) const { return addr->set(id); }
    bool operator()(const cscriptid& id) const { return addr->set(id); }
    bool operator()(const cnodestination& no) const { return false; }
};

} // anon namespace

bool cmoorecoinaddress::set(const ckeyid& id)
{
    setdata(params().base58prefix(cchainparams::pubkey_address), &id, 20);
    return true;
}

bool cmoorecoinaddress::set(const cscriptid& id)
{
    setdata(params().base58prefix(cchainparams::script_address), &id, 20);
    return true;
}

bool cmoorecoinaddress::set(const ctxdestination& dest)
{
    return boost::apply_visitor(cmoorecoinaddressvisitor(this), dest);
}

bool cmoorecoinaddress::isvalid() const
{
    return isvalid(params());
}

bool cmoorecoinaddress::isvalid(const cchainparams& params) const
{
    bool fcorrectsize = vchdata.size() == 20;
    bool fknownversion = vchversion == params.base58prefix(cchainparams::pubkey_address) ||
                         vchversion == params.base58prefix(cchainparams::script_address);
    return fcorrectsize && fknownversion;
}

ctxdestination cmoorecoinaddress::get() const
{
    if (!isvalid())
        return cnodestination();
    uint160 id;
    memcpy(&id, &vchdata[0], 20);
    if (vchversion == params().base58prefix(cchainparams::pubkey_address))
        return ckeyid(id);
    else if (vchversion == params().base58prefix(cchainparams::script_address))
        return cscriptid(id);
    else
        return cnodestination();
}

bool cmoorecoinaddress::getkeyid(ckeyid& keyid) const
{
    if (!isvalid() || vchversion != params().base58prefix(cchainparams::pubkey_address))
        return false;
    uint160 id;
    memcpy(&id, &vchdata[0], 20);
    keyid = ckeyid(id);
    return true;
}

bool cmoorecoinaddress::isscript() const
{
    return isvalid() && vchversion == params().base58prefix(cchainparams::script_address);
}

void cmoorecoinsecret::setkey(const ckey& vchsecret)
{
    assert(vchsecret.isvalid());
    setdata(params().base58prefix(cchainparams::secret_key), vchsecret.begin(), vchsecret.size());
    if (vchsecret.iscompressed())
        vchdata.push_back(1);
}

ckey cmoorecoinsecret::getkey()
{
    ckey ret;
    assert(vchdata.size() >= 32);
    ret.set(vchdata.begin(), vchdata.begin() + 32, vchdata.size() > 32 && vchdata[32] == 1);
    return ret;
}

bool cmoorecoinsecret::isvalid() const
{
    bool fexpectedformat = vchdata.size() == 32 || (vchdata.size() == 33 && vchdata[32] == 1);
    bool fcorrectversion = vchversion == params().base58prefix(cchainparams::secret_key);
    return fexpectedformat && fcorrectversion;
}

bool cmoorecoinsecret::setstring(const char* pszsecret)
{
    return cbase58data::setstring(pszsecret) && isvalid();
}

bool cmoorecoinsecret::setstring(const std::string& strsecret)
{
    return setstring(strsecret.c_str());
}
