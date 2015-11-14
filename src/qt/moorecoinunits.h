// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_moorecoinunits_h
#define moorecoin_qt_moorecoinunits_h

#include "amount.h"

#include <qabstractlistmodel>
#include <qstring>

// u+2009 thin space = utf-8 e2 80 89
#define real_thin_sp_cp 0x2009
#define real_thin_sp_utf8 "\xe2\x80\x89"
#define real_thin_sp_html "&thinsp;"

// u+200a hair space = utf-8 e2 80 8a
#define hair_sp_cp 0x200a
#define hair_sp_utf8 "\xe2\x80\x8a"
#define hair_sp_html "&#8202;"

// u+2006 six-per-em space = utf-8 e2 80 86
#define sixperem_sp_cp 0x2006
#define sixperem_sp_utf8 "\xe2\x80\x86"
#define sixperem_sp_html "&#8198;"

// u+2007 figure space = utf-8 e2 80 87
#define figure_sp_cp 0x2007
#define figure_sp_utf8 "\xe2\x80\x87"
#define figure_sp_html "&#8199;"

// qmessagebox seems to have a bug whereby it doesn't display thin/hair spaces
// correctly.  workaround is to display a space in a small font.  if you
// change this, please test that it doesn't cause the parent span to start
// wrapping.
#define html_hack_sp "<span style='white-space: nowrap; font-size: 6pt'> </span>"

// define thin_sp_* variables to be our preferred type of thin space
#define thin_sp_cp   real_thin_sp_cp
#define thin_sp_utf8 real_thin_sp_utf8
#define thin_sp_html html_hack_sp

/** moorecoin unit definitions. encapsulates parsing and formatting
   and serves as list model for drop-down selection boxes.
*/
class moorecoinunits: public qabstractlistmodel
{
    q_object

public:
    explicit moorecoinunits(qobject *parent);

    /** moorecoin units.
      @note source: https://en.moorecoin.it/wiki/units . please add only sensible ones
     */
    enum unit
    {
        btc,
        mbtc,
        ubtc
    };

    enum separatorstyle
    {
        separatornever,
        separatorstandard,
        separatoralways
    };

    //! @name static api
    //! unit conversion and formatting
    ///@{

    //! get list of units, for drop-down box
    static qlist<unit> availableunits();
    //! is unit id valid?
    static bool valid(int unit);
    //! short name
    static qstring name(int unit);
    //! longer description
    static qstring description(int unit);
    //! number of satoshis (1e-8) per unit
    static qint64 factor(int unit);
    //! number of decimals left
    static int decimals(int unit);
    //! format as string
    static qstring format(int unit, const camount& amount, bool plussign=false, separatorstyle separators=separatorstandard);
    //! format as string (with unit)
    static qstring formatwithunit(int unit, const camount& amount, bool plussign=false, separatorstyle separators=separatorstandard);
    static qstring formathtmlwithunit(int unit, const camount& amount, bool plussign=false, separatorstyle separators=separatorstandard);
    //! parse string to coin amount
    static bool parse(int unit, const qstring &value, camount *val_out);
    //! gets title for amount column including current display unit if optionsmodel reference available */
    static qstring getamountcolumntitle(int unit);
    ///@}

    //! @name abstractlistmodel implementation
    //! list model for unit drop-down selection box.
    ///@{
    enum roleindex {
        /** unit identifier */
        unitrole = qt::userrole
    };
    int rowcount(const qmodelindex &parent) const;
    qvariant data(const qmodelindex &index, int role) const;
    ///@}

    static qstring removespaces(qstring text)
    {
        text.remove(' ');
        text.remove(qchar(thin_sp_cp));
#if (thin_sp_cp != real_thin_sp_cp)
        text.remove(qchar(real_thin_sp_cp));
#endif
        return text;
    }

    //! return maximum number of base units (satoshis)
    static camount maxmoney();

private:
    qlist<moorecoinunits::unit> unitlist;
};
typedef moorecoinunits::unit moorecoinunit;

#endif // moorecoin_qt_moorecoinunits_h
