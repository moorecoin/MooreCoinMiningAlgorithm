// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "protocol.h"

#include "util.h"
#include "utilstrencodings.h"

#ifndef win32
# include <arpa/inet.h>
#endif

static const char* ppsztypename[] =
{
    "error",
    "tx",
    "block",
    "filtered block"
};

cmessageheader::cmessageheader(const messagestartchars& pchmessagestartin)
{
    memcpy(pchmessagestart, pchmessagestartin, message_start_size);
    memset(pchcommand, 0, sizeof(pchcommand));
    nmessagesize = -1;
    nchecksum = 0;
}

cmessageheader::cmessageheader(const messagestartchars& pchmessagestartin, const char* pszcommand, unsigned int nmessagesizein)
{
    memcpy(pchmessagestart, pchmessagestartin, message_start_size);
    memset(pchcommand, 0, sizeof(pchcommand));
    strncpy(pchcommand, pszcommand, command_size);
    nmessagesize = nmessagesizein;
    nchecksum = 0;
}

std::string cmessageheader::getcommand() const
{
    return std::string(pchcommand, pchcommand + strnlen(pchcommand, command_size));
}

bool cmessageheader::isvalid(const messagestartchars& pchmessagestartin) const
{
    // check start string
    if (memcmp(pchmessagestart, pchmessagestartin, message_start_size) != 0)
        return false;

    // check the command string for errors
    for (const char* p1 = pchcommand; p1 < pchcommand + command_size; p1++)
    {
        if (*p1 == 0)
        {
            // must be all zeros after the first zero
            for (; p1 < pchcommand + command_size; p1++)
                if (*p1 != 0)
                    return false;
        }
        else if (*p1 < ' ' || *p1 > 0x7e)
            return false;
    }

    // message size
    if (nmessagesize > max_size)
    {
        logprintf("cmessageheader::isvalid(): (%s, %u bytes) nmessagesize > max_size\n", getcommand(), nmessagesize);
        return false;
    }

    return true;
}



caddress::caddress() : cservice()
{
    init();
}

caddress::caddress(cservice ipin, uint64_t nservicesin) : cservice(ipin)
{
    init();
    nservices = nservicesin;
}

void caddress::init()
{
    nservices = node_network;
    ntime = 100000000;
}

cinv::cinv()
{
    type = 0;
    hash.setnull();
}

cinv::cinv(int typein, const uint256& hashin)
{
    type = typein;
    hash = hashin;
}

cinv::cinv(const std::string& strtype, const uint256& hashin)
{
    unsigned int i;
    for (i = 1; i < arraylen(ppsztypename); i++)
    {
        if (strtype == ppsztypename[i])
        {
            type = i;
            break;
        }
    }
    if (i == arraylen(ppsztypename))
        throw std::out_of_range(strprintf("cinv::cinv(string, uint256): unknown type '%s'", strtype));
    hash = hashin;
}

bool operator<(const cinv& a, const cinv& b)
{
    return (a.type < b.type || (a.type == b.type && a.hash < b.hash));
}

bool cinv::isknowntype() const
{
    return (type >= 1 && type < (int)arraylen(ppsztypename));
}

const char* cinv::getcommand() const
{
    if (!isknowntype())
        throw std::out_of_range(strprintf("cinv::getcommand(): type=%d unknown type", type));
    return ppsztypename[type];
}

std::string cinv::tostring() const
{
    return strprintf("%s %s", getcommand(), hash.tostring());
}
