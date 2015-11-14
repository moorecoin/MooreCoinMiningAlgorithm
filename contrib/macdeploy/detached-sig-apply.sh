#!/bin/sh
set -e

unsigned="$1"
signature="$2"
arch=x86_64
rootdir=dist
tempdir=signed.temp
outdir=signed-app

if [ -z "$unsigned" ]; then
  echo "usage: $0 <unsigned app> <signature>"
  exit 1
fi

if [ -z "$signature" ]; then
  echo "usage: $0 <unsigned app> <signature>"
  exit 1
fi

rm -rf ${tempdir} && mkdir -p ${tempdir}
tar -c ${tempdir} -xf ${unsigned}
cp -rf "${signature}"/* ${tempdir}

if [ -z "${pagestuff}" ]; then
  pagestuff=${tempdir}/pagestuff
fi

if [ -z "${codesign_allocate}" ]; then
  codesign_allocate=${tempdir}/codesign_allocate
fi

find ${tempdir} -name "*.sign" | while read i; do
  size=`stat -c %s "${i}"`
  target_file="`echo "${i}" | sed 's/\.sign$//'`"

  echo "allocating space for the signature of size ${size} in ${target_file}"
  ${codesign_allocate} -i "${target_file}" -a ${arch} ${size} -o "${i}.tmp"

  offset=`${pagestuff} "${i}.tmp" -p | tail -2 | grep offset | sed 's/[^0-9]*//g'`
  if [ -z ${quiet} ]; then
    echo "attaching signature at offset ${offset}"
  fi

  dd if="$i" of="${i}.tmp" bs=1 seek=${offset} count=${size} 2>/dev/null
  mv "${i}.tmp" "${target_file}"
  rm "${i}"
  echo "success."
done
mv ${tempdir}/${rootdir} ${outdir}
rm -rf ${tempdir}
echo "signed: ${outdir}"
