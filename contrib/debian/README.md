
debian
====================
this directory contains files used to package bitcoind/bitcoin-qt
for debian-based linux systems. if you compile bitcoind/bitcoin-qt yourself, there are some useful files here.

## bitcoin: uri support ##


bitcoin-qt.desktop  (gnome / open desktop)
to install:

	sudo desktop-file-install bitcoin-qt.desktop
	sudo update-desktop-database

if you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your bitcoin-qt binary to `/usr/bin`
and the `../../share/pixmaps/bitcoin128.png` to `/usr/share/pixmaps`

bitcoin-qt.protocol (kde)

