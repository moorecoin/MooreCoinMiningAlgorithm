#!/bin/bash

if [ -d "$1" ]; then
  cd "$1"
else
  echo "usage: $0 <datadir>" >&2
  echo "removes obsolete bitcoin database files" >&2
  exit 1
fi

level=0
if [ -f wallet.dat -a -f addr.dat -a -f blkindex.dat -a -f blk0001.dat ]; then level=1; fi
if [ -f wallet.dat -a -f peers.dat -a -f blkindex.dat -a -f blk0001.dat ]; then level=2; fi
if [ -f wallet.dat -a -f peers.dat -a -f coins/current -a -f blktree/current -a -f blocks/blk00000.dat ]; then level=3; fi
if [ -f wallet.dat -a -f peers.dat -a -f chainstate/current -a -f blocks/index/current -a -f blocks/blk00000.dat ]; then level=4; fi

case $level in
  0)
    echo "error: no bitcoin datadir detected."
    exit 1
    ;;
  1)
    echo "detected old bitcoin datadir (before 0.7)."
    echo "nothing to do."
    exit 0
    ;;
  2)
    echo "detected bitcoin 0.7 datadir."
    ;;
  3)
    echo "detected bitcoin pre-0.8 datadir."
    ;;
  4)
    echo "detected bitcoin 0.8 datadir."
    ;;
esac

files=""
dirs=""

if [ $level -ge 3 ]; then files=$(echo $files blk????.dat blkindex.dat); fi
if [ $level -ge 2 ]; then files=$(echo $files addr.dat); fi
if [ $level -ge 4 ]; then dirs=$(echo $dirs coins blktree); fi

for file in $files; do
  if [ -f $file ]; then
    echo "deleting: $file"
    rm -f $file
  fi
done

for dir in $dirs; do
  if [ -d $dir ]; then
    echo "deleting: $dir/"
    rm -rf $dir
  fi
done

echo "done."
