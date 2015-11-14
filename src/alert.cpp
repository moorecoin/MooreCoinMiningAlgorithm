// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "alert.h"

#include "clientversion.h"
#include "net.h"
#include "pubkey.h"
#include "timedata.h"
#include "ui_interface.h"
#include "util.h"

#include <stdint.h>
#include <algorithm>
#include <map>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

using namespace std;

map<uint256, calert> mapalerts;
ccriticalsection cs_mapalerts;

void cunsignedalert::setnull()
{
    nversion = 1;
    nrelayuntil = 0;
    nexpiration = 0;
    nid = 0;
    ncancel = 0;
    setcancel.clear();
    nminver = 0;
    nmaxver = 0;
    setsubver.clear();
    npriority = 0;

    strcomment.clear();
    strstatusbar.clear();
    strreserved.clear();
}

std::string cunsignedalert::tostring() const
{
    std::string strsetcancel;
    boost_foreach(int n, setcancel)
        strsetcancel += strprintf("%d ", n);
    std::string strsetsubver;
    boost_foreach(const std::string& str, setsubver)
        strsetsubver += "\"" + str + "\" ";
    return strprintf(
        "calert(\n"
        "    nversion     = %d\n"
        "    nrelayuntil  = %d\n"
        "    nexpiration  = %d\n"
        "    nid          = %d\n"
        "    ncancel      = %d\n"
        "    setcancel    = %s\n"
        "    nminver      = %d\n"
        "    nmaxver      = %d\n"
        "    setsubver    = %s\n"
        "    npriority    = %d\n"
        "    strcomment   = \"%s\"\n"
        "    strstatusbar = \"%s\"\n"
        ")\n",
        nversion,
        nrelayuntil,
        nexpiration,
        nid,
        ncancel,
        strsetcancel,
        nminver,
        nmaxver,
        strsetsubver,
        npriority,
        strcomment,
        strstatusbar);
}

void calert::setnull()
{
    cunsignedalert::setnull();
    vchmsg.clear();
    vchsig.clear();
}

bool calert::isnull() const
{
    return (nexpiration == 0);
}

uint256 calert::gethash() const
{
    return hash(this->vchmsg.begin(), this->vchmsg.end());
}

bool calert::isineffect() const
{
    return (getadjustedtime() < nexpiration);
}

bool calert::cancels(const calert& alert) const
{
    if (!isineffect())
        return false; // this was a no-op before 31403
    return (alert.nid <= ncancel || setcancel.count(alert.nid));
}

bool calert::appliesto(int nversion, const std::string& strsubverin) const
{
    // todo: rework for client-version-embedded-in-strsubver ?
    return (isineffect() &&
            nminver <= nversion && nversion <= nmaxver &&
            (setsubver.empty() || setsubver.count(strsubverin)));
}

bool calert::appliestome() const
{
    return appliesto(protocol_version, formatsubversion(client_name, client_version, std::vector<std::string>()));
}

bool calert::relayto(cnode* pnode) const
{
    if (!isineffect())
        return false;
    // don't relay to nodes which haven't sent their version message
    if (pnode->nversion == 0)
        return false;
    // returns true if wasn't already contained in the set
    if (pnode->setknown.insert(gethash()).second)
    {
        if (appliesto(pnode->nversion, pnode->strsubver) ||
            appliestome() ||
            getadjustedtime() < nrelayuntil)
        {
            pnode->pushmessage("alert", *this);
            return true;
        }
    }
    return false;
}

bool calert::checksignature(const std::vector<unsigned char>& alertkey) const
{
    cpubkey key(alertkey);
    if (!key.verify(hash(vchmsg.begin(), vchmsg.end()), vchsig))
        return error("calert::checksignature(): verify signature failed");

    // now unserialize the data
    cdatastream smsg(vchmsg, ser_network, protocol_version);
    smsg >> *(cunsignedalert*)this;
    return true;
}

calert calert::getalertbyhash(const uint256 &hash)
{
    calert retval;
    {
        lock(cs_mapalerts);
        map<uint256, calert>::iterator mi = mapalerts.find(hash);
        if(mi != mapalerts.end())
            retval = mi->second;
    }
    return retval;
}

bool calert::processalert(const std::vector<unsigned char>& alertkey, bool fthread)
{
    if (!checksignature(alertkey))
        return false;
    if (!isineffect())
        return false;

    // alert.nid=max is reserved for if the alert key is
    // compromised. it must have a pre-defined message,
    // must never expire, must apply to all versions,
    // and must cancel all previous
    // alerts or it will be ignored (so an attacker can't
    // send an "everything is ok, don't panic" version that
    // cannot be overridden):
    int maxint = std::numeric_limits<int>::max();
    if (nid == maxint)
    {
        if (!(
                nexpiration == maxint &&
                ncancel == (maxint-1) &&
                nminver == 0 &&
                nmaxver == maxint &&
                setsubver.empty() &&
                npriority == maxint &&
                strstatusbar == "urgent: alert key compromised, upgrade required"
                ))
            return false;
    }

    {
        lock(cs_mapalerts);
        // cancel previous alerts
        for (map<uint256, calert>::iterator mi = mapalerts.begin(); mi != mapalerts.end();)
        {
            const calert& alert = (*mi).second;
            if (cancels(alert))
            {
                logprint("alert", "cancelling alert %d\n", alert.nid);
                uiinterface.notifyalertchanged((*mi).first, ct_deleted);
                mapalerts.erase(mi++);
            }
            else if (!alert.isineffect())
            {
                logprint("alert", "expiring alert %d\n", alert.nid);
                uiinterface.notifyalertchanged((*mi).first, ct_deleted);
                mapalerts.erase(mi++);
            }
            else
                mi++;
        }

        // check if this alert has been cancelled
        boost_foreach(pairtype(const uint256, calert)& item, mapalerts)
        {
            const calert& alert = item.second;
            if (alert.cancels(*this))
            {
                logprint("alert", "alert already cancelled by %d\n", alert.nid);
                return false;
            }
        }

        // add to mapalerts
        mapalerts.insert(make_pair(gethash(), *this));
        // notify ui and -alertnotify if it applies to me
        if(appliestome())
        {
            uiinterface.notifyalertchanged(gethash(), ct_new);
            notify(strstatusbar, fthread);
        }
    }

    logprint("alert", "accepted alert %d, appliestome()=%d\n", nid, appliestome());
    return true;
}

void
calert::notify(const std::string& strmessage, bool fthread)
{
    std::string strcmd = getarg("-alertnotify", "");
    if (strcmd.empty()) return;

    // alert text should be plain ascii coming from a trusted source, but to
    // be safe we first strip anything not in safechars, then add single quotes around
    // the whole string before passing it to the shell:
    std::string singlequote("'");
    std::string safestatus = sanitizestring(strmessage);
    safestatus = singlequote+safestatus+singlequote;
    boost::replace_all(strcmd, "%s", safestatus);

    if (fthread)
        boost::thread t(runcommand, strcmd); // thread runs free
    else
        runcommand(strcmd);
}
