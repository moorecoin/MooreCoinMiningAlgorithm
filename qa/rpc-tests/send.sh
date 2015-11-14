#!/bin/bash
# copyright (c) 2014 the bitcoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
timeout=10
signal=hup
pidfile=.send.pid
if [ $# -eq 0 ]; then
  echo -e "usage:\t$0 <cmd>"
  echo -e "\truns <cmd> and wait ${timeout} seconds or until sig${signal} is received."
  echo -e "\treturns: 0 if sig${signal} is received, 1 otherwise."
  echo -e "or:\t$0 -stop"
  echo -e "\tsends sig${signal} to running send.sh"
  exit 0
fi

if [ $1 = "-stop" ]; then
  if [ -s ${pidfile} ]; then
      kill -s ${signal} $(<$pidfile 2>/dev/null) 2>/dev/null
  fi
  exit 0
fi

trap '[[ ${pid} ]] && kill ${pid}' ${signal}
trap 'rm -f ${pidfile}' exit
echo $$ > ${pidfile}
"$@"
sleep ${timeout} & pid=$!
wait ${pid} && exit 1

exit 0
