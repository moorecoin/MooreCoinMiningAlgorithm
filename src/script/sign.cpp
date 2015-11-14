// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "script/sign.h"

#include "primitives/transaction.h"
#include "key.h"
#include "keystore.h"
#include "script/standard.h"
#include "uint256.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

transactionsignaturecreator::transactionsignaturecreator(const ckeystore* keystorein, const ctransaction* txtoin, unsigned int ninin, int nhashtypein) : basesignaturecreator(keystorein), txto(txtoin), nin(ninin), nhashtype(nhashtypein), checker(txto, nin) {}

bool transactionsignaturecreator::createsig(std::vector<unsigned char>& vchsig, const ckeyid& address, const cscript& scriptcode) const
{
    ckey key;
    if (!keystore->getkey(address, key))
        return false;

    uint256 hash = signaturehash(scriptcode, *txto, nin, nhashtype);
    if (!key.sign(hash, vchsig))
        return false;
    vchsig.push_back((unsigned char)nhashtype);
    return true;
}

static bool sign1(const ckeyid& address, const basesignaturecreator& creator, const cscript& scriptcode, cscript& scriptsigret)
{
    vector<unsigned char> vchsig;
    if (!creator.createsig(vchsig, address, scriptcode))
        return false;
    scriptsigret << vchsig;
    return true;
}

static bool signn(const vector<valtype>& multisigdata, const basesignaturecreator& creator, const cscript& scriptcode, cscript& scriptsigret)
{
    int nsigned = 0;
    int nrequired = multisigdata.front()[0];
    for (unsigned int i = 1; i < multisigdata.size()-1 && nsigned < nrequired; i++)
    {
        const valtype& pubkey = multisigdata[i];
        ckeyid keyid = cpubkey(pubkey).getid();
        if (sign1(keyid, creator, scriptcode, scriptsigret))
            ++nsigned;
    }
    return nsigned==nrequired;
}

/**
 * sign scriptpubkey using signature made with creator.
 * signatures are returned in scriptsigret (or returns false if scriptpubkey can't be signed),
 * unless whichtyperet is tx_scripthash, in which case scriptsigret is the redemption script.
 * returns false if scriptpubkey could not be completely satisfied.
 */
static bool signstep(const basesignaturecreator& creator, const cscript& scriptpubkey,
                     cscript& scriptsigret, txnouttype& whichtyperet)
{
    scriptsigret.clear();

    vector<valtype> vsolutions;
    if (!solver(scriptpubkey, whichtyperet, vsolutions))
        return false;

    ckeyid keyid;
    switch (whichtyperet)
    {
    case tx_nonstandard:
    case tx_null_data:
        return false;
    case tx_pubkey:
        keyid = cpubkey(vsolutions[0]).getid();
        return sign1(keyid, creator, scriptpubkey, scriptsigret);
    case tx_pubkeyhash:
        keyid = ckeyid(uint160(vsolutions[0]));
        if (!sign1(keyid, creator, scriptpubkey, scriptsigret))
            return false;
        else
        {
            cpubkey vch;
            creator.keystore().getpubkey(keyid, vch);
            scriptsigret << tobytevector(vch);
        }
        return true;
    case tx_scripthash:
        return creator.keystore().getcscript(uint160(vsolutions[0]), scriptsigret);

    case tx_multisig:
        scriptsigret << op_0; // workaround checkmultisig bug
        return (signn(vsolutions, creator, scriptpubkey, scriptsigret));
    }
    return false;
}

bool producesignature(const basesignaturecreator& creator, const cscript& frompubkey, cscript& scriptsig)
{
    txnouttype whichtype;
    if (!signstep(creator, frompubkey, scriptsig, whichtype))
        return false;

    if (whichtype == tx_scripthash)
    {
        // solver returns the subscript that need to be evaluated;
        // the final scriptsig is the signatures from that
        // and then the serialized subscript:
        cscript subscript = scriptsig;

        txnouttype subtype;
        bool fsolved =
            signstep(creator, subscript, scriptsig, subtype) && subtype != tx_scripthash;
        // append serialized subscript whether or not it is completely signed:
        scriptsig << static_cast<valtype>(subscript);
        if (!fsolved) return false;
    }

    // test solution
    return verifyscript(scriptsig, frompubkey, standard_script_verify_flags, creator.checker());
}

bool signsignature(const ckeystore &keystore, const cscript& frompubkey, cmutabletransaction& txto, unsigned int nin, int nhashtype)
{
    assert(nin < txto.vin.size());
    ctxin& txin = txto.vin[nin];

    ctransaction txtoconst(txto);
    transactionsignaturecreator creator(&keystore, &txtoconst, nin, nhashtype);

    return producesignature(creator, frompubkey, txin.scriptsig);
}

