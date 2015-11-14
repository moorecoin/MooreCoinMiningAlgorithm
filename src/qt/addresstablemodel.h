// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_addresstablemodel_h
#define moorecoin_qt_addresstablemodel_h

#include <qabstracttablemodel>
#include <qstringlist>

class addresstablepriv;
class walletmodel;

class cwallet;

/**
   qt model of the address book in the core. this allows views to access and modify the address book.
 */
class addresstablemodel : public qabstracttablemodel
{
    q_object

public:
    explicit addresstablemodel(cwallet *wallet, walletmodel *parent = 0);
    ~addresstablemodel();

    enum columnindex {
        label = 0,   /**< user specified label */
        address = 1  /**< moorecoin address */
    };

    enum roleindex {
        typerole = qt::userrole /**< type of address (#send or #receive) */
    };

    /** return status of edit/insert operation */
    enum editstatus {
        ok,                     /**< everything ok */
        no_changes,             /**< no changes were made during edit operation */
        invalid_address,        /**< unparseable address */
        duplicate_address,      /**< address already in address book */
        wallet_unlock_failure,  /**< wallet could not be unlocked to create new receiving address */
        key_generation_failure  /**< generating a new public key for a receiving address failed */
    };

    static const qstring send;      /**< specifies send address */
    static const qstring receive;   /**< specifies receive address */

    /** @name methods overridden from qabstracttablemodel
        @{*/
    int rowcount(const qmodelindex &parent) const;
    int columncount(const qmodelindex &parent) const;
    qvariant data(const qmodelindex &index, int role) const;
    bool setdata(const qmodelindex &index, const qvariant &value, int role);
    qvariant headerdata(int section, qt::orientation orientation, int role) const;
    qmodelindex index(int row, int column, const qmodelindex &parent) const;
    bool removerows(int row, int count, const qmodelindex &parent = qmodelindex());
    qt::itemflags flags(const qmodelindex &index) const;
    /*@}*/

    /* add an address to the model.
       returns the added address on success, and an empty string otherwise.
     */
    qstring addrow(const qstring &type, const qstring &label, const qstring &address);

    /* look up label for address in address book, if not found return empty string.
     */
    qstring labelforaddress(const qstring &address) const;

    /* look up row index of an address in the model.
       return -1 if not found.
     */
    int lookupaddress(const qstring &address) const;

    editstatus geteditstatus() const { return editstatus; }

private:
    walletmodel *walletmodel;
    cwallet *wallet;
    addresstablepriv *priv;
    qstringlist columns;
    editstatus editstatus;

    /** notify listeners that data changed. */
    void emitdatachanged(int index);

public slots:
    /* update address list from core.
     */
    void updateentry(const qstring &address, const qstring &label, bool ismine, const qstring &purpose, int status);

    friend class addresstablepriv;
};

#endif // moorecoin_qt_addresstablemodel_h
