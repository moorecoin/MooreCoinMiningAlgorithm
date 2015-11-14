#!/bin/bash

###   this script attempts to download the signature file sha256sums.asc from bitcoin.org
###   it first checks if the signature passes, and then downloads the files specified in
###   the file, and checks if the hashes of these files match those that are specified
###   in the signature file.
###   the script returns 0 if everything passes the checks. it returns 1 if either the
###   signature check or the hash check doesn't pass. if an error occurs the return value is 2

function clean_up {
   for file in $*
   do
      rm "$file" 2> /dev/null
   done
}

workingdir="/tmp/bitcoin"
tmpfile="hashes.tmp"

#this url is used if a version number is not specified as an argument to the script
signaturefile="https://bitcoin.org/bin/0.9.2.1/sha256sums.asc"

signaturefilename="sha256sums.asc"
rcsubdir="test/"
basedir="https://bitcoin.org/bin/"
versionprefix="bitcoin-"
rcversionstring="rc"

if [ ! -d "$workingdir" ]; then
   mkdir "$workingdir"
fi

cd "$workingdir"

#test if a version number has been passed as an argument
if [ -n "$1" ]; then
   #let's also check if the version number includes the prefix 'bitcoin-',
   #  and add this prefix if it doesn't
   if [[ $1 == "$versionprefix"* ]]; then
      version="$1"
   else
      version="$versionprefix$1"
   fi

   #now let's see if the version string contains "rc", and strip it off if it does
   #  and simultaneously add rcsubdir to basedir, where we will look for signaturefilename
   if [[ $version == *"$rcversionstring"* ]]; then
      basedir="$basedir${version/%-$rcversionstring*}/"
      basedir="$basedir$rcsubdir"
   else
      basedir="$basedir$version/"
   fi

   signaturefile="$basedir$signaturefilename"
else
   basedir="${signaturefile%/*}/"
fi

#first we fetch the file containing the signature
wgetout=$(wget -n "$basedir$signaturefilename" 2>&1)

#and then see if wget completed successfully
if [ $? -ne 0 ]; then
   echo "error: couldn't fetch signature file. have you specified the version number in the following format?"
   echo "[bitcoin-]<version>-[rc[0-9]] (example: bitcoin-0.9.2-rc1)"
   echo "wget output:"
   echo "$wgetout"|sed 's/^/\t/g'
   exit 2
fi

#then we check it
gpgout=$(gpg --yes --decrypt --output "$tmpfile" "$signaturefilename" 2>&1)

#return value 0: good signature
#return value 1: bad signature
#return value 2: gpg error

ret="$?"
if [ $ret -ne 0 ]; then
   if [ $ret -eq 1 ]; then
      #and notify the user if it's bad
      echo "bad signature."
   elif [ $ret -eq 2 ]; then
      #or if a gpg error has occurred
      echo "gpg error. do you have gavin's code signing key installed?"
   fi

   echo "gpg output:"
   echo "$gpgout"|sed 's/^/\t/g'
   clean_up $signaturefilename $tmpfile
   exit "$ret"
fi

#here we extract the filenames from the signature file
files=$(awk '{print $2}' "$tmpfile")

#and download these one by one
for file in in $files
do
   wget --quiet -n "$basedir$file"
done

#check hashes
diff=$(diff <(sha256sum $files) "$tmpfile")

if [ $? -eq 1 ]; then
   echo "hashes don't match."
   echo "offending files:"
   echo "$diff"|grep "^<"|awk '{print "\t"$3}'
   exit 1
elif [ $? -gt 1 ]; then
   echo "error executing 'diff'"
   exit 2   
fi

#everything matches! clean up the mess
clean_up $files $signaturefilename $tmpfile

exit 0
