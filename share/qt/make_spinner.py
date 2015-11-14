#!/usr/bin/env python
# w.j. van der laan, 2011
# make spinning animation from a .png
# requires imagemagick 6.7+
from __future__ import division
from os import path
from pil import image
from subprocess import popen

src='img/reload.png'
tmpdir='../../src/qt/res/movies/'
tmpname='spinner-%03i.png'
numframes=35
framerate=10.0
convert='convert'
clockwise=true
dsize=(16,16)

im_src = image.open(src)

if clockwise:
    im_src = im_src.transpose(image.flip_left_right)

def frame_to_filename(frame):
    return path.join(tmpdir, tmpname % frame)

frame_files = []
for frame in xrange(numframes):
    rotation = (frame + 0.5) / numframes * 360.0
    if clockwise:
        rotation = -rotation
    im_new = im_src.rotate(rotation, image.bicubic)
    im_new.thumbnail(dsize, image.antialias)
    outfile = frame_to_filename(frame)
    im_new.save(outfile, 'png')
    frame_files.append(outfile)


