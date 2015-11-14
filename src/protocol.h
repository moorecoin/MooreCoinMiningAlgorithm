// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
#error this header can only be compiled as c++.
#endif

#ifndef moorecoin_protocol_h
#define moorecoin_protocol_h

#include "netbase.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"

#include <stdint.h>
#include <string>

#define message_start_size 4

/** message header.
 * (4) message start.
 * (12) command.
 * (4) size.
 * (4) checksum.
 */
class cmessageheader
{
public:
    typedef unsigned char messagestartchars[message_start_size];

    cmessageheader(const messagestartchars& pchmessagestartin);
    cmessageheader(const messagestartchars& pchmessagestartin, const char* pszcommand, unsigned int nmessagesizein);

    std::string getcommand() const;
    bool isvalid(const messagestartchars& messagestart) const;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion)
    {
        readwrite(flatdata(pchmessagestart));
        readwrite(flatdata(pchcommand));
        readwrite(nmessagesize);
        readwrite(nchecksum);
    }

    // todo: make private (improves encapsulation)
public:
    enum {
        command_size = 12,
        message_size_size = sizeof(int),
        checksum_size = sizeof(int),

        message_size_offset = message_start_size + command_size,
        checksum_offset = message_size_offset + message_size_size,
        header_size = message_start_size + command_size + message_size_size + checksum_size
    };
    char pchmessagestart[message_start_size];
    char pchcommand[command_size];
    unsigned int nmessagesize;
    unsigned int nchecksum;
};

/** nservices flags */
enum {
    // node_network means that the node is capable of serving the block chain. it is currently
    // set by all moorecoin core nodes, and is unset by spv clients or other peers that just want
    // network services but don't provide them.
    node_network = (1 << 0),
    // node_getutxo means the node is capable of responding to the getutxo protocol request.
    // moorecoin core does not support this but a patch set called moorecoin xt does.
    // see bip 64 for details on how this is implemented.
    node_getutxo = (1 << 1),

    // bits 24-31 are reserved for temporary experiments. just pick a bit that
    // isn't getting used, or one not being used much, and notify the
    // moorecoin-development mailing list. remember that service bits are just
    // unauthenticated advertisements, so your code must be robust against
    // collisions and other cases where nodes may be advertising a service they
    // do not actually support. other service bits should be allocated via the
    // bip process.
};

/** a cservice with information about it as peer */
class caddress : public cservice
{
public:
    caddress();
    explicit caddress(cservice ipin, uint64_t nservicesin = node_network);

    void init();

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion)
    {
        if (ser_action.forread())
            init();
        if (ntype & ser_disk)
            readwrite(nversion);
        if ((ntype & ser_disk) ||
            (nversion >= caddr_time_version && !(ntype & ser_gethash)))
            readwrite(ntime);
        readwrite(nservices);
        readwrite(*(cservice*)this);
    }

    // todo: make private (improves encapsulation)
public:
    uint64_t nservices;

    // disk and network only
    unsigned int ntime;
};

/** inv message data */
class cinv
{
public:
    cinv();
    cinv(int typein, const uint256& hashin);
    cinv(const std::string& strtype, const uint256& hashin);

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion)
    {
        readwrite(type);
        readwrite(hash);
    }

    friend bool operator<(const cinv& a, const cinv& b);

    bool isknowntype() const;
    const char* getcommand() const;
    std::string tostring() const;

    // todo: make private (improves encapsulation)
public:
    int type;
    uint256 hash;
};

enum {
    msg_tx = 1,
    msg_block,
    // nodes may always request a msg_filtered_block in a getdata, however,
    // msg_filtered_block should not appear in any invs except as a part of getdata.
    msg_filtered_block,
};

#endif // moorecoin_protocol_h
