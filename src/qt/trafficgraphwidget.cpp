// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "trafficgraphwidget.h"
#include "clientmodel.h"

#include <qpainter>
#include <qcolor>
#include <qtimer>

#include <cmath>

#define desired_samples         800

#define xmargin                 10
#define ymargin                 10

trafficgraphwidget::trafficgraphwidget(qwidget *parent) :
    qwidget(parent),
    timer(0),
    fmax(0.0f),
    nmins(0),
    vsamplesin(),
    vsamplesout(),
    nlastbytesin(0),
    nlastbytesout(0),
    clientmodel(0)
{
    timer = new qtimer(this);
    connect(timer, signal(timeout()), slot(updaterates()));
}

void trafficgraphwidget::setclientmodel(clientmodel *model)
{
    clientmodel = model;
    if(model) {
        nlastbytesin = model->gettotalbytesrecv();
        nlastbytesout = model->gettotalbytessent();
    }
}

int trafficgraphwidget::getgraphrangemins() const
{
    return nmins;
}

void trafficgraphwidget::paintpath(qpainterpath &path, qqueue<float> &samples)
{
    int h = height() - ymargin * 2, w = width() - xmargin * 2;
    int samplecount = samples.size(), x = xmargin + w, y;
    if(samplecount > 0) {
        path.moveto(x, ymargin + h);
        for(int i = 0; i < samplecount; ++i) {
            x = xmargin + w - w * i / desired_samples;
            y = ymargin + h - (int)(h * samples.at(i) / fmax);
            path.lineto(x, y);
        }
        path.lineto(x, ymargin + h);
    }
}

void trafficgraphwidget::paintevent(qpaintevent *)
{
    qpainter painter(this);
    painter.fillrect(rect(), qt::black);

    if(fmax <= 0.0f) return;

    qcolor axiscol(qt::gray);
    int h = height() - ymargin * 2;
    painter.setpen(axiscol);
    painter.drawline(xmargin, ymargin + h, width() - xmargin, ymargin + h);

    // decide what order of magnitude we are
    int base = floor(log10(fmax));
    float val = pow(10.0f, base);

    const qstring units     = tr("kb/s");
    const float ymargintext = 2.0;
    
    // draw lines
    painter.setpen(axiscol);
    painter.drawtext(xmargin, ymargin + h - h * val / fmax-ymargintext, qstring("%1 %2").arg(val).arg(units));
    for(float y = val; y < fmax; y += val) {
        int yy = ymargin + h - h * y / fmax;
        painter.drawline(xmargin, yy, width() - xmargin, yy);
    }
    // if we drew 3 or fewer lines, break them up at the next lower order of magnitude
    if(fmax / val <= 3.0f) {
        axiscol = axiscol.darker();
        val = pow(10.0f, base - 1);
        painter.setpen(axiscol);
        painter.drawtext(xmargin, ymargin + h - h * val / fmax-ymargintext, qstring("%1 %2").arg(val).arg(units));
        int count = 1;
        for(float y = val; y < fmax; y += val, count++) {
            // don't overwrite lines drawn above
            if(count % 10 == 0)
                continue;
            int yy = ymargin + h - h * y / fmax;
            painter.drawline(xmargin, yy, width() - xmargin, yy);
        }
    }

    if(!vsamplesin.empty()) {
        qpainterpath p;
        paintpath(p, vsamplesin);
        painter.fillpath(p, qcolor(0, 255, 0, 128));
        painter.setpen(qt::green);
        painter.drawpath(p);
    }
    if(!vsamplesout.empty()) {
        qpainterpath p;
        paintpath(p, vsamplesout);
        painter.fillpath(p, qcolor(255, 0, 0, 128));
        painter.setpen(qt::red);
        painter.drawpath(p);
    }
}

void trafficgraphwidget::updaterates()
{
    if(!clientmodel) return;

    quint64 bytesin = clientmodel->gettotalbytesrecv(),
            bytesout = clientmodel->gettotalbytessent();
    float inrate = (bytesin - nlastbytesin) / 1024.0f * 1000 / timer->interval();
    float outrate = (bytesout - nlastbytesout) / 1024.0f * 1000 / timer->interval();
    vsamplesin.push_front(inrate);
    vsamplesout.push_front(outrate);
    nlastbytesin = bytesin;
    nlastbytesout = bytesout;

    while(vsamplesin.size() > desired_samples) {
        vsamplesin.pop_back();
    }
    while(vsamplesout.size() > desired_samples) {
        vsamplesout.pop_back();
    }

    float tmax = 0.0f;
    foreach(float f, vsamplesin) {
        if(f > tmax) tmax = f;
    }
    foreach(float f, vsamplesout) {
        if(f > tmax) tmax = f;
    }
    fmax = tmax;
    update();
}

void trafficgraphwidget::setgraphrangemins(int mins)
{
    nmins = mins;
    int msecspersample = nmins * 60 * 1000 / desired_samples;
    timer->stop();
    timer->setinterval(msecspersample);

    clear();
}

void trafficgraphwidget::clear()
{
    timer->stop();

    vsamplesout.clear();
    vsamplesin.clear();
    fmax = 0.0f;

    if(clientmodel) {
        nlastbytesin = clientmodel->gettotalbytesrecv();
        nlastbytesout = clientmodel->gettotalbytessent();
    }
    timer->start();
}
