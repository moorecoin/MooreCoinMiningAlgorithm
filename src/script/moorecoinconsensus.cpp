// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "moorecoinconsensus.h"

#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "version.h"

namespace {

/** a class that deserializes a single ctransaction one time. */
class txinputstream
{
public:
    txinputstream(int ntypein, int nversionin, const unsigned char *txto, size_t txtolen) :
    m_type(ntypein),
    m_version(nversionin),
    m_data(txto),
    m_remaining(txtolen)
    {}

    txinputstream& read(char* pch, size_t nsize)
    {
        if (nsize > m_remaining)
            throw std::ios_base::failure(std::string(__func__) + ": end of data");

        if (pch == null)
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");

        if (m_data == null)
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");

        memcpy(pch, m_data, nsize);
        m_remaining -= nsize;
        m_data += nsize;
        return *this;
    }

    template<typename t>
    txinputstream& operator>>(t& obj)
    {
        ::unserialize(*this, obj, m_type, m_version);
        return *this;
    }

private:
    const int m_type;
    const int m_version;
    const unsigned char* m_data;
    size_t m_remaining;
};

inline int set_error(moorecoinconsensus_error* ret, moorecoinconsensus_error serror)
{
    if (ret)
        *ret = serror;
    return 0;
}

} // anon namespace

int moorecoinconsensus_verify_script(const unsigned char *scriptpubkey, unsigned int scriptpubkeylen,
                                    const unsigned char *txto        , unsigned int txtolen,
                                    unsigned int nin, unsigned int flags, moorecoinconsensus_error* err)
{
    try {
        txinputstream stream(ser_network, protocol_version, txto, txtolen);
        ctransaction tx;
        stream >> tx;
        if (nin >= tx.vin.size())
            return set_error(err, moorecoinconsensus_err_tx_index);
        if (tx.getserializesize(ser_network, protocol_version) != txtolen)
            return set_error(err, moorecoinconsensus_err_tx_size_mismatch);

         // regardless of the verification result, the tx did not error.
         set_error(err, moorecoinconsensus_err_ok);

        return verifyscript(tx.vin[nin].scriptsig, cscript(scriptpubkey, scriptpubkey + scriptpubkeylen), flags, transactionsignaturechecker(&tx, nin), null);
    } catch (const std::exception&) {
        return set_error(err, moorecoinconsensus_err_tx_deserialize); // error deserializing
    }
}

unsigned int moorecoinconsensus_version()
{
    // just use the api version for now
    return moorecoinconsensus_api_ver;
}
