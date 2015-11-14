#!/bin/bash
# create multiresolution windows icon
icon_src=../../src/qt/res/icons/bitcoin.png
icon_dst=../../src/qt/res/icons/bitcoin.ico
convert ${icon_src} -resize 16x16 bitcoin-16.png
convert ${icon_src} -resize 32x32 bitcoin-32.png
convert ${icon_src} -resize 48x48 bitcoin-48.png
convert bitcoin-16.png bitcoin-32.png bitcoin-48.png ${icon_dst}

