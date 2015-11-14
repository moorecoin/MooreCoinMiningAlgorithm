// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "moorecoinamountfield.h"

#include "moorecoinunits.h"
#include "guiconstants.h"
#include "qvaluecombobox.h"

#include <qapplication>
#include <qabstractspinbox>
#include <qhboxlayout>
#include <qkeyevent>
#include <qlineedit>

/** qspinbox that uses fixed-point numbers internally and uses our own
 * formatting/parsing functions.
 */
class amountspinbox: public qabstractspinbox
{
    q_object

public:
    explicit amountspinbox(qwidget *parent):
        qabstractspinbox(parent),
        currentunit(moorecoinunits::btc),
        singlestep(100000) // satoshis
    {
        setalignment(qt::alignright);

        connect(lineedit(), signal(textedited(qstring)), this, signal(valuechanged()));
    }

    qvalidator::state validate(qstring &text, int &pos) const
    {
        if(text.isempty())
            return qvalidator::intermediate;
        bool valid = false;
        parse(text, &valid);
        /* make sure we return intermediate so that fixup() is called on defocus */
        return valid ? qvalidator::intermediate : qvalidator::invalid;
    }

    void fixup(qstring &input) const
    {
        bool valid = false;
        camount val = parse(input, &valid);
        if(valid)
        {
            input = moorecoinunits::format(currentunit, val, false, moorecoinunits::separatoralways);
            lineedit()->settext(input);
        }
    }

    camount value(bool *valid_out=0) const
    {
        return parse(text(), valid_out);
    }

    void setvalue(const camount& value)
    {
        lineedit()->settext(moorecoinunits::format(currentunit, value, false, moorecoinunits::separatoralways));
        emit valuechanged();
    }

    void stepby(int steps)
    {
        bool valid = false;
        camount val = value(&valid);
        val = val + steps * singlestep;
        val = qmin(qmax(val, camount(0)), moorecoinunits::maxmoney());
        setvalue(val);
    }

    void setdisplayunit(int unit)
    {
        bool valid = false;
        camount val = value(&valid);

        currentunit = unit;

        if(valid)
            setvalue(val);
        else
            clear();
    }

    void setsinglestep(const camount& step)
    {
        singlestep = step;
    }

    qsize minimumsizehint() const
    {
        if(cachedminimumsizehint.isempty())
        {
            ensurepolished();

            const qfontmetrics fm(fontmetrics());
            int h = lineedit()->minimumsizehint().height();
            int w = fm.width(moorecoinunits::format(moorecoinunits::btc, moorecoinunits::maxmoney(), false, moorecoinunits::separatoralways));
            w += 2; // cursor blinking space

            qstyleoptionspinbox opt;
            initstyleoption(&opt);
            qsize hint(w, h);
            qsize extra(35, 6);
            opt.rect.setsize(hint + extra);
            extra += hint - style()->subcontrolrect(qstyle::cc_spinbox, &opt,
                                                    qstyle::sc_spinboxeditfield, this).size();
            // get closer to final result by repeating the calculation
            opt.rect.setsize(hint + extra);
            extra += hint - style()->subcontrolrect(qstyle::cc_spinbox, &opt,
                                                    qstyle::sc_spinboxeditfield, this).size();
            hint += extra;
            hint.setheight(h);

            opt.rect = rect();

            cachedminimumsizehint = style()->sizefromcontents(qstyle::ct_spinbox, &opt, hint, this)
                                    .expandedto(qapplication::globalstrut());
        }
        return cachedminimumsizehint;
    }

private:
    int currentunit;
    camount singlestep;
    mutable qsize cachedminimumsizehint;

