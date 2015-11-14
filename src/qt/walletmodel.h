// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_walletmodel_h
#define moorecoin_qt_walletmodel_h

#include "paymentrequestplus.h"
#include "walletmodeltransaction.h"

#include "support/allocators/secure.h"

#include <map>
#include <vector>

#include <qobject>

class addresstablemodel;
class optionsmodel;
class recentrequeststablemodel;
class transactiontablemodel;
class walletmodeltransaction;

class ccoincontrol;
class ckeyid;
class coutpoint;
class coutput;
class cpubkey;
class cwallet;
class uint256;

qt_begin_namespace
class qtimer;
qt_end_namespace

class sendcoinsrecipient
{
public:
    explicit sendcoinsrecipient() : amount(0), fsubtractfeefromamount(false), nversion(sendcoinsrecipient::current_version) { }
    explicit sendcoinsrecipient(const qstring &addr, const qstring &label, const camount& amount, const qstring &message):
        address(addr), label(label), amount(amount), message(message), fsubtractfeefromamount(false), nversion(sendcoinsrecipient::current_version) {}

    // if from an unauthenticated payment request, this is used for storing
    // the addresses, e.g. address-a<br />address-b<br />address-c.
    // info: as we don't need to process addresses in here when using
    // payment requests, we can abuse it for displaying an address list.
    // todo: this is a hack, should be replaced with a cleaner solution!
    qstring address;
    qstring label;
    camount amount;
    // if from a payment request, this is used for storing the memo
    qstring message;

    // if from a payment request, paymentrequest.isinitialized() will be true
    paymentrequestplus paymentrequest;
    // empty if no authentication or invalid signature/cert/etc.
    qstring authenticatedmerchant;

    bool fsubtractfeefromamount; // memory only

    static const int current_version = 1;
    int nversion;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        std::string saddress = address.tostdstring();
        std::string slabel = label.tostdstring();
        std::string smessage = message.tostdstring();
        std::string spaymentrequest;
        if (!ser_action.forread() && paymentrequest.isinitialized())
            paymentrequest.serializetostring(&spaymentrequest);
        std::string sauthenticatedmerchant = authenticatedmerchant.tostdstring();

        readwrite(this->nversion);
        nversion = this->nversion;
        readwrite(saddress);
        readwrite(slabel);
        readwrite(amount);
        readwrite(smessage);
        readwrite(spaymentrequest);
        readwrite(sauthenticatedmerchant);

        if (ser_action.forread())
        {
            address = qstring::fromstdstring(saddress);
            label = qstring::fromstdstring(slabel);
            message = qstring::fromstdstring(smessage);
            if (!spaymentrequest.empty())
                paymentrequest.parse(qbytearray::fromrawdata(spaymentrequest.data(), spaymentrequest.size()));
            authenticatedmerchant = qstring::fromstdstring(sauthenticatedmerchant);
        }
    }
};

/** interface to moorecoin wallet from qt view code. */
class walletmodel : public qobject
{
    q_object

public:
    explicit walletmodel(cwallet *wallet, optionsmodel *optionsmodel, qobject *parent = 0);
    ~walletmodel();

    enum statuscode // returned by sendcoins
    {
        ok,
        invalidamount,
        invalidaddress,
        amountexceedsbalance,
        amountwithfeeexceedsbalance,
        duplicateaddress,
        transactioncreationfailed, // error returned when wallet is still locked
        transactioncommitfailed,
        absurdfee,
        paymentrequestexpired
    };

    enum encryptionstatus
    {
        unencrypted,  // !wallet->iscrypted()
        locked,       // wallet->iscrypted() && wallet->islocked()
        unlocked      // wallet->iscrypted() && !wallet->islocked()
    };

    optionsmodel *getoptionsmodel();
    addresstablemodel *getaddresstablemodel();
    transactiontablemodel *gettransactiontablemodel();
    recentrequeststablemodel *getrecentrequeststablemodel();

    camount getbalance(const ccoincontrol *coincontrol = null) const;
    camount getunconfirmedbalance() const;
    camount getimmaturebalance() const;
    bool havewatchonly() const;
    camount getwatchbalance() const;
    camount getwatchunconfirmedbalance() const;
    camount getwatchimmaturebalance() const;
    encryptionstatus getencryptionstatus() const;

    // check address for validity
    bool validateaddress(const qstring &address);

    // return status record for sendcoins, contains error id + information
    struct sendcoinsreturn
    {
        sendcoinsreturn(statuscode status = ok):
            status(status) {}
        statuscode status;
    };

