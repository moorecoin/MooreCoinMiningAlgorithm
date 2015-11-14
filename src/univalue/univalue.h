// copyright 2014 bitpay inc.
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_univalue_univalue_h
#define moorecoin_univalue_univalue_h

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <cassert>

#include <sstream>        // .get_int64()
#include <utility>        // std::pair

class univalue {
public:
    enum vtype { vnull, vobj, varr, vstr, vnum, vreal, vbool, };

    univalue() { typ = vnull; }
    univalue(univalue::vtype initialtype, const std::string& initialstr = "") {
        typ = initialtype;
        val = initialstr;
    }
    univalue(uint64_t val_) {
        setint(val_);
    }
    univalue(int64_t val_) {
        setint(val_);
    }
    univalue(bool val_) {
        setbool(val_);
    }
    univalue(int val_) {
        setint(val_);
    }
    univalue(double val_) {
        setfloat(val_);
    }
    univalue(const std::string& val_) {
        setstr(val_);
    }
    univalue(const char *val_) {
        std::string s(val_);
        setstr(s);
    }
    ~univalue() {}

    void clear();

    bool setnull();
    bool setbool(bool val);
    bool setnumstr(const std::string& val);
    bool setint(uint64_t val);
    bool setint(int64_t val);
    bool setint(int val) { return setint((int64_t)val); }
    bool setfloat(double val);
    bool setstr(const std::string& val);
    bool setarray();
    bool setobject();

    enum vtype gettype() const { return typ; }
    const std::string& getvalstr() const { return val; }
    bool empty() const { return (values.size() == 0); }

    size_t size() const { return values.size(); }

    bool getbool() const { return istrue(); }
    bool checkobject(const std::map<std::string,univalue::vtype>& membertypes);
    const univalue& operator[](const std::string& key) const;
    const univalue& operator[](unsigned int index) const;
    bool exists(const std::string& key) const { return (findkey(key) >= 0); }

    bool isnull() const { return (typ == vnull); }
    bool istrue() const { return (typ == vbool) && (val == "1"); }
    bool isfalse() const { return (typ == vbool) && (val != "1"); }
    bool isbool() const { return (typ == vbool); }
    bool isstr() const { return (typ == vstr); }
    bool isnum() const { return (typ == vnum); }
    bool isreal() const { return (typ == vreal); }
    bool isarray() const { return (typ == varr); }
    bool isobject() const { return (typ == vobj); }

    bool push_back(const univalue& val);
    bool push_back(const std::string& val_) {
        univalue tmpval(vstr, val_);
        return push_back(tmpval);
    }
    bool push_back(const char *val_) {
        std::string s(val_);
        return push_back(s);
    }
    bool push_backv(const std::vector<univalue>& vec);

    bool pushkv(const std::string& key, const univalue& val);
    bool pushkv(const std::string& key, const std::string& val) {
        univalue tmpval(vstr, val);
        return pushkv(key, tmpval);
    }
    bool pushkv(const std::string& key, const char *val_) {
        std::string val(val_);
        return pushkv(key, val);
    }
    bool pushkv(const std::string& key, int64_t val) {
        univalue tmpval(val);
        return pushkv(key, tmpval);
    }
    bool pushkv(const std::string& key, uint64_t val) {
        univalue tmpval(val);
        return pushkv(key, tmpval);
    }
    bool pushkv(const std::string& key, int val) {
        univalue tmpval((int64_t)val);
        return pushkv(key, tmpval);
    }
    bool pushkv(const std::string& key, double val) {
        univalue tmpval(val);
        return pushkv(key, tmpval);
    }
    bool pushkvs(const univalue& obj);

    std::string write(unsigned int prettyindent = 0,
                      unsigned int indentlevel = 0) const;

    bool read(const char *raw);
    bool read(const std::string& rawstr) {
        return read(rawstr.c_str());
    }

private:
    univalue::vtype typ;
    std::string val;                       // numbers are stored as c++ strings
    std::vector<std::string> keys;
    std::vector<univalue> values;

    int findkey(const std::string& key) const;
    void writearray(unsigned int prettyindent, unsigned int indentlevel, std::string& s) const;
    void writeobject(unsigned int prettyindent, unsigned int indentlevel, std::string& s) const;

public:
    // strict type-specific getters, these throw std::runtime_error if the
    // value is of unexpected type
    std::vector<std::string> getkeys() const;
    std::vector<univalue> getvalues() const;
    bool get_bool() const;
    std::string get_str() const;
    int get_int() const;
    int64_t get_int64() const;
    double get_real() const;
    const univalue& get_obj() const;
    const univalue& get_array() const;

    enum vtype type() const { return gettype(); }
    bool push_back(std::pair<std::string,univalue> pear) {
        return pushkv(pear.first, pear.second);
    }
    friend const univalue& find_value( const univalue& obj, const std::string& name);
};

//
// the following were added for compatibility with json_spirit.
// most duplicate other methods, and should be removed.
//
static inline std::pair<std::string,univalue> pair(const char *ckey, const char *cval)
{
    std::string key(ckey);
    univalue uval(cval);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, std::string strval)
{
    std::string key(ckey);
    univalue uval(strval);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, uint64_t u64val)
{
    std::string key(ckey);
    univalue uval(u64val);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, int64_t i64val)
{
    std::string key(ckey);
    univalue uval(i64val);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, bool ival)
{
    std::string key(ckey);
    univalue uval(ival);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, int ival)
{
    std::string key(ckey);
    univalue uval(ival);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, double dval)
{
    std::string key(ckey);
    univalue uval(dval);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(const char *ckey, const univalue& uval)
{
    std::string key(ckey);
    return std::make_pair(key, uval);
}

static inline std::pair<std::string,univalue> pair(std::string key, const univalue& uval)
{
    return std::make_pair(key, uval);
}

enum jtokentype {
    jtok_err        = -1,
    jtok_none       = 0,                           // eof
    jtok_obj_open,
    jtok_obj_close,
    jtok_arr_open,
    jtok_arr_close,
    jtok_colon,
    jtok_comma,
    jtok_kw_null,
    jtok_kw_true,
    jtok_kw_false,
    jtok_number,
    jtok_string,
};

extern enum jtokentype getjsontoken(std::string& tokenval,
                                    unsigned int& consumed, const char *raw);
extern const char *uvtypename(univalue::vtype t);

extern const univalue nullunivalue;

const univalue& find_value( const univalue& obj, const std::string& name);

#endif // moorecoin_univalue_univalue_h
