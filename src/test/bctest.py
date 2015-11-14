# copyright 2014 bitpay, inc.
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

import subprocess
import os
import json
import sys

def bctest(testdir, testobj, exeext):

	execprog = testobj['exec'] + exeext
	execargs = testobj['args']
	execrun = [execprog] + execargs
	stdincfg = none
	inputdata = none
	if "input" in testobj:
		filename = testdir + "/" + testobj['input']
		inputdata = open(filename).read()
		stdincfg = subprocess.pipe

	outputfn = none
	outputdata = none
	if "output_cmp" in testobj:
		outputfn = testobj['output_cmp']
		outputdata = open(testdir + "/" + outputfn).read()
	proc = subprocess.popen(execrun, stdin=stdincfg, stdout=subprocess.pipe, stderr=subprocess.pipe,universal_newlines=true)
	try:
		outs = proc.communicate(input=inputdata)
	except oserror:
		print("oserror, failed to execute " + execprog)
		sys.exit(1)

	if outputdata and (outs[0] != outputdata):
		print("output data mismatch for " + outputfn)
		sys.exit(1)

	wantrc = 0
	if "return_code" in testobj:
		wantrc = testobj['return_code']
	if proc.returncode != wantrc:
		print("return code mismatch for " + outputfn)
		sys.exit(1)

def bctester(testdir, input_basename, buildenv):
	input_filename = testdir + "/" + input_basename
	raw_data = open(input_filename).read()
	input_data = json.loads(raw_data)

	for testobj in input_data:
		bctest(testdir, testobj, buildenv.exeext)

	sys.exit(0)