    // prepare transaction for getting txfee before sending coins
    sendcoinsreturn preparetransaction(walletmodeltransaction &transaction, const ccoincontrol *coincontrol = null);

    // send coins to a list of recipients
    sendcoinsreturn sendcoins(walletmodeltransaction &transaction);

    // wallet encryption
    bool setwalletencrypted(bool encrypted, const securestring &passphrase);
    // passphrase only needed when unlocking
    bool setwalletlocked(bool locked, const securestring &passphrase=securestring());
    bool changepassphrase(const securestring &oldpass, const securestring &newpass);
    // wallet backup
    bool backupwallet(const qstring &filename);

    // rai object for unlocking wallet, returned by requestunlock()
    class unlockcontext
    {
    public:
        unlockcontext(walletmodel *wallet, bool valid, bool relock);
        ~unlockcontext();

        bool isvalid() const { return valid; }

        // copy operator and constructor transfer the context
        unlockcontext(const unlockcontext& obj) { copyfrom(obj); }
        unlockcontext& operator=(const unlockcontext& rhs) { copyfrom(rhs); return *this; }
    private:
        walletmodel *wallet;
        bool valid;
        mutable bool relock; // mutable, as it can be set to false by copying

        void copyfrom(const unlockcontext& rhs);
    };

    unlockcontext requestunlock();

    bool getpubkey(const ckeyid &address, cpubkey& vchpubkeyout) const;
    void getoutputs(const std::vector<coutpoint>& voutpoints, std::vector<coutput>& voutputs);
    bool isspent(const coutpoint& outpoint) const;
    void listcoins(std::map<qstring, std::vector<coutput> >& mapcoins) const;

    bool islockedcoin(uint256 hash, unsigned int n) const;
    void lockcoin(coutpoint& output);
    void unlockcoin(coutpoint& output);
    void listlockedcoins(std::vector<coutpoint>& voutpts);

    void loadreceiverequests(std::vector<std::string>& vreceiverequests);
    bool savereceiverequest(const std::string &saddress, const int64_t nid, const std::string &srequest);

private:
    cwallet *wallet;
    bool fhavewatchonly;
    bool fforcecheckbalancechanged;

    // wallet has an options model for wallet-specific options
    // (transaction fee, for example)
    optionsmodel *optionsmodel;

    addresstablemodel *addresstablemodel;
    transactiontablemodel *transactiontablemodel;
    recentrequeststablemodel *recentrequeststablemodel;

    // cache some values to be able to detect changes
    camount cachedbalance;
    camount cachedunconfirmedbalance;
    camount cachedimmaturebalance;
    camount cachedwatchonlybalance;
    camount cachedwatchunconfbalance;
    camount cachedwatchimmaturebalance;
    encryptionstatus cachedencryptionstatus;
    int cachednumblocks;

    qtimer *polltimer;

    void subscribetocoresignals();
    void unsubscribefromcoresignals();
    void checkbalancechanged();

signals:
    // signal that balance in wallet changed
    void balancechanged(const camount& balance, const camount& unconfirmedbalance, const camount& immaturebalance,
                        const camount& watchonlybalance, const camount& watchunconfbalance, const camount& watchimmaturebalance);

    // encryption status of wallet changed
    void encryptionstatuschanged(int status);

    // signal emitted when wallet needs to be unlocked
    // it is valid behaviour for listeners to keep the wallet locked after this signal;
    // this means that the unlocking failed or was cancelled.
    void requireunlock();

    // fired when a message should be reported to the user
    void message(const qstring &title, const qstring &message, unsigned int style);

    // coins sent: from wallet, to recipient, in (serialized) transaction:
    void coinssent(cwallet* wallet, sendcoinsrecipient recipient, qbytearray transaction);

    // show progress dialog e.g. for rescan
    void showprogress(const qstring &title, int nprogress);

    // watch-only address added
    void notifywatchonlychanged(bool fhavewatchonly);

public slots:
    /* wallet status might have changed */
    void updatestatus();
    /* new transaction, or transaction changed status */
    void updatetransaction();
    /* new, updated or removed address book entry */
    void updateaddressbook(const qstring &address, const qstring &label, bool ismine, const qstring &purpose, int status);
    /* watch-only added */
    void updatewatchonlyflag(bool fhavewatchonly);
    /* current, immature or unconfirmed balance might have changed - emit 'balancechanged' if so */
    void pollbalancechanged();
};

#endif // moorecoin_qt_walletmodel_h