    /**
     * parse a string into a number of base monetary units and
     * return validity.
     * @note must return 0 if !valid.
     */
    camount parse(const qstring &text, bool *valid_out=0) const
    {
        camount val = 0;
        bool valid = moorecoinunits::parse(currentunit, text, &val);
        if(valid)
        {
            if(val < 0 || val > moorecoinunits::maxmoney())
                valid = false;
        }
        if(valid_out)
            *valid_out = valid;
        return valid ? val : 0;
    }

protected:
    bool event(qevent *event)
    {
        if (event->type() == qevent::keypress || event->type() == qevent::keyrelease)
        {
            qkeyevent *keyevent = static_cast<qkeyevent *>(event);
            if (keyevent->key() == qt::key_comma)
            {
                // translate a comma into a period
                qkeyevent periodkeyevent(event->type(), qt::key_period, keyevent->modifiers(), ".", keyevent->isautorepeat(), keyevent->count());
                return qabstractspinbox::event(&periodkeyevent);
            }
        }
        return qabstractspinbox::event(event);
    }

    stepenabled stepenabled() const
    {
        if (isreadonly()) // disable steps when amountspinbox is read-only
            return stepnone;
        if (text().isempty()) // allow step-up with empty field
            return stepupenabled;

        stepenabled rv = 0;
        bool valid = false;
        camount val = value(&valid);
        if(valid)
        {
            if(val > 0)
                rv |= stepdownenabled;
            if(val < moorecoinunits::maxmoney())
                rv |= stepupenabled;
        }
        return rv;
    }

signals:
    void valuechanged();
};

#include "moorecoinamountfield.moc"

moorecoinamountfield::moorecoinamountfield(qwidget *parent) :
    qwidget(parent),
    amount(0)
{
    amount = new amountspinbox(this);
    amount->setlocale(qlocale::c());
    amount->installeventfilter(this);
    amount->setmaximumwidth(170);

    qhboxlayout *layout = new qhboxlayout(this);
    layout->addwidget(amount);
    unit = new qvaluecombobox(this);
    unit->setmodel(new moorecoinunits(this));
    layout->addwidget(unit);
    layout->addstretch(1);
    layout->setcontentsmargins(0,0,0,0);

    setlayout(layout);

    setfocuspolicy(qt::tabfocus);
    setfocusproxy(amount);

    // if one if the widgets changes, the combined content changes as well
    connect(amount, signal(valuechanged()), this, signal(valuechanged()));
    connect(unit, signal(currentindexchanged(int)), this, slot(unitchanged(int)));

    // set default based on configuration
    unitchanged(unit->currentindex());
}

void moorecoinamountfield::clear()
{
    amount->clear();
    unit->setcurrentindex(0);
}

void moorecoinamountfield::setenabled(bool fenabled)
{
    amount->setenabled(fenabled);
    unit->setenabled(fenabled);
}

bool moorecoinamountfield::validate()
{
    bool valid = false;
    value(&valid);
    setvalid(valid);
    return valid;
}

void moorecoinamountfield::setvalid(bool valid)
{
    if (valid)
        amount->setstylesheet("");
    else
        amount->setstylesheet(style_invalid);
}

bool moorecoinamountfield::eventfilter(qobject *object, qevent *event)
{
    if (event->type() == qevent::focusin)
    {
        // clear invalid flag on focus
        setvalid(true);
    }
    return qwidget::eventfilter(object, event);
}

qwidget *moorecoinamountfield::setuptabchain(qwidget *prev)
{
    qwidget::settaborder(prev, amount);
    qwidget::settaborder(amount, unit);
    return unit;
}

camount moorecoinamountfield::value(bool *valid_out) const
{
    return amount->value(valid_out);
}

void moorecoinamountfield::setvalue(const camount& value)
{
    amount->setvalue(value);
}

void moorecoinamountfield::setreadonly(bool freadonly)
{
    amount->setreadonly(freadonly);
}

void moorecoinamountfield::unitchanged(int idx)
{
    // use description tooltip for current unit for the combobox
    unit->settooltip(unit->itemdata(idx, qt::tooltiprole).tostring());

    // determine new unit id
    int newunit = unit->itemdata(idx, moorecoinunits::unitrole).toint();

    amount->setdisplayunit(newunit);
}

void moorecoinamountfield::setdisplayunit(int newunit)
{
    unit->setvalue(newunit);
}

void moorecoinamountfield::setsinglestep(const camount& step)
{
    amount->setsinglestep(step);
}
