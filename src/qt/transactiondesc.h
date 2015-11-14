// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_transactiondesc_h
#define moorecoin_qt_transactiondesc_h

#include <qobject>
#include <qstring>

class transactionrecord;

class cwallet;
class cwallettx;

/** provide a human-readable extended html description of a transaction.
 */
class transactiondesc: public qobject
{
    q_object

public:
    static qstring tohtml(cwallet *wallet, cwallettx &wtx, transactionrecord *rec, int unit);

private:
    transactiondesc() {}

    static qstring formattxstatus(const cwallettx& wtx);
};

#endif // moorecoin_qt_transactiondesc_h
