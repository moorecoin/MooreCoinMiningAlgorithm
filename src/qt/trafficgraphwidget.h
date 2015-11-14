// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_trafficgraphwidget_h
#define moorecoin_qt_trafficgraphwidget_h

#include <qwidget>
#include <qqueue>

class clientmodel;

qt_begin_namespace
class qpaintevent;
class qtimer;
qt_end_namespace

class trafficgraphwidget : public qwidget
{
    q_object

public:
    explicit trafficgraphwidget(qwidget *parent = 0);
    void setclientmodel(clientmodel *model);
    int getgraphrangemins() const;

protected:
    void paintevent(qpaintevent *);

public slots:
    void updaterates();
    void setgraphrangemins(int mins);
    void clear();

private:
    void paintpath(qpainterpath &path, qqueue<float> &samples);

    qtimer *timer;
    float fmax;
    int nmins;
    qqueue<float> vsamplesin;
    qqueue<float> vsamplesout;
    quint64 nlastbytesin;
    quint64 nlastbytesout;
    clientmodel *clientmodel;
};

#endif // moorecoin_qt_trafficgraphwidget_h
