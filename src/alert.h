// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_alert_h
#define moorecoin_alert_h

#include "serialize.h"
#include "sync.h"

#include <map>
#include <set>
#include <stdint.h>
#include <string>

class calert;
class cnode;
class uint256;

extern std::map<uint256, calert> mapalerts;
extern ccriticalsection cs_mapalerts;

/** alerts are for notifying old versions if they become too obsolete and
 * need to upgrade.  the message is displayed in the status bar.
 * alert messages are broadcast as a vector of signed data.  unserializing may
 * not read the entire buffer if the alert is for a newer version, but older
 * versions can still relay the original data.
 */
class cunsignedalert
{
public:
    int nversion;
    int64_t nrelayuntil;      // when newer nodes stop relaying to newer nodes
    int64_t nexpiration;
    int nid;
    int ncancel;
    std::set<int> setcancel;
    int nminver;            // lowest version inclusive
    int nmaxver;            // highest version inclusive
    std::set<std::string> setsubver;  // empty matches all
    int npriority;

    // actions
    std::string strcomment;
    std::string strstatusbar;
    std::string strreserved;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(this->nversion);
        nversion = this->nversion;
        readwrite(nrelayuntil);
        readwrite(nexpiration);
        readwrite(nid);
        readwrite(ncancel);
        readwrite(setcancel);
        readwrite(nminver);
        readwrite(nmaxver);
        readwrite(setsubver);
        readwrite(npriority);

        readwrite(limited_string(strcomment, 65536));
        readwrite(limited_string(strstatusbar, 256));
        readwrite(limited_string(strreserved, 256));
    }

    void setnull();

    std::string tostring() const;
};

/** an alert is a combination of a serialized cunsignedalert and a signature. */
class calert : public cunsignedalert
{
public:
    std::vector<unsigned char> vchmsg;
    std::vector<unsigned char> vchsig;

    calert()
    {
        setnull();
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(vchmsg);
        readwrite(vchsig);
    }

    void setnull();
    bool isnull() const;
    uint256 gethash() const;
    bool isineffect() const;
    bool cancels(const calert& alert) const;
    bool appliesto(int nversion, const std::string& strsubverin) const;
    bool appliestome() const;
    bool relayto(cnode* pnode) const;
    bool checksignature(const std::vector<unsigned char>& alertkey) const;
    bool processalert(const std::vector<unsigned char>& alertkey, bool fthread = true); // fthread means run -alertnotify in a free-running thread
    static void notify(const std::string& strmessage, bool fthread);

    /*
     * get copy of (active) alert object by hash. returns a null alert if it is not found.
     */
    static calert getalertbyhash(const uint256 &hash);
};

#endif // moorecoin_alert_h
