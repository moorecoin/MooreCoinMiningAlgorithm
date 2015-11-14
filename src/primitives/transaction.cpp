// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

std::string coutpoint::tostring() const
{
    return strprintf("coutpoint(%s, %u)", hash.tostring().substr(0,10), n);
}

ctxin::ctxin(coutpoint prevoutin, cscript scriptsigin, uint32_t nsequencein)
{
    prevout = prevoutin;
    scriptsig = scriptsigin;
    nsequence = nsequencein;
}

ctxin::ctxin(uint256 hashprevtx, uint32_t nout, cscript scriptsigin, uint32_t nsequencein)
{
    prevout = coutpoint(hashprevtx, nout);
    scriptsig = scriptsigin;
    nsequence = nsequencein;
}

std::string ctxin::tostring() const
{
    std::string str;
    str += "ctxin(";
    str += prevout.tostring();
    if (prevout.isnull())
        str += strprintf(", coinbase %s", hexstr(scriptsig));
    else
        str += strprintf(", scriptsig=%s", scriptsig.tostring().substr(0,24));
    if (nsequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nsequence=%u", nsequence);
    str += ")";
    return str;
}

ctxout::ctxout(const camount& nvaluein, cscript scriptpubkeyin)
{
    nvalue = nvaluein;
    scriptpubkey = scriptpubkeyin;
}

uint256 ctxout::gethash() const
{
    return serializehash(*this);
}

std::string ctxout::tostring() const
{
    return strprintf("ctxout(nvalue=%d.%08d, scriptpubkey=%s)", nvalue / coin, nvalue % coin, scriptpubkey.tostring().substr(0,30));
}

cmutabletransaction::cmutabletransaction() : nversion(ctransaction::current_version), nlocktime(0) {}
cmutabletransaction::cmutabletransaction(const ctransaction& tx) : nversion(tx.nversion), vin(tx.vin), vout(tx.vout), nlocktime(tx.nlocktime) {}

uint256 cmutabletransaction::gethash() const
{
    return serializehash(*this);
}

void ctransaction::updatehash() const
{
    *const_cast<uint256*>(&hash) = serializehash(*this);
}

ctransaction::ctransaction() : nversion(ctransaction::current_version), vin(), vout(), nlocktime(0) { }

ctransaction::ctransaction(const cmutabletransaction &tx) : nversion(tx.nversion), vin(tx.vin), vout(tx.vout), nlocktime(tx.nlocktime) {
    updatehash();
}

ctransaction& ctransaction::operator=(const ctransaction &tx) {
    *const_cast<int*>(&nversion) = tx.nversion;
    *const_cast<std::vector<ctxin>*>(&vin) = tx.vin;
    *const_cast<std::vector<ctxout>*>(&vout) = tx.vout;
    *const_cast<unsigned int*>(&nlocktime) = tx.nlocktime;
    *const_cast<uint256*>(&hash) = tx.hash;
    return *this;
}

camount ctransaction::getvalueout() const
{
    camount nvalueout = 0;
    for (std::vector<ctxout>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        nvalueout += it->nvalue;
        if (!moneyrange(it->nvalue) || !moneyrange(nvalueout))
            throw std::runtime_error("ctransaction::getvalueout(): value out of range");
    }
    return nvalueout;
}

double ctransaction::computepriority(double dpriorityinputs, unsigned int ntxsize) const
{
    ntxsize = calculatemodifiedsize(ntxsize);
    if (ntxsize == 0) return 0.0;

    return dpriorityinputs / ntxsize;
}

unsigned int ctransaction::calculatemodifiedsize(unsigned int ntxsize) const
{
    // in order to avoid disincentivizing cleaning up the utxo set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptsig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    if (ntxsize == 0)
        ntxsize = ::getserializesize(*this, ser_network, protocol_version);
    for (std::vector<ctxin>::const_iterator it(vin.begin()); it != vin.end(); ++it)
    {
        unsigned int offset = 41u + std::min(110u, (unsigned int)it->scriptsig.size());
        if (ntxsize > offset)
            ntxsize -= offset;
    }
    return ntxsize;
}

std::string ctransaction::tostring() const
{
    std::string str;
    str += strprintf("ctransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nlocktime=%u)\n",
        gethash().tostring().substr(0,10),
        nversion,
        vin.size(),
        vout.size(),
        nlocktime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].tostring() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].tostring() + "\n";
    return str;
}
