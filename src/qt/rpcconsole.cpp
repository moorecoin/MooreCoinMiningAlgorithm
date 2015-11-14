// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcconsole.h"
#include "ui_rpcconsole.h"

#include "clientmodel.h"
#include "guiutil.h"
#include "peertablemodel.h"
#include "scicon.h"

#include "main.h"
#include "chainparams.h"
#include "rpcserver.h"
#include "rpcclient.h"
#include "util.h"

#include <openssl/crypto.h>

#include "univalue/univalue.h"

#ifdef enable_wallet
#include <db_cxx.h>
#endif

#include <qkeyevent>
#include <qmenu>
#include <qscrollbar>
#include <qthread>
#include <qtime>

#if qt_version < 0x050000
#include <qurl>
#endif

// todo: add a scrollback limit, as there is currently none
// todo: make it possible to filter out categories (esp debug messages when implemented)
// todo: receive errors and debug messages through clientmodel

const int console_history = 50;
const qsize icon_size(24, 24);

const int initial_traffic_graph_mins = 30;

const struct {
    const char *url;
    const char *source;
} icon_mapping[] = {
    {"cmd-request", ":/icons/tx_input"},
    {"cmd-reply", ":/icons/tx_output"},
    {"cmd-error", ":/icons/tx_output"},
    {"misc", ":/icons/tx_inout"},
    {null, null}
};

/* object for executing console rpc commands in a separate thread.
*/
class rpcexecutor : public qobject
{
    q_object

public slots:
    void request(const qstring &command);

signals:
    void reply(int category, const qstring &command);
};

#include "rpcconsole.moc"

/**
 * split shell command line into a list of arguments. aims to emulate \c bash and friends.
 *
 * - arguments are delimited with whitespace
 * - extra whitespace at the beginning and end and between arguments will be ignored
 * - text can be "double" or 'single' quoted
 * - the backslash \c \ is used as escape character
 *   - outside quotes, any character can be escaped
 *   - within double quotes, only escape \c " and backslashes before a \c " or another backslash
 *   - within single quotes, no escaping is possible and no special interpretation takes place
 *
 * @param[out]   args        parsed arguments will be appended to this list
 * @param[in]    strcommand  command line to split
 */
bool parsecommandline(std::vector<std::string> &args, const std::string &strcommand)
{
    enum cmdparsestate
    {
        state_eating_spaces,
        state_argument,
        state_singlequoted,
        state_doublequoted,
        state_escape_outer,
        state_escape_doublequoted
    } state = state_eating_spaces;
    std::string curarg;
    foreach(char ch, strcommand)
    {
        switch(state)
        {
        case state_argument: // in or after argument
        case state_eating_spaces: // handle runs of whitespace
            switch(ch)
            {
            case '"': state = state_doublequoted; break;
            case '\'': state = state_singlequoted; break;
            case '\\': state = state_escape_outer; break;
            case ' ': case '\n': case '\t':
                if(state == state_argument) // space ends argument
                {
                    args.push_back(curarg);
                    curarg.clear();
                }
                state = state_eating_spaces;
                break;
            default: curarg += ch; state = state_argument;
            }
            break;
        case state_singlequoted: // single-quoted string
            switch(ch)
            {
            case '\'': state = state_argument; break;
            default: curarg += ch;
            }
            break;
        case state_doublequoted: // double-quoted string
            switch(ch)
            {
            case '"': state = state_argument; break;
            case '\\': state = state_escape_doublequoted; break;
            default: curarg += ch;
            }
            break;
        case state_escape_outer: // '\' outside quotes
            curarg += ch; state = state_argument;
            break;
        case state_escape_doublequoted: // '\' in double-quoted text
            if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
            curarg += ch; state = state_doublequoted;
            break;
        }
    }
    switch(state) // final state
    {
    case state_eating_spaces:
        return true;
    case state_argument:
        args.push_back(curarg);
        return true;
    default: // error to end in one of the other states
        return false;
    }
}

