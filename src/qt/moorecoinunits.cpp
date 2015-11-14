// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "moorecoinunits.h"

#include "primitives/transaction.h"

#include <qstringlist>

moorecoinunits::moorecoinunits(qobject *parent):
        qabstractlistmodel(parent),
        unitlist(availableunits())
{
}

qlist<moorecoinunits::unit> moorecoinunits::availableunits()
{
    qlist<moorecoinunits::unit> unitlist;
    unitlist.append(btc);
    unitlist.append(mbtc);
    unitlist.append(ubtc);
    return unitlist;
}

bool moorecoinunits::valid(int unit)
{
    switch(unit)
    {
    case btc:
    case mbtc:
    case ubtc:
        return true;
    default:
        return false;
    }
}

qstring moorecoinunits::name(int unit)
{
    switch(unit)
    {
    case btc: return qstring("btc");
    case mbtc: return qstring("mbtc");
    case ubtc: return qstring::fromutf8("渭btc");
    default: return qstring("???");
    }
}

qstring moorecoinunits::description(int unit)
{
    switch(unit)
    {
    case btc: return qstring("moorecoins");
    case mbtc: return qstring("milli-moorecoins (1 / 1" thin_sp_utf8 "000)");
    case ubtc: return qstring("micro-moorecoins (1 / 1" thin_sp_utf8 "000" thin_sp_utf8 "000)");
    default: return qstring("???");
    }
}

qint64 moorecoinunits::factor(int unit)
{
    switch(unit)
    {
    case btc:  return 100000000;
    case mbtc: return 100000;
    case ubtc: return 100;
    default:   return 100000000;
    }
}

int moorecoinunits::decimals(int unit)
{
    switch(unit)
    {
    case btc: return 8;
    case mbtc: return 5;
    case ubtc: return 2;
    default: return 0;
    }
}

qstring moorecoinunits::format(int unit, const camount& nin, bool fplus, separatorstyle separators)
{
    // note: not using straight sprintf here because we do not want
    // localized number formatting.
    if(!valid(unit))
        return qstring(); // refuse to format invalid unit
    qint64 n = (qint64)nin;
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    qstring quotient_str = qstring::number(quotient);
    qstring remainder_str = qstring::number(remainder).rightjustified(num_decimals, '0');

    // use si-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    qchar thin_sp(thin_sp_cp);
    int q_size = quotient_str.size();
    if (separators == separatoralways || (separators == separatorstandard && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fplus && n > 0)
        quotient_str.insert(0, '+');
    return quotient_str + qstring(".") + remainder_str;
}


// todo: review all remaining calls to moorecoinunits::formatwithunit to
// todo: determine whether the output is used in a plain text context
// todo: or an html context (and replace with
// todo: btcoinunits::formathtmlwithunit in the latter case). hopefully
// todo: there aren't instances where the result could be used in
// todo: either context.

// note: using formatwithunit in an html context risks wrapping
// quantities at the thousands separator. more subtly, it also results
// in a standard space rather than a thin space, due to a bug in qt's
// xml whitespace canonicalisation
//
// please take care to use formathtmlwithunit instead, when
// appropriate.

qstring moorecoinunits::formatwithunit(int unit, const camount& amount, bool plussign, separatorstyle separators)
{
    return format(unit, amount, plussign, separators) + qstring(" ") + name(unit);
}

qstring moorecoinunits::formathtmlwithunit(int unit, const camount& amount, bool plussign, separatorstyle separators)
{
    qstring str(formatwithunit(unit, amount, plussign, separators));
    str.replace(qchar(thin_sp_cp), qstring(thin_sp_html));
    return qstring("<span style='white-space: nowrap;'>%1</span>").arg(str);
}


bool moorecoinunits::parse(int unit, const qstring &value, camount *val_out)
{
    if(!valid(unit) || value.isempty())
        return false; // refuse to parse invalid unit or empty string
    int num_decimals = decimals(unit);

    // ignore spaces and thin spaces when parsing
    qstringlist parts = removespaces(value).split(".");

    if(parts.size() > 2)
    {
        return false; // more than one dot
    }
    qstring whole = parts[0];
    qstring decimals;

    if(parts.size() > 1)
    {
        decimals = parts[1];
    }
    if(decimals.size() > num_decimals)
    {
        return false; // exceeds max precision
    }
    bool ok = false;
    qstring str = whole + decimals.leftjustified(num_decimals, '0');

    if(str.size() > 18)
    {
        return false; // longer numbers will exceed 63 bits
    }
    camount retvalue(str.tolonglong(&ok));
    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

qstring moorecoinunits::getamountcolumntitle(int unit)
{
    qstring amounttitle = qobject::tr("amount");
    if (moorecoinunits::valid(unit))
    {
        amounttitle += " ("+moorecoinunits::name(unit) + ")";
    }
    return amounttitle;
}

int moorecoinunits::rowcount(const qmodelindex &parent) const
{
    q_unused(parent);
    return unitlist.size();
}

qvariant moorecoinunits::data(const qmodelindex &index, int role) const
{
    int row = index.row();
    if(row >= 0 && row < unitlist.size())
    {
        unit unit = unitlist.at(row);
        switch(role)
        {
        case qt::editrole:
        case qt::displayrole:
            return qvariant(name(unit));
        case qt::tooltiprole:
            return qvariant(description(unit));
        case unitrole:
            return qvariant(static_cast<int>(unit));
        }
    }
    return qvariant();
}

camount moorecoinunits::maxmoney()
{
    return max_money;
}
