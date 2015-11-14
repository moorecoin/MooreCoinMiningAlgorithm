// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_overviewpage_h
#define moorecoin_qt_overviewpage_h

#include "amount.h"

#include <qwidget>

class clientmodel;
class transactionfilterproxy;
class txviewdelegate;
class walletmodel;

namespace ui {
    class overviewpage;
}

qt_begin_namespace
class qmodelindex;
qt_end_namespace

/** overview ("home") page widget */
class overviewpage : public qwidget
{
    q_object

public:
    explicit overviewpage(qwidget *parent = 0);
    ~overviewpage();

    void setclientmodel(clientmodel *clientmodel);
    void setwalletmodel(walletmodel *walletmodel);
    void showoutofsyncwarning(bool fshow);

public slots:
    void setbalance(const camount& balance, const camount& unconfirmedbalance, const camount& immaturebalance,
                    const camount& watchonlybalance, const camount& watchunconfbalance, const camount& watchimmaturebalance);

signals:
    void transactionclicked(const qmodelindex &index);

private:
    ui::overviewpage *ui;
    clientmodel *clientmodel;
    walletmodel *walletmodel;
    camount currentbalance;
    camount currentunconfirmedbalance;
    camount currentimmaturebalance;
    camount currentwatchonlybalance;
    camount currentwatchunconfbalance;
    camount currentwatchimmaturebalance;

    txviewdelegate *txdelegate;
    transactionfilterproxy *filter;

private slots:
    void updatedisplayunit();
    void handletransactionclicked(const qmodelindex &index);
    void updatealerts(const qstring &warnings);
    void updatewatchonlylabels(bool showwatchonly);
};

#endif // moorecoin_qt_overviewpage_h
