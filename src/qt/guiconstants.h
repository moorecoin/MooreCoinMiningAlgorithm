// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_guiconstants_h
#define moorecoin_qt_guiconstants_h

/* milliseconds between model updates */
static const int model_update_delay = 250;

/* askpassphrasedialog -- maximum passphrase length */
static const int max_passphrase_size = 1024;

/* moorecoingui -- size of icons in status bar */
static const int statusbar_iconsize = 16;

/* invalid field background style */
#define style_invalid "background:#ff8080"

/* transaction list -- unconfirmed transaction */
#define color_unconfirmed qcolor(128, 128, 128)
/* transaction list -- negative amount */
#define color_negative qcolor(255, 0, 0)
/* transaction list -- bare address (without label) */
#define color_bareaddress qcolor(140, 140, 140)
/* transaction list -- tx status decoration - open until date */
#define color_tx_status_openuntildate qcolor(64, 64, 255)
/* transaction list -- tx status decoration - offline */
#define color_tx_status_offline qcolor(192, 192, 192)
/* transaction list -- tx status decoration - default color */
#define color_black qcolor(0, 0, 0)

/* tooltips longer than this (in characters) are converted into rich text,
   so that they can be word-wrapped.
 */
static const int tooltip_wrap_threshold = 80;

/* maximum allowed uri length */
static const int max_uri_length = 255;

/* qrcodedialog -- size of exported qr code image */
#define export_image_size 256

/* number of frames in spinner animation */
#define spinner_frames 35

#define qapp_org_name "moorecoin"
#define qapp_org_domain "moorecoin.org"
#define qapp_app_name_default "moorecoin-qt"
#define qapp_app_name_testnet "moorecoin-qt-testnet"

#endif // moorecoin_qt_guiconstants_h
