#!/usr/bin/env python

import os
import sys
import subprocess
import hashlib
from pil import image

def file_hash(filename):
    '''return hash of raw file contents'''
    with open(filename, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()

def content_hash(filename):
    '''return hash of rgba contents of image'''
    i = image.open(filename)
    i = i.convert('rgba')
    data = i.tostring()
    return hashlib.sha256(data).hexdigest()

#optimize png, remove various color profiles, remove ancillary chunks (alla) and text chunks (text)
#pngcrush -brute -ow -rem gama -rem chrm -rem iccp -rem srgb -rem alla -rem text

pngcrush = 'pngcrush'
git = 'git'
folders = ["src/qt/res/movies", "src/qt/res/icons", "src/qt/res/images"]
basepath = subprocess.check_output([git, 'rev-parse', '--show-toplevel']).rstrip('\n')
totalsavebytes = 0

outputarray = []
for folder in folders:
    absfolder=os.path.join(basepath, folder)
    for file in os.listdir(absfolder):
        extension = os.path.splitext(file)[1]
        if extension.lower() == '.png':
            print("optimizing "+file+"..."),
            file_path = os.path.join(absfolder, file)
            filemetamap = {'file' : file, 'osize': os.path.getsize(file_path), 'sha256old' : file_hash(file_path)};
            filemetamap['contenthashpre'] = content_hash(file_path)
        
            pngcrushoutput = ""
            try:
                pngcrushoutput = subprocess.check_output(
                        [pngcrush, "-brute", "-ow", "-rem", "gama", "-rem", "chrm", "-rem", "iccp", "-rem", "srgb", "-rem", "alla", "-rem", "text", file_path],
                        stderr=subprocess.stdout).rstrip('\n')
            except:
                print "pngcrush is not installed, aborting..."
                sys.exit(0)
        
            #verify
            if "not a png file" in subprocess.check_output([pngcrush, "-n", "-v", file_path], stderr=subprocess.stdout):
                print "png file "+file+" is corrupted after crushing, check out pngcursh version"
                sys.exit(1)
            
            filemetamap['sha256new'] = file_hash(file_path)
            filemetamap['contenthashpost'] = content_hash(file_path)

            if filemetamap['contenthashpre'] != filemetamap['contenthashpost']:
                print "image contents of png file "+file+" before and after crushing don't match"
                sys.exit(1)

            filemetamap['psize'] = os.path.getsize(file_path)
            outputarray.append(filemetamap)
            print("done\n"),

print "summary:\n+++++++++++++++++"
for filedict in outputarray:
    oldhash = filedict['sha256old']
    newhash = filedict['sha256new']
    totalsavebytes += filedict['osize'] - filedict['psize']
    print filedict['file']+"\n  size diff from: "+str(filedict['osize'])+" to: "+str(filedict['psize'])+"\n  old sha256: "+oldhash+"\n  new sha256: "+newhash+"\n"
    
print "completed. total reduction: "+str(totalsavebytes)+" bytes"