void rpcexecutor::request(const qstring &command)
{
    std::vector<std::string> args;
    if(!parsecommandline(args, command.tostdstring()))
    {
        emit reply(rpcconsole::cmd_error, qstring("parse error: unbalanced ' or \""));
        return;
    }
    if(args.empty())
        return; // nothing to do
    try
    {
        std::string strprint;
        // convert argument list to json objects in method-dependent way,
        // and pass it along with the method name to the dispatcher.
        univalue result = tablerpc.execute(
            args[0],
            rpcconvertvalues(args[0], std::vector<std::string>(args.begin() + 1, args.end())));

        // format result reply
        if (result.isnull())
            strprint = "";
        else if (result.isstr())
            strprint = result.get_str();
        else
            strprint = result.write(2);

        emit reply(rpcconsole::cmd_reply, qstring::fromstdstring(strprint));
    }
    catch (univalue& objerror)
    {
        try // nice formatting for standard-format error
        {
            int code = find_value(objerror, "code").get_int();
            std::string message = find_value(objerror, "message").get_str();
            emit reply(rpcconsole::cmd_error, qstring::fromstdstring(message) + " (code " + qstring::number(code) + ")");
        }
        catch (const std::runtime_error&) // raised when converting to invalid type, i.e. missing code or message
        {   // show raw json object
            emit reply(rpcconsole::cmd_error, qstring::fromstdstring(objerror.write()));
        }
    }
    catch (const std::exception& e)
    {
        emit reply(rpcconsole::cmd_error, qstring("error: ") + qstring::fromstdstring(e.what()));
    }
}

rpcconsole::rpcconsole(qwidget *parent) :
    qwidget(parent),
    ui(new ui::rpcconsole),
    clientmodel(0),
    historyptr(0),
    cachednodeid(-1),
    contextmenu(0)
{
    ui->setupui(this);
    guiutil::restorewindowgeometry("nrpcconsolewindow", this->size(), this);

#ifndef q_os_mac
    ui->opendebuglogfilebutton->seticon(singlecoloricon(":/icons/export"));
#endif
    ui->clearbutton->seticon(singlecoloricon(":/icons/remove"));

    // install event filter for up and down arrow
    ui->lineedit->installeventfilter(this);
    ui->messageswidget->installeventfilter(this);

    connect(ui->clearbutton, signal(clicked()), this, slot(clear()));
    connect(ui->btncleartrafficgraph, signal(clicked()), ui->trafficgraph, slot(clear()));

    // set library version labels
    ui->opensslversion->settext(ssleay_version(ssleay_version));
#ifdef enable_wallet
    ui->berkeleydbversion->settext(dbenv::version(0, 0, 0));
#else
    ui->label_berkeleydbversion->hide();
    ui->berkeleydbversion->hide();
#endif

    startexecutor();
    settrafficgraphrange(initial_traffic_graph_mins);

    ui->detailwidget->hide();
    ui->peerheading->settext(tr("select a peer to view detailed information."));

    clear();
}

rpcconsole::~rpcconsole()
{
    guiutil::savewindowgeometry("nrpcconsolewindow", this);
    emit stopexecutor();
    delete ui;
}

bool rpcconsole::eventfilter(qobject* obj, qevent *event)
{
    if(event->type() == qevent::keypress) // special key handling
    {
        qkeyevent *keyevt = static_cast<qkeyevent*>(event);
        int key = keyevt->key();
        qt::keyboardmodifiers mod = keyevt->modifiers();
        switch(key)
        {
        case qt::key_up: if(obj == ui->lineedit) { browsehistory(-1); return true; } break;
        case qt::key_down: if(obj == ui->lineedit) { browsehistory(1); return true; } break;
        case qt::key_pageup: /* pass paging keys to messages widget */
        case qt::key_pagedown:
            if(obj == ui->lineedit)
            {
                qapplication::postevent(ui->messageswidget, new qkeyevent(*keyevt));
                return true;
            }
            break;
        default:
            // typing in messages widget brings focus to line edit, and redirects key there
            // exclude most combinations and keys that emit no text, except paste shortcuts
            if(obj == ui->messageswidget && (
                  (!mod && !keyevt->text().isempty() && key != qt::key_tab) ||
                  ((mod & qt::controlmodifier) && key == qt::key_v) ||
                  ((mod & qt::shiftmodifier) && key == qt::key_insert)))
            {
                ui->lineedit->setfocus();
                qapplication::postevent(ui->lineedit, new qkeyevent(*keyevt));
                return true;
            }
        }
    }
    return qwidget::eventfilter(obj, event);
}

