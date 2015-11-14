#!/usr/bin/env python
'''
run this script inside of src/ and it will look for all the files
that were changed this year that still have the last year in the
copyright headers, and it will fix the headers on that file using
a perl regex one liner.

for example: if it finds something like this and we're in 2014

// copyright (c) 2009-2013 the moorecoin core developers

it will change it to

// copyright (c) 2009-2014 the moorecoin core developers

it will do this for all the files in the folder and its children.

author: @gubatron
'''
import os
import time

year = time.gmtime()[0]
last_year = year - 1
command = "perl -pi -e 's/%s the moorecoin/%s the moorecoin/' %s"
listfilescommand = "find . | grep %s"

extensions = [".cpp",".h"]

def getlastgitmodifieddate(filepath):
  gitgetlastcommitdatecommand = "git log " + filepath +" | grep date | head -n 1"
  p = os.popen(gitgetlastcommitdatecommand)
  result = ""
  for l in p:
    result = l
    break
  result = result.replace("\n","")
  return result

n=1
for extension in extensions:
  foundfiles = os.popen(listfilescommand % extension)
  for filepath in foundfiles:
    filepath = filepath[1:-1]
    if filepath.endswith(extension):
      filepath = os.getcwd() + filepath
      modifiedtime = getlastgitmodifieddate(filepath)
      if len(modifiedtime) > 0 and str(year) in modifiedtime:
        print n,"last git modified: ", modifiedtime, " - ", filepath
        os.popen(command % (last_year,year,filepath))
        n = n + 1


