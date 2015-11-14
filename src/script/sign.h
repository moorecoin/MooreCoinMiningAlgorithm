// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_script_sign_h
#define moorecoin_script_sign_h

#include "script/interpreter.h"

class ckeyid;
class ckeystore;
class cscript;
class ctransaction;

struct cmutabletransaction;

/** virtual base class for signature creators. */
class basesignaturecreator {
protected:
    const ckeystore* keystore;

public:
    basesignaturecreator(const ckeystore* keystorein) : keystore(keystorein) {}
    const ckeystore& keystore() const { return *keystore; };
    virtual ~basesignaturecreator() {}
    virtual const basesignaturechecker& checker() const =0;

    /** create a singular (non-script) signature. */
    virtual bool createsig(std::vector<unsigned char>& vchsig, const ckeyid& keyid, const cscript& scriptcode) const =0;
};

/** a signature creator for transactions. */
class transactionsignaturecreator : public basesignaturecreator {
    const ctransaction* txto;
    unsigned int nin;
    int nhashtype;
    const transactionsignaturechecker checker;

public:
    transactionsignaturecreator(const ckeystore* keystorein, const ctransaction* txtoin, unsigned int ninin, int nhashtypein=sighash_all);
    const basesignaturechecker& checker() const { return checker; }
    bool createsig(std::vector<unsigned char>& vchsig, const ckeyid& keyid, const cscript& scriptcode) const;
};

/** produce a script signature using a generic signature creator. */
bool producesignature(const basesignaturecreator& creator, const cscript& scriptpubkey, cscript& scriptsig);

/** produce a script signature for a transaction. */
bool signsignature(const ckeystore& keystore, const cscript& frompubkey, cmutabletransaction& txto, unsigned int nin, int nhashtype=sighash_all);
bool signsignature(const ckeystore& keystore, const ctransaction& txfrom, cmutabletransaction& txto, unsigned int nin, int nhashtype=sighash_all);

/** combine two script signatures using a generic signature checker, intelligently, possibly with op_0 placeholders. */
cscript combinesignatures(const cscript& scriptpubkey, const basesignaturechecker& checker, const cscript& scriptsig1, const cscript& scriptsig2);

/** combine two script signatures on transactions. */
cscript combinesignatures(const cscript& scriptpubkey, const ctransaction& txto, unsigned int nin, const cscript& scriptsig1, const cscript& scriptsig2);

#endif // moorecoin_script_sign_h
