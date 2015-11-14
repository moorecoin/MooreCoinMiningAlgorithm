// copyright 2014 bitpay inc.
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>
#include <vector>
#include <stdio.h>
#include "univalue.h"

using namespace std;

// convert hexadecimal string to unsigned integer
static const char *hatoui(const char *first, const char *last,
                          unsigned int& out)
{
    unsigned int result = 0;
    for (; first != last; ++first)
    {
        int digit;
        if (isdigit(*first))
            digit = *first - '0';

        else if (*first >= 'a' && *first <= 'f')
            digit = *first - 'a' + 10;

        else if (*first >= 'a' && *first <= 'f')
            digit = *first - 'a' + 10;

        else
            break;

        result = 16 * result + digit;
    }
    out = result;

    return first;
}

enum jtokentype getjsontoken(string& tokenval, unsigned int& consumed,
                            const char *raw)
{
    tokenval.clear();
    consumed = 0;

    const char *rawstart = raw;

    while ((*raw) && (isspace(*raw)))             // skip whitespace
        raw++;

    switch (*raw) {

    case 0:
        return jtok_none;

    case '{':
        raw++;
        consumed = (raw - rawstart);
        return jtok_obj_open;
    case '}':
        raw++;
        consumed = (raw - rawstart);
        return jtok_obj_close;
    case '[':
        raw++;
        consumed = (raw - rawstart);
        return jtok_arr_open;
    case ']':
        raw++;
        consumed = (raw - rawstart);
        return jtok_arr_close;

    case ':':
        raw++;
        consumed = (raw - rawstart);
        return jtok_colon;
    case ',':
        raw++;
        consumed = (raw - rawstart);
        return jtok_comma;

    case 'n':
    case 't':
    case 'f':
        if (!strncmp(raw, "null", 4)) {
            raw += 4;
            consumed = (raw - rawstart);
            return jtok_kw_null;
        } else if (!strncmp(raw, "true", 4)) {
            raw += 4;
            consumed = (raw - rawstart);
            return jtok_kw_true;
        } else if (!strncmp(raw, "false", 5)) {
            raw += 5;
            consumed = (raw - rawstart);
            return jtok_kw_false;
        } else
            return jtok_err;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        // part 1: int
        string numstr;

        const char *first = raw;

        const char *firstdigit = first;
        if (!isdigit(*firstdigit))
            firstdigit++;
        if ((*firstdigit == '0') && isdigit(firstdigit[1]))
            return jtok_err;

        numstr += *raw;                       // copy first char
        raw++;

        if ((*first == '-') && (!isdigit(*raw)))
            return jtok_err;

        while ((*raw) && isdigit(*raw)) {     // copy digits
            numstr += *raw;
            raw++;
        }

        // part 2: frac
        if (*raw == '.') {
            numstr += *raw;                   // copy .
            raw++;

            if (!isdigit(*raw))
                return jtok_err;
            while ((*raw) && isdigit(*raw)) { // copy digits
                numstr += *raw;
                raw++;
            }
        }

        // part 3: exp
        if (*raw == 'e' || *raw == 'e') {
            numstr += *raw;                   // copy e
            raw++;

            if (*raw == '-' || *raw == '+') { // copy +/-
                numstr += *raw;
                raw++;
            }

            if (!isdigit(*raw))
                return jtok_err;
            while ((*raw) && isdigit(*raw)) { // copy digits
                numstr += *raw;
                raw++;
            }
        }

        tokenval = numstr;
        consumed = (raw - rawstart);
        return jtok_number;
        }

    case '"': {
        raw++;                                // skip "

        string valstr;

        while (*raw) {
            if (*raw < 0x20)
                return jtok_err;

            else if (*raw == '\\') {
                raw++;                        // skip backslash

                switch (*raw) {
                case '"':  valstr += "\""; break;
                case '\\': valstr += "\\"; break;
                case '/':  valstr += "/"; break;
                case 'b':  valstr += "\b"; break;
                case 'f':  valstr += "\f"; break;
                case 'n':  valstr += "\n"; break;
                case 'r':  valstr += "\r"; break;
                case 't':  valstr += "\t"; break;

                case 'u': {
                    unsigned int codepoint;
                    if (hatoui(raw + 1, raw + 1 + 4, codepoint) !=
                               raw + 1 + 4)
                        return jtok_err;

                    if (codepoint <= 0x7f)
                        valstr.push_back((char)codepoint);
                    else if (codepoint <= 0x7ff) {
                        valstr.push_back((char)(0xc0 | (codepoint >> 6)));
                        valstr.push_back((char)(0x80 | (codepoint & 0x3f)));
                    } else if (codepoint <= 0xffff) {
                        valstr.push_back((char)(0xe0 | (codepoint >> 12)));
                        valstr.push_back((char)(0x80 | ((codepoint >> 6) & 0x3f)));
                        valstr.push_back((char)(0x80 | (codepoint & 0x3f)));
                    }

                    raw += 4;
                    break;
                    }
                default:
                    return jtok_err;

                }

                raw++;                        // skip esc'd char
            }

            else if (*raw == '"') {
                raw++;                        // skip "
                break;                        // stop scanning
            }

            else {
                valstr += *raw;
                raw++;
            }
        }

        tokenval = valstr;
        consumed = (raw - rawstart);
        return jtok_string;
        }

    default:
        return jtok_err;
    }
}