void rpcconsole::setclientmodel(clientmodel *model)
{
    clientmodel = model;
    ui->trafficgraph->setclientmodel(model);
    if(model)
    {
        // keep up to date with client
        setnumconnections(model->getnumconnections());
        connect(model, signal(numconnectionschanged(int)), this, slot(setnumconnections(int)));

        setnumblocks(model->getnumblocks(), model->getlastblockdate());
        connect(model, signal(numblockschanged(int,qdatetime)), this, slot(setnumblocks(int,qdatetime)));

        updatetrafficstats(model->gettotalbytesrecv(), model->gettotalbytessent());
        connect(model, signal(byteschanged(quint64,quint64)), this, slot(updatetrafficstats(quint64, quint64)));

        // set up peer table
        ui->peerwidget->setmodel(model->getpeertablemodel());
        ui->peerwidget->verticalheader()->hide();
        ui->peerwidget->setedittriggers(qabstractitemview::noedittriggers);
        ui->peerwidget->setselectionbehavior(qabstractitemview::selectrows);
        ui->peerwidget->setselectionmode(qabstractitemview::singleselection);
        ui->peerwidget->setcontextmenupolicy(qt::customcontextmenu);
        ui->peerwidget->setcolumnwidth(peertablemodel::address, address_column_width);
        ui->peerwidget->setcolumnwidth(peertablemodel::subversion, subversion_column_width);
        ui->peerwidget->setcolumnwidth(peertablemodel::ping, ping_column_width);

        // create context menu actions
        qaction* disconnectaction = new qaction(tr("&disconnect node"), this);

        // create context menu
        contextmenu = new qmenu();
        contextmenu->addaction(disconnectaction);

        // context menu signals
        connect(ui->peerwidget, signal(customcontextmenurequested(const qpoint&)), this, slot(showmenu(const qpoint&)));
        connect(disconnectaction, signal(triggered()), this, slot(disconnectselectednode()));

        // connect the peerwidget selection model to our peerselected() handler
        connect(ui->peerwidget->selectionmodel(), signal(selectionchanged(const qitemselection &, const qitemselection &)),
             this, slot(peerselected(const qitemselection &, const qitemselection &)));
        connect(model->getpeertablemodel(), signal(layoutchanged()), this, slot(peerlayoutchanged()));

        // provide initial values
        ui->clientversion->settext(model->formatfullversion());
        ui->clientname->settext(model->clientname());
        ui->builddate->settext(model->formatbuilddate());
        ui->startuptime->settext(model->formatclientstartuptime());

        ui->networkname->settext(qstring::fromstdstring(params().networkidstring()));
    }
}

static qstring categoryclass(int category)
{
    switch(category)
    {
    case rpcconsole::cmd_request:  return "cmd-request"; break;
    case rpcconsole::cmd_reply:    return "cmd-reply"; break;
    case rpcconsole::cmd_error:    return "cmd-error"; break;
    default:                       return "misc";
    }
}

