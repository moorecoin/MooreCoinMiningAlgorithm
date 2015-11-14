#!/bin/sh
set -e

rootdir=dist
bundle="${rootdir}/bitcoin-qt.app"
codesign=codesign
tempdir=sign.temp
templist=${tempdir}/signatures.txt
out=signature.tar.gz
outroot=osx

if [ ! -n "$1" ]; then
  echo "usage: $0 <codesign args>"
  echo "example: $0 -s myidentity"
  exit 1
fi

rm -rf ${tempdir} ${templist}
mkdir -p ${tempdir}

${codesign} -f --file-list ${templist} "$@" "${bundle}"

grep -v coderesources < "${templist}" | while read i; do
  targetfile="${bundle}/`echo "${i}" | sed "s|.*${bundle}/||"`"
  size=`pagestuff "$i" -p | tail -2 | grep size | sed 's/[^0-9]*//g'`
  offset=`pagestuff "$i" -p | tail -2 | grep offset | sed 's/[^0-9]*//g'`
  signfile="${tempdir}/${outroot}/${targetfile}.sign"
  dirname="`dirname "${signfile}"`"
  mkdir -p "${dirname}"
  echo "adding detached signature for: ${targetfile}. size: ${size}. offset: ${offset}"
  dd if="$i" of="${signfile}" bs=1 skip=${offset} count=${size} 2>/dev/null
done

grep coderesources < "${templist}" | while read i; do
  targetfile="${bundle}/`echo "${i}" | sed "s|.*${bundle}/||"`"
  resource="${tempdir}/${outroot}/${targetfile}"
  dirname="`dirname "${resource}"`"
  mkdir -p "${dirname}"
  echo "adding resource for: "${targetfile}""
  cp "${i}" "${resource}"
done

rm ${templist}

tar -c "${tempdir}" -czf "${out}" .
rm -rf "${tempdir}"
echo "created ${out}"