bool univalue::read(const char *raw)
{
    clear();

    bool expectname = false;
    bool expectcolon = false;
    vector<univalue*> stack;

    enum jtokentype tok = jtok_none;
    enum jtokentype last_tok = jtok_none;
    while (1) {
        last_tok = tok;

        string tokenval;
        unsigned int consumed;
        tok = getjsontoken(tokenval, consumed, raw);
        if (tok == jtok_none || tok == jtok_err)
            break;
        raw += consumed;

        switch (tok) {

        case jtok_obj_open:
        case jtok_arr_open: {
            vtype utyp = (tok == jtok_obj_open ? vobj : varr);
            if (!stack.size()) {
                if (utyp == vobj)
                    setobject();
                else
                    setarray();
                stack.push_back(this);
            } else {
                univalue tmpval(utyp);
                univalue *top = stack.back();
                top->values.push_back(tmpval);

                univalue *newtop = &(top->values.back());
                stack.push_back(newtop);
            }

            if (utyp == vobj)
                expectname = true;
            break;
            }

        case jtok_obj_close:
        case jtok_arr_close: {
            if (!stack.size() || expectcolon || (last_tok == jtok_comma))
                return false;

            vtype utyp = (tok == jtok_obj_close ? vobj : varr);
            univalue *top = stack.back();
            if (utyp != top->gettype())
                return false;

            stack.pop_back();
            expectname = false;
            break;
            }

        case jtok_colon: {
            if (!stack.size() || expectname || !expectcolon)
                return false;

            univalue *top = stack.back();
            if (top->gettype() != vobj)
                return false;

            expectcolon = false;
            break;
            }

        case jtok_comma: {
            if (!stack.size() || expectname || expectcolon ||
                (last_tok == jtok_comma) || (last_tok == jtok_arr_open))
                return false;

            univalue *top = stack.back();
            if (top->gettype() == vobj)
                expectname = true;
            break;
            }

        case jtok_kw_null:
        case jtok_kw_true:
        case jtok_kw_false: {
            if (!stack.size() || expectname || expectcolon)
                return false;

            univalue tmpval;
            switch (tok) {
            case jtok_kw_null:
                // do nothing more
                break;
            case jtok_kw_true:
                tmpval.setbool(true);
                break;
            case jtok_kw_false:
                tmpval.setbool(false);
                break;
            default: /* impossible */ break;
            }

            univalue *top = stack.back();
            top->values.push_back(tmpval);

            break;
            }

        case jtok_number: {
            if (!stack.size() || expectname || expectcolon)
                return false;

            univalue tmpval(vnum, tokenval);
            univalue *top = stack.back();
            top->values.push_back(tmpval);

            break;
            }

        case jtok_string: {
            if (!stack.size())
                return false;

            univalue *top = stack.back();

            if (expectname) {
                top->keys.push_back(tokenval);
                expectname = false;
                expectcolon = true;
            } else {
                univalue tmpval(vstr, tokenval);
                top->values.push_back(tmpval);
            }

            break;
            }

        default:
            return false;
        }
    }

    if (stack.size() != 0)
        return false;

    return true;
}

