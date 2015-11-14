#!/usr/bin/python
#
# linearize-data.py: construct a linear, no-fork version of the chain.
#
# copyright (c) 2013-2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

from __future__ import print_function, division
import json
import struct
import re
import os
import base64
import httplib
import sys
import hashlib
import datetime
import time
from collections import namedtuple

settings = {}

def uint32(x):
	return x & 0xffffffffl

def bytereverse(x):
	return uint32(( ((x) << 24) | (((x) << 8) & 0x00ff0000) |
		       (((x) >> 8) & 0x0000ff00) | ((x) >> 24) ))

def bufreverse(in_buf):
	out_words = []
	for i in range(0, len(in_buf), 4):
		word = struct.unpack('@i', in_buf[i:i+4])[0]
		out_words.append(struct.pack('@i', bytereverse(word)))
	return ''.join(out_words)

def wordreverse(in_buf):
	out_words = []
	for i in range(0, len(in_buf), 4):
		out_words.append(in_buf[i:i+4])
	out_words.reverse()
	return ''.join(out_words)

def calc_hdr_hash(blk_hdr):
	hash1 = hashlib.sha256()
	hash1.update(blk_hdr)
	hash1_o = hash1.digest()

	hash2 = hashlib.sha256()
	hash2.update(hash1_o)
	hash2_o = hash2.digest()

	return hash2_o

def calc_hash_str(blk_hdr):
	hash = calc_hdr_hash(blk_hdr)
	hash = bufreverse(hash)
	hash = wordreverse(hash)
	hash_str = hash.encode('hex')
	return hash_str

def get_blk_dt(blk_hdr):
	members = struct.unpack("<i", blk_hdr[68:68+4])
	ntime = members[0]
	dt = datetime.datetime.fromtimestamp(ntime)
	dt_ym = datetime.datetime(dt.year, dt.month, 1)
	return (dt_ym, ntime)

def get_block_hashes(settings):
	blkindex = []
	f = open(settings['hashlist'], "r")
	for line in f:
		line = line.rstrip()
		blkindex.append(line)

	print("read " + str(len(blkindex)) + " hashes")

	return blkindex

def mkblockmap(blkindex):
	blkmap = {}
	for height,hash in enumerate(blkindex):
		blkmap[hash] = height
	return blkmap

# block header and extent on disk
blockextent = namedtuple('blockextent', ['fn', 'offset', 'inhdr', 'blkhdr', 'size'])