void rpcconsole::clear()
{
    ui->messageswidget->clear();
    history.clear();
    historyptr = 0;
    ui->lineedit->clear();
    ui->lineedit->setfocus();

    // add smoothly scaled icon images.
    // (when using width/height on an img, qt uses nearest instead of linear interpolation)
    for(int i=0; icon_mapping[i].url; ++i)
    {
        ui->messageswidget->document()->addresource(
                    qtextdocument::imageresource,
                    qurl(icon_mapping[i].url),
                    singlecolorimage(icon_mapping[i].source, singlecolor()).scaled(icon_size, qt::ignoreaspectratio, qt::smoothtransformation));
    }

    // set default style sheet
    ui->messageswidget->document()->setdefaultstylesheet(
                "table { }"
                "td.time { color: #808080; padding-top: 3px; } "
                "td.cmd-request { color: #006060; } "
                "td.cmd-error { color: red; } "
                "b { color: #006060; } "
                );

    message(cmd_reply, (tr("welcome to the moorecoin core rpc console.") + "<br>" +
                        tr("use up and down arrows to navigate history, and <b>ctrl-l</b> to clear screen.") + "<br>" +
                        tr("type <b>help</b> for an overview of available commands.")), true);
}

void rpcconsole::keypressevent(qkeyevent *event)
{
    if(windowtype() != qt::widget && event->key() == qt::key_escape)
    {
        close();
    }
}

void rpcconsole::message(int category, const qstring &message, bool html)
{
    qtime time = qtime::currenttime();
    qstring timestring = time.tostring();
    qstring out;
    out += "<table><tr><td class=\"time\" width=\"65\">" + timestring + "</td>";
    out += "<td class=\"icon\" width=\"32\"><img src=\"" + categoryclass(category) + "\"></td>";
    out += "<td class=\"message " + categoryclass(category) + "\" valign=\"middle\">";
    if(html)
        out += message;
    else
        out += guiutil::htmlescape(message, true);
    out += "</td></tr></table>";
    ui->messageswidget->append(out);
}

void rpcconsole::setnumconnections(int count)
{
    if (!clientmodel)
        return;

    qstring connections = qstring::number(count) + " (";
    connections += tr("in:") + " " + qstring::number(clientmodel->getnumconnections(connections_in)) + " / ";
    connections += tr("out:") + " " + qstring::number(clientmodel->getnumconnections(connections_out)) + ")";

    ui->numberofconnections->settext(connections);
}

void rpcconsole::setnumblocks(int count, const qdatetime& blockdate)
{
    ui->numberofblocks->settext(qstring::number(count));
    ui->lastblocktime->settext(blockdate.tostring());
}

void rpcconsole::on_lineedit_returnpressed()
{
    qstring cmd = ui->lineedit->text();
    ui->lineedit->clear();

    if(!cmd.isempty())
    {
        message(cmd_request, cmd);
        emit cmdrequest(cmd);
        // remove command, if already in history
        history.removeone(cmd);
        // append command to history
        history.append(cmd);
        // enforce maximum history size
        while(history.size() > console_history)
            history.removefirst();
        // set pointer to end of history
        historyptr = history.size();
        // scroll console view to end
        scrolltoend();
    }
}

void rpcconsole::browsehistory(int offset)
{
    historyptr += offset;
    if(historyptr < 0)
        historyptr = 0;
    if(historyptr > history.size())
        historyptr = history.size();
    qstring cmd;
    if(historyptr < history.size())
        cmd = history.at(historyptr);
    ui->lineedit->settext(cmd);
}

void rpcconsole::startexecutor()
{
    qthread *thread = new qthread;
    rpcexecutor *executor = new rpcexecutor();
    executor->movetothread(thread);

    // replies from executor object must go to this object
    connect(executor, signal(reply(int,qstring)), this, slot(message(int,qstring)));
    // requests from this object must go to executor
    connect(this, signal(cmdrequest(qstring)), executor, slot(request(qstring)));

    // on stopexecutor signal
    // - queue executor for deletion (in execution thread)
    // - quit the qt event loop in the execution thread
    connect(this, signal(stopexecutor()), executor, slot(deletelater()));
    connect(this, signal(stopexecutor()), thread, slot(quit()));
    // queue the thread for deletion (in this thread) when it is finished
    connect(thread, signal(finished()), thread, slot(deletelater()));

    // default implementation of qthread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread->start();
}

