// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_clientmodel_h
#define moorecoin_qt_clientmodel_h

#include <qobject>
#include <qdatetime>

class addresstablemodel;
class optionsmodel;
class peertablemodel;
class transactiontablemodel;

class cwallet;

qt_begin_namespace
class qtimer;
qt_end_namespace

enum blocksource {
    block_source_none,
    block_source_reindex,
    block_source_disk,
    block_source_network
};

enum numconnections {
    connections_none = 0,
    connections_in   = (1u << 0),
    connections_out  = (1u << 1),
    connections_all  = (connections_in | connections_out),
};

/** model for moorecoin network client. */
class clientmodel : public qobject
{
    q_object

public:
    explicit clientmodel(optionsmodel *optionsmodel, qobject *parent = 0);
    ~clientmodel();

    optionsmodel *getoptionsmodel();
    peertablemodel *getpeertablemodel();

    //! return number of connections, default is in- and outbound (total)
    int getnumconnections(unsigned int flags = connections_all) const;
    int getnumblocks() const;

    quint64 gettotalbytesrecv() const;
    quint64 gettotalbytessent() const;

    double getverificationprogress() const;
    qdatetime getlastblockdate() const;

    //! return true if core is doing initial block download
    bool ininitialblockdownload() const;
    //! return true if core is importing blocks
    enum blocksource getblocksource() const;
    //! return warnings to be displayed in status bar
    qstring getstatusbarwarnings() const;

    qstring formatfullversion() const;
    qstring formatbuilddate() const;
    bool isreleaseversion() const;
    qstring clientname() const;
    qstring formatclientstartuptime() const;

private:
    optionsmodel *optionsmodel;
    peertablemodel *peertablemodel;

    int cachednumblocks;
    qdatetime cachedblockdate;
    bool cachedreindexing;
    bool cachedimporting;

    qtimer *polltimer;

    void subscribetocoresignals();
    void unsubscribefromcoresignals();

signals:
    void numconnectionschanged(int count);
    void numblockschanged(int count, const qdatetime& blockdate);
    void alertschanged(const qstring &warnings);
    void byteschanged(quint64 totalbytesin, quint64 totalbytesout);

    //! fired when a message should be reported to the user
    void message(const qstring &title, const qstring &message, unsigned int style);

    // show progress dialog e.g. for verifychain
    void showprogress(const qstring &title, int nprogress);

public slots:
    void updatetimer();
    void updatenumconnections(int numconnections);
    void updatealert(const qstring &hash, int status);
};

#endif // moorecoin_qt_clientmodel_h
