// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_peertablemodel_h
#define moorecoin_qt_peertablemodel_h

#include "main.h"
#include "net.h"

#include <qabstracttablemodel>
#include <qstringlist>

class clientmodel;
class peertablepriv;

qt_begin_namespace
class qtimer;
qt_end_namespace

struct cnodecombinedstats {
    cnodestats nodestats;
    cnodestatestats nodestatestats;
    bool fnodestatestatsavailable;
};

class nodelessthan
{
public:
    nodelessthan(int ncolumn, qt::sortorder forder) :
        column(ncolumn), order(forder) {}
    bool operator()(const cnodecombinedstats &left, const cnodecombinedstats &right) const;

private:
    int column;
    qt::sortorder order;
};

/**
   qt model providing information about connected peers, similar to the
   "getpeerinfo" rpc call. used by the rpc console ui.
 */
class peertablemodel : public qabstracttablemodel
{
    q_object

public:
    explicit peertablemodel(clientmodel *parent = 0);
    const cnodecombinedstats *getnodestats(int idx);
    int getrowbynodeid(nodeid nodeid);
    void startautorefresh();
    void stopautorefresh();

    enum columnindex {
        address = 0,
        subversion = 1,
        ping = 2
    };

    /** @name methods overridden from qabstracttablemodel
        @{*/
    int rowcount(const qmodelindex &parent) const;
    int columncount(const qmodelindex &parent) const;
    qvariant data(const qmodelindex &index, int role) const;
    qvariant headerdata(int section, qt::orientation orientation, int role) const;
    qmodelindex index(int row, int column, const qmodelindex &parent) const;
    qt::itemflags flags(const qmodelindex &index) const;
    void sort(int column, qt::sortorder order);
    /*@}*/

public slots:
    void refresh();

private:
    clientmodel *clientmodel;
    qstringlist columns;
    peertablepriv *priv;
    qtimer *timer;
};

#endif // moorecoin_qt_peertablemodel_h