void rpcconsole::on_tabwidget_currentchanged(int index)
{
    if (ui->tabwidget->widget(index) == ui->tab_console)
        ui->lineedit->setfocus();
    else if (ui->tabwidget->widget(index) != ui->tab_peers)
        clearselectednode();
}

void rpcconsole::on_opendebuglogfilebutton_clicked()
{
    guiutil::opendebuglogfile();
}

void rpcconsole::scrolltoend()
{
    qscrollbar *scrollbar = ui->messageswidget->verticalscrollbar();
    scrollbar->setvalue(scrollbar->maximum());
}

void rpcconsole::on_sldgraphrange_valuechanged(int value)
{
    const int multiplier = 5; // each position on the slider represents 5 min
    int mins = value * multiplier;
    settrafficgraphrange(mins);
}

qstring rpcconsole::formatbytes(quint64 bytes)
{
    if(bytes < 1024)
        return qstring(tr("%1 b")).arg(bytes);
    if(bytes < 1024 * 1024)
        return qstring(tr("%1 kb")).arg(bytes / 1024);
    if(bytes < 1024 * 1024 * 1024)
        return qstring(tr("%1 mb")).arg(bytes / 1024 / 1024);

    return qstring(tr("%1 gb")).arg(bytes / 1024 / 1024 / 1024);
}

void rpcconsole::settrafficgraphrange(int mins)
{
    ui->trafficgraph->setgraphrangemins(mins);
    ui->lblgraphrange->settext(guiutil::formatdurationstr(mins * 60));
}

void rpcconsole::updatetrafficstats(quint64 totalbytesin, quint64 totalbytesout)
{
    ui->lblbytesin->settext(formatbytes(totalbytesin));
    ui->lblbytesout->settext(formatbytes(totalbytesout));
}

void rpcconsole::peerselected(const qitemselection &selected, const qitemselection &deselected)
{
    q_unused(deselected);

    if (!clientmodel || selected.indexes().isempty())
        return;

    const cnodecombinedstats *stats = clientmodel->getpeertablemodel()->getnodestats(selected.indexes().first().row());
    if (stats)
        updatenodedetail(stats);
}

void rpcconsole::peerlayoutchanged()
{
    if (!clientmodel)
        return;

    const cnodecombinedstats *stats = null;
    bool funselect = false;
    bool freselect = false;

    if (cachednodeid == -1) // no node selected yet
        return;

    // find the currently selected row
    int selectedrow = -1;
    qmodelindexlist selectedmodelindex = ui->peerwidget->selectionmodel()->selectedindexes();
    if (!selectedmodelindex.isempty()) {
        selectedrow = selectedmodelindex.first().row();
    }

    // check if our detail node has a row in the table (it may not necessarily
    // be at selectedrow since its position can change after a layout change)
    int detailnoderow = clientmodel->getpeertablemodel()->getrowbynodeid(cachednodeid);

    if (detailnoderow < 0)
    {
        // detail node dissapeared from table (node disconnected)
        funselect = true;
    }
    else
    {
        if (detailnoderow != selectedrow)
        {
            // detail node moved position
            funselect = true;
            freselect = true;
        }

        // get fresh stats on the detail node.
        stats = clientmodel->getpeertablemodel()->getnodestats(detailnoderow);
    }

    if (funselect && selectedrow >= 0) {
        clearselectednode();
    }

    if (freselect)
    {
        ui->peerwidget->selectrow(detailnoderow);
    }

    if (stats)
        updatenodedetail(stats);
}

