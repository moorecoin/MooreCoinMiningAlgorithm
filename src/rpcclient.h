// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_rpcclient_h
#define moorecoin_rpcclient_h

#include "univalue/univalue.h"

univalue rpcconvertvalues(const std::string& strmethod, const std::vector<std::string>& strparams);
/** non-rfc4627 json parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
univalue parsenonrfcjsonvalue(const std::string& strval);

#endif // moorecoin_rpcclient_h
