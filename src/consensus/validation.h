// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_consensus_validation_h
#define moorecoin_consensus_validation_h

#include <string>

/** "reject" message codes */
static const unsigned char reject_malformed = 0x01;
static const unsigned char reject_invalid = 0x10;
static const unsigned char reject_obsolete = 0x11;
static const unsigned char reject_duplicate = 0x12;
static const unsigned char reject_nonstandard = 0x40;
static const unsigned char reject_dust = 0x41;
static const unsigned char reject_insufficientfee = 0x42;
static const unsigned char reject_checkpoint = 0x43;

/** capture information about block/transaction validation */
class cvalidationstate {
private:
    enum mode_state {
        mode_valid,   //! everything ok
        mode_invalid, //! network rule violation (dos value may be set)
        mode_error,   //! run-time error
    } mode;
    int ndos;
    std::string strrejectreason;
    unsigned char chrejectcode;
    bool corruptionpossible;
public:
    cvalidationstate() : mode(mode_valid), ndos(0), chrejectcode(0), corruptionpossible(false) {}
    bool dos(int level, bool ret = false,
             unsigned char chrejectcodein=0, std::string strrejectreasonin="",
             bool corruptionin=false) {
        chrejectcode = chrejectcodein;
        strrejectreason = strrejectreasonin;
        corruptionpossible = corruptionin;
        if (mode == mode_error)
            return ret;
        ndos += level;
        mode = mode_invalid;
        return ret;
    }
    bool invalid(bool ret = false,
                 unsigned char _chrejectcode=0, std::string _strrejectreason="") {
        return dos(0, ret, _chrejectcode, _strrejectreason);
    }
    bool error(const std::string& strrejectreasonin) {
        if (mode == mode_valid)
            strrejectreason = strrejectreasonin;
        mode = mode_error;
        return false;
    }
    bool isvalid() const {
        return mode == mode_valid;
    }
    bool isinvalid() const {
        return mode == mode_invalid;
    }
    bool iserror() const {
        return mode == mode_error;
    }
    bool isinvalid(int &ndosout) const {
        if (isinvalid()) {
            ndosout = ndos;
            return true;
        }
        return false;
    }
    bool corruptionpossible() const {
        return corruptionpossible;
    }
    unsigned char getrejectcode() const { return chrejectcode; }
    std::string getrejectreason() const { return strrejectreason; }
};

#endif // moorecoin_consensus_validation_h
