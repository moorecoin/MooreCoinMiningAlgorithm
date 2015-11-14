// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_core_io_h
#define moorecoin_core_io_h

#include <string>
#include <vector>

class cblock;
class cscript;
class ctransaction;
class uint256;
class univalue;

// core_read.cpp
extern cscript parsescript(const std::string& s);
extern bool decodehextx(ctransaction& tx, const std::string& strhextx);
extern bool decodehexblk(cblock&, const std::string& strhexblk);
extern uint256 parsehashuv(const univalue& v, const std::string& strname);
extern uint256 parsehashstr(const std::string&, const std::string& strname);
extern std::vector<unsigned char> parsehexuv(const univalue& v, const std::string& strname);

// core_write.cpp
extern std::string formatscript(const cscript& script);
extern std::string encodehextx(const ctransaction& tx);
extern void scriptpubkeytouniv(const cscript& scriptpubkey,
                        univalue& out, bool fincludehex);
extern void txtouniv(const ctransaction& tx, const uint256& hashblock, univalue& entry);

#endif // moorecoin_core_io_h
