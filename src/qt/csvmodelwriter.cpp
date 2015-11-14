// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "csvmodelwriter.h"

#include <qabstractitemmodel>
#include <qfile>
#include <qtextstream>

csvmodelwriter::csvmodelwriter(const qstring &filename, qobject *parent) :
    qobject(parent),
    filename(filename), model(0)
{
}

void csvmodelwriter::setmodel(const qabstractitemmodel *model)
{
    this->model = model;
}

void csvmodelwriter::addcolumn(const qstring &title, int column, int role)
{
    column col;
    col.title = title;
    col.column = column;
    col.role = role;

    columns.append(col);
}

static void writevalue(qtextstream &f, const qstring &value)
{
    qstring escaped = value;
    escaped.replace('"', "\"\"");
    f << "\"" << escaped << "\"";
}

static void writesep(qtextstream &f)
{
    f << ",";
}

static void writenewline(qtextstream &f)
{
    f << "\n";
}

bool csvmodelwriter::write()
{
    qfile file(filename);
    if(!file.open(qiodevice::writeonly | qiodevice::text))
        return false;
    qtextstream out(&file);

    int numrows = 0;
    if(model)
    {
        numrows = model->rowcount();
    }

    // header row
    for(int i=0; i<columns.size(); ++i)
    {
        if(i!=0)
        {
            writesep(out);
        }
        writevalue(out, columns[i].title);
    }
    writenewline(out);

    // data rows
    for(int j=0; j<numrows; ++j)
    {
        for(int i=0; i<columns.size(); ++i)
        {
            if(i!=0)
            {
                writesep(out);
            }
            qvariant data = model->index(j, columns[i].column).data(columns[i].role);
            writevalue(out, data.tostring());
        }
        writenewline(out);
    }

    file.close();

    return file.error() == qfile::noerror;
}
