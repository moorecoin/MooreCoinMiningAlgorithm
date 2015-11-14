// copyright 2014 bitpay inc.
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>
#include <ctype.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>      // std::runtime_error

#include "univalue.h"

#include "utilstrencodings.h" // parsexx

using namespace std;

const univalue nullunivalue;

void univalue::clear()
{
    typ = vnull;
    val.clear();
    keys.clear();
    values.clear();
}

bool univalue::setnull()
{
    clear();
    return true;
}

bool univalue::setbool(bool val_)
{
    clear();
    typ = vbool;
    if (val_)
        val = "1";
    return true;
}

static bool validnumstr(const string& s)
{
    string tokenval;
    unsigned int consumed;
    enum jtokentype tt = getjsontoken(tokenval, consumed, s.c_str());
    return (tt == jtok_number);
}

bool univalue::setnumstr(const string& val_)
{
    if (!validnumstr(val_))
        return false;

    clear();
    typ = vnum;
    val = val_;
    return true;
}

bool univalue::setint(uint64_t val)
{
    string s;
    ostringstream oss;

    oss << val;

    return setnumstr(oss.str());
}

bool univalue::setint(int64_t val)
{
    string s;
    ostringstream oss;

    oss << val;

    return setnumstr(oss.str());
}

bool univalue::setfloat(double val)
{
    string s;
    ostringstream oss;

    oss << std::setprecision(16) << val;

    bool ret = setnumstr(oss.str());
    typ = vreal;
    return ret;
}

bool univalue::setstr(const string& val_)
{
    clear();
    typ = vstr;
    val = val_;
    return true;
}

bool univalue::setarray()
{
    clear();
    typ = varr;
    return true;
}

bool univalue::setobject()
{
    clear();
    typ = vobj;
    return true;
}

bool univalue::push_back(const univalue& val)
{
    if (typ != varr)
        return false;

    values.push_back(val);
    return true;
}

bool univalue::push_backv(const std::vector<univalue>& vec)
{
    if (typ != varr)
        return false;

    values.insert(values.end(), vec.begin(), vec.end());

    return true;
}

bool univalue::pushkv(const std::string& key, const univalue& val)
{
    if (typ != vobj)
        return false;

    keys.push_back(key);
    values.push_back(val);
    return true;
}

bool univalue::pushkvs(const univalue& obj)
{
    if (typ != vobj || obj.typ != vobj)
        return false;

    for (unsigned int i = 0; i < obj.keys.size(); i++) {
        keys.push_back(obj.keys[i]);
        values.push_back(obj.values[i]);
    }

    return true;
}

int univalue::findkey(const std::string& key) const
{
    for (unsigned int i = 0; i < keys.size(); i++) {
        if (keys[i] == key)
            return (int) i;
    }

    return -1;
}

bool univalue::checkobject(const std::map<std::string,univalue::vtype>& t)
{
    for (std::map<std::string,univalue::vtype>::const_iterator it = t.begin();
         it != t.end(); it++) {
        int idx = findkey(it->first);
        if (idx < 0)
            return false;

        if (values[idx].gettype() != it->second)
            return false;
    }

    return true;
}

const univalue& univalue::operator[](const std::string& key) const
{
    if (typ != vobj)
        return nullunivalue;

    int index = findkey(key);
    if (index < 0)
        return nullunivalue;

    return values[index];
}

const univalue& univalue::operator[](unsigned int index) const
{
    if (typ != vobj && typ != varr)
        return nullunivalue;
    if (index >= values.size())
        return nullunivalue;

    return values[index];
}

const char *uvtypename(univalue::vtype t)
{
    switch (t) {
    case univalue::vnull: return "null";
    case univalue::vbool: return "bool";
    case univalue::vobj: return "object";
    case univalue::varr: return "array";
    case univalue::vstr: return "string";
    case univalue::vnum: return "number";
    case univalue::vreal: return "number";
    }

    // not reached
    return null;
}

const univalue& find_value( const univalue& obj, const std::string& name)
{
    for (unsigned int i = 0; i < obj.keys.size(); i++)
    {
        if( obj.keys[i] == name )
        {
            return obj.values[i];
        }
    }

    return nullunivalue;
}

std::vector<std::string> univalue::getkeys() const
{
    if (typ != vobj)
        throw std::runtime_error("json value is not an object as expected");
    return keys;
}

std::vector<univalue> univalue::getvalues() const
{
    if (typ != vobj && typ != varr)
        throw std::runtime_error("json value is not an object or array as expected");
    return values;
}

bool univalue::get_bool() const
{
    if (typ != vbool)
        throw std::runtime_error("json value is not a boolean as expected");
    return getbool();
}

std::string univalue::get_str() const
{
    if (typ != vstr)
        throw std::runtime_error("json value is not a string as expected");
    return getvalstr();
}

int univalue::get_int() const
{
    if (typ != vnum)
        throw std::runtime_error("json value is not an integer as expected");
    int32_t retval;
    if (!parseint32(getvalstr(), &retval))
        throw std::runtime_error("json integer out of range");
    return retval;
}

int64_t univalue::get_int64() const
{
    if (typ != vnum)
        throw std::runtime_error("json value is not an integer as expected");
    int64_t retval;
    if (!parseint64(getvalstr(), &retval))
        throw std::runtime_error("json integer out of range");
    return retval;
}

double univalue::get_real() const
{
    if (typ != vreal && typ != vnum)
        throw std::runtime_error("json value is not a number as expected");
    double retval;
    if (!parsedouble(getvalstr(), &retval))
        throw std::runtime_error("json double out of range");
    return retval;
}

const univalue& univalue::get_obj() const
{
    if (typ != vobj)
        throw std::runtime_error("json value is not an object as expected");
    return *this;
}

const univalue& univalue::get_array() const
{
    if (typ != varr)
        throw std::runtime_error("json value is not an array as expected");
    return *this;
}