class blockdatacopier:
	def __init__(self, settings, blkindex, blkmap):
		self.settings = settings
		self.blkindex = blkindex
		self.blkmap = blkmap

		self.infn = 0
		self.inf = none
		self.outfn = 0
		self.outsz = 0
		self.outf = none
		self.outfname = none
		self.blkcountin = 0
		self.blkcountout = 0

		self.lastdate = datetime.datetime(2000, 1, 1)
		self.hights = 1408893517 - 315360000
		self.timestampsplit = false
		self.fileoutput = true
		self.setfiletime = false
		self.maxoutsz = settings['max_out_sz']
		if 'output' in settings:
			self.fileoutput = false
		if settings['file_timestamp'] != 0:
			self.setfiletime = true
		if settings['split_timestamp'] != 0:
			self.timestampsplit = true
        # extents and cache for out-of-order blocks
		self.blockextents = {}
		self.outoforderdata = {}
		self.outofordersize = 0 # running total size for items in outoforderdata

	def writeblock(self, inhdr, blk_hdr, rawblock):
		if not self.fileoutput and ((self.outsz + self.inlen) > self.maxoutsz):
			self.outf.close()
			if self.setfiletime:
				os.utime(outfname, (int(time.time()), hights))
			self.outf = none
			self.outfname = none
			self.outfn = outfn + 1
			self.outsz = 0

		(blkdate, blkts) = get_blk_dt(blk_hdr)
		if self.timestampsplit and (blkdate > self.lastdate):
			print("new month " + blkdate.strftime("%y-%m") + " @ " + hash_str)
			lastdate = blkdate
			if outf:
				outf.close()
				if setfiletime:
					os.utime(outfname, (int(time.time()), hights))
				self.outf = none
				self.outfname = none
				self.outfn = self.outfn + 1
				self.outsz = 0

		if not self.outf:
			if self.fileoutput:
				outfname = self.settings['output_file']
			else:
				outfname = "%s/blk%05d.dat" % (self.settings['output'], outfn)
			print("output file " + outfname)
			self.outf = open(outfname, "wb")

		self.outf.write(inhdr)
		self.outf.write(blk_hdr)
		self.outf.write(rawblock)
		self.outsz = self.outsz + len(inhdr) + len(blk_hdr) + len(rawblock)

		self.blkcountout = self.blkcountout + 1
		if blkts > self.hights:
			self.hights = blkts

		if (self.blkcountout % 1000) == 0:
			print('%i blocks scanned, %i blocks written (of %i, %.1f%% complete)' % 
					(self.blkcountin, self.blkcountout, len(self.blkindex), 100.0 * self.blkcountout / len(self.blkindex)))

	def infilename(self, fn):
		return "%s/blk%05d.dat" % (self.settings['input'], fn)

	def fetchblock(self, extent):
		'''fetch block contents from disk given extents'''
		with open(self.infilename(extent.fn), "rb") as f:
			f.seek(extent.offset)
			return f.read(extent.size)

	def copyoneblock(self):
		'''find the next block to be written in the input, and copy it to the output.'''
		extent = self.blockextents.pop(self.blkcountout)
		if self.blkcountout in self.outoforderdata:
			# if the data is cached, use it from memory and remove from the cache
			rawblock = self.outoforderdata.pop(self.blkcountout)
			self.outofordersize -= len(rawblock)
		else: # otherwise look up data on disk
			rawblock = self.fetchblock(extent)

		self.writeblock(extent.inhdr, extent.blkhdr, rawblock)

	def run(self):
		while self.blkcountout < len(self.blkindex):
			if not self.inf:
				fname = self.infilename(self.infn)
				print("input file " + fname)
				try:
					self.inf = open(fname, "rb")
				except ioerror:
					print("premature end of block data")
					return

			inhdr = self.inf.read(8)
			if (not inhdr or (inhdr[0] == "\0")):
				self.inf.close()
				self.inf = none
				self.infn = self.infn + 1
				continue

			inmagic = inhdr[:4]
			if (inmagic != self.settings['netmagic']):
				print("invalid magic: " + inmagic.encode('hex'))
				return
			inlenle = inhdr[4:]
			su = struct.unpack("<i", inlenle)
			inlen = su[0] - 80 # length without header
			blk_hdr = self.inf.read(80)
			inextent = blockextent(self.infn, self.inf.tell(), inhdr, blk_hdr, inlen)

			hash_str = calc_hash_str(blk_hdr)
			if not hash_str in blkmap:
				print("skipping unknown block " + hash_str)
				self.inf.seek(inlen, os.seek_cur)
				continue

			blkheight = self.blkmap[hash_str]
			self.blkcountin += 1

			if self.blkcountout == blkheight:
				# if in-order block, just copy
				rawblock = self.inf.read(inlen)
				self.writeblock(inhdr, blk_hdr, rawblock)

				# see if we can catch up to prior out-of-order blocks
				while self.blkcountout in self.blockextents:
					self.copyoneblock()

			else: # if out-of-order, skip over block data for now
				self.blockextents[blkheight] = inextent
				if self.outofordersize < self.settings['out_of_order_cache_sz']:
					# if there is space in the cache, read the data
					# reading the data in file sequence instead of seeking and fetching it later is preferred,
					# but we don't want to fill up memory
					self.outoforderdata[blkheight] = self.inf.read(inlen)
					self.outofordersize += inlen
				else: # if no space in cache, seek forward
					self.inf.seek(inlen, os.seek_cur)

		print("done (%i blocks written)" % (self.blkcountout))

if __name__ == '__main__':
	if len(sys.argv) != 2:
		print("usage: linearize-data.py config-file")
		sys.exit(1)

	f = open(sys.argv[1])
	for line in f:
		# skip comment lines
		m = re.search('^\s*#', line)
		if m:
			continue

		# parse key=value lines
		m = re.search('^(\w+)\s*=\s*(\s.*)$', line)
		if m is none:
			continue
		settings[m.group(1)] = m.group(2)
	f.close()

	if 'netmagic' not in settings:
		settings['netmagic'] = 'f9beb4d9'
	if 'genesis' not in settings:
		settings['genesis'] = '000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f'
	if 'input' not in settings:
		settings['input'] = 'input'
	if 'hashlist' not in settings:
		settings['hashlist'] = 'hashlist.txt'
	if 'file_timestamp' not in settings:
		settings['file_timestamp'] = 0
	if 'split_timestamp' not in settings:
		settings['split_timestamp'] = 0
	if 'max_out_sz' not in settings:
		settings['max_out_sz'] = 1000l * 1000 * 1000
	if 'out_of_order_cache_sz' not in settings:
		settings['out_of_order_cache_sz'] = 100 * 1000 * 1000

	settings['max_out_sz'] = long(settings['max_out_sz'])
	settings['split_timestamp'] = int(settings['split_timestamp'])
	settings['file_timestamp'] = int(settings['file_timestamp'])
	settings['netmagic'] = settings['netmagic'].decode('hex')
	settings['out_of_order_cache_sz'] = int(settings['out_of_order_cache_sz'])

	if 'output_file' not in settings and 'output' not in settings:
		print("missing output file / directory")
		sys.exit(1)

	blkindex = get_block_hashes(settings)
	blkmap = mkblockmap(blkindex)

	if not settings['genesis'] in blkmap:
		print("genesis block not found in hashlist")
	else:
		blockdatacopier(settings, blkindex, blkmap).run()