bool signsignature(const ckeystore &keystore, const ctransaction& txfrom, cmutabletransaction& txto, unsigned int nin, int nhashtype)
{
    assert(nin < txto.vin.size());
    ctxin& txin = txto.vin[nin];
    assert(txin.prevout.n < txfrom.vout.size());
    const ctxout& txout = txfrom.vout[txin.prevout.n];

    return signsignature(keystore, txout.scriptpubkey, txto, nin, nhashtype);
}

static cscript pushall(const vector<valtype>& values)
{
    cscript result;
    boost_foreach(const valtype& v, values)
        result << v;
    return result;
}

static cscript combinemultisig(const cscript& scriptpubkey, const basesignaturechecker& checker,
                               const vector<valtype>& vsolutions,
                               const vector<valtype>& sigs1, const vector<valtype>& sigs2)
{
    // combine all the signatures we've got:
    set<valtype> allsigs;
    boost_foreach(const valtype& v, sigs1)
    {
        if (!v.empty())
            allsigs.insert(v);
    }
    boost_foreach(const valtype& v, sigs2)
    {
        if (!v.empty())
            allsigs.insert(v);
    }

    // build a map of pubkey -> signature by matching sigs to pubkeys:
    assert(vsolutions.size() > 1);
    unsigned int nsigsrequired = vsolutions.front()[0];
    unsigned int npubkeys = vsolutions.size()-2;
    map<valtype, valtype> sigs;
    boost_foreach(const valtype& sig, allsigs)
    {
        for (unsigned int i = 0; i < npubkeys; i++)
        {
            const valtype& pubkey = vsolutions[i+1];
            if (sigs.count(pubkey))
                continue; // already got a sig for this pubkey

            if (checker.checksig(sig, pubkey, scriptpubkey))
            {
                sigs[pubkey] = sig;
                break;
            }
        }
    }
    // now build a merged cscript:
    unsigned int nsigshave = 0;
    cscript result; result << op_0; // pop-one-too-many workaround
    for (unsigned int i = 0; i < npubkeys && nsigshave < nsigsrequired; i++)
    {
        if (sigs.count(vsolutions[i+1]))
        {
            result << sigs[vsolutions[i+1]];
            ++nsigshave;
        }
    }
    // fill any missing with op_0:
    for (unsigned int i = nsigshave; i < nsigsrequired; i++)
        result << op_0;

    return result;
}

static cscript combinesignatures(const cscript& scriptpubkey, const basesignaturechecker& checker,
                                 const txnouttype txtype, const vector<valtype>& vsolutions,
                                 vector<valtype>& sigs1, vector<valtype>& sigs2)
{
    switch (txtype)
    {
    case tx_nonstandard:
    case tx_null_data:
        // don't know anything about this, assume bigger one is correct:
        if (sigs1.size() >= sigs2.size())
            return pushall(sigs1);
        return pushall(sigs2);
    case tx_pubkey:
    case tx_pubkeyhash:
        // signatures are bigger than placeholders or empty scripts:
        if (sigs1.empty() || sigs1[0].empty())
            return pushall(sigs2);
        return pushall(sigs1);
    case tx_scripthash:
        if (sigs1.empty() || sigs1.back().empty())
            return pushall(sigs2);
        else if (sigs2.empty() || sigs2.back().empty())
            return pushall(sigs1);
        else
        {
            // recur to combine:
            valtype spk = sigs1.back();
            cscript pubkey2(spk.begin(), spk.end());

            txnouttype txtype2;
            vector<vector<unsigned char> > vsolutions2;
            solver(pubkey2, txtype2, vsolutions2);
            sigs1.pop_back();
            sigs2.pop_back();
            cscript result = combinesignatures(pubkey2, checker, txtype2, vsolutions2, sigs1, sigs2);
            result << spk;
            return result;
        }
    case tx_multisig:
        return combinemultisig(scriptpubkey, checker, vsolutions, sigs1, sigs2);
    }

    return cscript();
}

cscript combinesignatures(const cscript& scriptpubkey, const ctransaction& txto, unsigned int nin,
                          const cscript& scriptsig1, const cscript& scriptsig2)
{
    transactionsignaturechecker checker(&txto, nin);
    return combinesignatures(scriptpubkey, checker, scriptsig1, scriptsig2);
}

cscript combinesignatures(const cscript& scriptpubkey, const basesignaturechecker& checker,
                          const cscript& scriptsig1, const cscript& scriptsig2)
{
    txnouttype txtype;
    vector<vector<unsigned char> > vsolutions;
    solver(scriptpubkey, txtype, vsolutions);

    vector<valtype> stack1;
    evalscript(stack1, scriptsig1, script_verify_strictenc, basesignaturechecker());
    vector<valtype> stack2;
    evalscript(stack2, scriptsig2, script_verify_strictenc, basesignaturechecker());

    return combinesignatures(scriptpubkey, checker, txtype, vsolutions, stack1, stack2);
}
