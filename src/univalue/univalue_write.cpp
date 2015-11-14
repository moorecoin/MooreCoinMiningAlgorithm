// copyright 2014 bitpay inc.
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include <ctype.h>
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include "univalue.h"
#include "univalue_escapes.h"

// todo: using utf8

using namespace std;

static string json_escape(const string& ins)
{
    string outs;
    outs.reserve(ins.size() * 2);

    for (unsigned int i = 0; i < ins.size(); i++) {
        unsigned char ch = ins[i];
        const char *escstr = escapes[ch];

        if (escstr)
            outs += escstr;

        else if (isprint(ch))
            outs += ch;

        else {
            char tmpesc[16];
            sprintf(tmpesc, "\\u%04x", ch);
            outs += tmpesc;
        }
    }

    return outs;
}

string univalue::write(unsigned int prettyindent,
                       unsigned int indentlevel) const
{
    string s;
    s.reserve(1024);

    unsigned int modindent = indentlevel;
    if (modindent == 0)
        modindent = 1;

    switch (typ) {
    case vnull:
        s += "null";
        break;
    case vobj:
        writeobject(prettyindent, modindent, s);
        break;
    case varr:
        writearray(prettyindent, modindent, s);
        break;
    case vstr:
        s += "\"" + json_escape(val) + "\"";
        break;
    case vreal:
        {
            std::stringstream ss;
            ss << std::showpoint << std::fixed << std::setprecision(8) << get_real();
            s += ss.str();
        }
        break;
    case vnum:
        s += val;
        break;
    case vbool:
        s += (val == "1" ? "true" : "false");
        break;
    }

    return s;
}

static void indentstr(unsigned int prettyindent, unsigned int indentlevel, string& s)
{
    s.append(prettyindent * indentlevel, ' ');
}

void univalue::writearray(unsigned int prettyindent, unsigned int indentlevel, string& s) const
{
    s += "[";
    if (prettyindent)
        s += "\n";

    for (unsigned int i = 0; i < values.size(); i++) {
        if (prettyindent)
            indentstr(prettyindent, indentlevel, s);
        s += values[i].write(prettyindent, indentlevel + 1);
        if (i != (values.size() - 1)) {
            s += ",";
            if (prettyindent)
                s += " ";
        }
        if (prettyindent)
            s += "\n";
    }

    if (prettyindent)
        indentstr(prettyindent, indentlevel - 1, s);
    s += "]";
}

void univalue::writeobject(unsigned int prettyindent, unsigned int indentlevel, string& s) const
{
    s += "{";
    if (prettyindent)
        s += "\n";

    for (unsigned int i = 0; i < keys.size(); i++) {
        if (prettyindent)
            indentstr(prettyindent, indentlevel, s);
        s += "\"" + json_escape(keys[i]) + "\":";
        if (prettyindent)
            s += " ";
        s += values[i].write(prettyindent, indentlevel + 1);
        if (i != (values.size() - 1))
            s += ",";
        if (prettyindent)
            s += "\n";
    }

    if (prettyindent)
        indentstr(prettyindent, indentlevel - 1, s);
    s += "}";
}