void rpcconsole::updatenodedetail(const cnodecombinedstats *stats)
{
    // update cached nodeid
    cachednodeid = stats->nodestats.nodeid;

    // update the detail ui with latest node information
    qstring peeraddrdetails(qstring::fromstdstring(stats->nodestats.addrname) + " ");
    peeraddrdetails += tr("(node id: %1)").arg(qstring::number(stats->nodestats.nodeid));
    if (!stats->nodestats.addrlocal.empty())
        peeraddrdetails += "<br />" + tr("via %1").arg(qstring::fromstdstring(stats->nodestats.addrlocal));
    ui->peerheading->settext(peeraddrdetails);
    ui->peerservices->settext(guiutil::formatservicesstr(stats->nodestats.nservices));
    ui->peerlastsend->settext(stats->nodestats.nlastsend ? guiutil::formatdurationstr(gettime() - stats->nodestats.nlastsend) : tr("never"));
    ui->peerlastrecv->settext(stats->nodestats.nlastrecv ? guiutil::formatdurationstr(gettime() - stats->nodestats.nlastrecv) : tr("never"));
    ui->peerbytessent->settext(formatbytes(stats->nodestats.nsendbytes));
    ui->peerbytesrecv->settext(formatbytes(stats->nodestats.nrecvbytes));
    ui->peerconntime->settext(guiutil::formatdurationstr(gettime() - stats->nodestats.ntimeconnected));
    ui->peerpingtime->settext(guiutil::formatpingtime(stats->nodestats.dpingtime));
    ui->peerpingwait->settext(guiutil::formatpingtime(stats->nodestats.dpingwait));
    ui->timeoffset->settext(guiutil::formattimeoffset(stats->nodestats.ntimeoffset));
    ui->peerversion->settext(qstring("%1").arg(qstring::number(stats->nodestats.nversion)));
    ui->peersubversion->settext(qstring::fromstdstring(stats->nodestats.cleansubver));
    ui->peerdirection->settext(stats->nodestats.finbound ? tr("inbound") : tr("outbound"));
    ui->peerheight->settext(qstring("%1").arg(qstring::number(stats->nodestats.nstartingheight)));
    ui->peerwhitelisted->settext(stats->nodestats.fwhitelisted ? tr("yes") : tr("no"));

    // this check fails for example if the lock was busy and
    // nodestatestats couldn't be fetched.
    if (stats->fnodestatestatsavailable) {
        // ban score is init to 0
        ui->peerbanscore->settext(qstring("%1").arg(stats->nodestatestats.nmisbehavior));

        // sync height is init to -1
        if (stats->nodestatestats.nsyncheight > -1)
            ui->peersyncheight->settext(qstring("%1").arg(stats->nodestatestats.nsyncheight));
        else
            ui->peersyncheight->settext(tr("unknown"));

        // common height is init to -1
        if (stats->nodestatestats.ncommonheight > -1)
            ui->peercommonheight->settext(qstring("%1").arg(stats->nodestatestats.ncommonheight));
        else
            ui->peercommonheight->settext(tr("unknown"));
    }

    ui->detailwidget->show();
}

void rpcconsole::resizeevent(qresizeevent *event)
{
    qwidget::resizeevent(event);
}

void rpcconsole::showevent(qshowevent *event)
{
    qwidget::showevent(event);

    if (!clientmodel)
        return;

    // start peertablemodel auto refresh
    clientmodel->getpeertablemodel()->startautorefresh();
}

void rpcconsole::hideevent(qhideevent *event)
{
    qwidget::hideevent(event);

    if (!clientmodel)
        return;

    // stop peertablemodel auto refresh
    clientmodel->getpeertablemodel()->stopautorefresh();
}

void rpcconsole::showmenu(const qpoint& point)
{
    qmodelindex index = ui->peerwidget->indexat(point);
    if (index.isvalid())
        contextmenu->exec(qcursor::pos());
}

void rpcconsole::disconnectselectednode()
{
    // get currently selected peer address
    qstring strnode = guiutil::getentrydata(ui->peerwidget, 0, peertablemodel::address);
    // find the node, disconnect it and clear the selected node
    if (cnode *bannednode = findnode(strnode.tostdstring())) {
        bannednode->fdisconnect = true;
        clearselectednode();
    }
}

void rpcconsole::clearselectednode()
{
    ui->peerwidget->selectionmodel()->clearselection();
    cachednodeid = -1;
    ui->detailwidget->hide();
    ui->peerheading->settext(tr("select a peer to view detailed information."));
}
