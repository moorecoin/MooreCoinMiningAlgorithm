// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_csvmodelwriter_h
#define moorecoin_qt_csvmodelwriter_h

#include <qlist>
#include <qobject>

qt_begin_namespace
class qabstractitemmodel;
qt_end_namespace

/** export a qt table model to a csv file. this is useful for analyzing or post-processing the data in
    a spreadsheet.
 */
class csvmodelwriter : public qobject
{
    q_object

public:
    explicit csvmodelwriter(const qstring &filename, qobject *parent = 0);

    void setmodel(const qabstractitemmodel *model);
    void addcolumn(const qstring &title, int column, int role=qt::editrole);

    /** perform export of the model to csv.
        @returns true on success, false otherwise
    */
    bool write();

private:
    qstring filename;
    const qabstractitemmodel *model;

    struct column
    {
        qstring title;
        int column;
        int role;
    };
    qlist<column> columns;
};

#endif // moorecoin_qt_csvmodelwriter_h
