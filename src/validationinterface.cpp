// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "validationinterface.h"

static cmainsignals g_signals;

cmainsignals& getmainsignals()
{
    return g_signals;
}

void registervalidationinterface(cvalidationinterface* pwalletin) {
    g_signals.synctransaction.connect(boost::bind(&cvalidationinterface::synctransaction, pwalletin, _1, _2));
    g_signals.updatedtransaction.connect(boost::bind(&cvalidationinterface::updatedtransaction, pwalletin, _1));
    g_signals.setbestchain.connect(boost::bind(&cvalidationinterface::setbestchain, pwalletin, _1));
    g_signals.inventory.connect(boost::bind(&cvalidationinterface::inventory, pwalletin, _1));
    g_signals.broadcast.connect(boost::bind(&cvalidationinterface::resendwallettransactions, pwalletin, _1));
    g_signals.blockchecked.connect(boost::bind(&cvalidationinterface::blockchecked, pwalletin, _1, _2));
}

void unregistervalidationinterface(cvalidationinterface* pwalletin) {
    g_signals.blockchecked.disconnect(boost::bind(&cvalidationinterface::blockchecked, pwalletin, _1, _2));
    g_signals.broadcast.disconnect(boost::bind(&cvalidationinterface::resendwallettransactions, pwalletin, _1));
    g_signals.inventory.disconnect(boost::bind(&cvalidationinterface::inventory, pwalletin, _1));
    g_signals.setbestchain.disconnect(boost::bind(&cvalidationinterface::setbestchain, pwalletin, _1));
    g_signals.updatedtransaction.disconnect(boost::bind(&cvalidationinterface::updatedtransaction, pwalletin, _1));
    g_signals.synctransaction.disconnect(boost::bind(&cvalidationinterface::synctransaction, pwalletin, _1, _2));
}

void unregisterallvalidationinterfaces() {
    g_signals.blockchecked.disconnect_all_slots();
    g_signals.broadcast.disconnect_all_slots();
    g_signals.inventory.disconnect_all_slots();
    g_signals.setbestchain.disconnect_all_slots();
    g_signals.updatedtransaction.disconnect_all_slots();
    g_signals.synctransaction.disconnect_all_slots();
}

void syncwithwallets(const ctransaction &tx, const cblock *pblock) {
    g_signals.synctransaction(tx, pblock);
}
