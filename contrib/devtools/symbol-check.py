#!/usr/bin/python
# copyright (c) 2014 wladimir j. van der laan
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
'''
a script to check that the (linux) executables produced by gitian only contain
allowed gcc, glibc and libstdc++ version symbols.  this makes sure they are
still compatible with the minimum supported linux distribution versions.

example usage:

    find ../gitian-builder/build -type f -executable | xargs python contrib/devtools/symbol-check.py
'''
from __future__ import division, print_function
import subprocess
import re
import sys

# debian 6.0.9 (squeeze) has:
#
# - g++ version 4.4.5 (https://packages.debian.org/search?suite=default&section=all&arch=any&searchon=names&keywords=g%2b%2b)
# - libc version 2.11.3 (https://packages.debian.org/search?suite=default&section=all&arch=any&searchon=names&keywords=libc6)
# - libstdc++ version 4.4.5 (https://packages.debian.org/search?suite=default&section=all&arch=any&searchon=names&keywords=libstdc%2b%2b6)
#
# ubuntu 10.04.4 (lucid lynx) has:
#
# - g++ version 4.4.3 (http://packages.ubuntu.com/search?keywords=g%2b%2b&searchon=names&suite=lucid&section=all)
# - libc version 2.11.1 (http://packages.ubuntu.com/search?keywords=libc6&searchon=names&suite=lucid&section=all)
# - libstdc++ version 4.4.3 (http://packages.ubuntu.com/search?suite=lucid&section=all&arch=any&keywords=libstdc%2b%2b&searchon=names)
#
# taking the minimum of these as our target.
#
# according to gnu abi document (http://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html) this corresponds to:
#   gcc 4.4.0: gcc_4.4.0
#   gcc 4.4.2: glibcxx_3.4.13, cxxabi_1.3.3
#   (glibc)    glibc_2_11
#
max_versions = {
'gcc':     (4,4,0),
'cxxabi':  (1,3,3),
'glibcxx': (3,4,13),
'glibc':   (2,11)
}
# ignore symbols that are exported as part of every executable
ignore_exports = {
'_edata', '_end', '_init', '__bss_start', '_fini'
}
readelf_cmd = '/usr/bin/readelf'
cppfilt_cmd = '/usr/bin/c++filt'

class cppfilt(object):
    '''
    demangle c++ symbol names.

    use a pipe to the 'c++filt' command.
    '''
    def __init__(self):
        self.proc = subprocess.popen(cppfilt_cmd, stdin=subprocess.pipe, stdout=subprocess.pipe)

    def __call__(self, mangled):
        self.proc.stdin.write(mangled + '\n')
        return self.proc.stdout.readline().rstrip()

    def close(self):
        self.proc.stdin.close()
        self.proc.stdout.close()
        self.proc.wait()

def read_symbols(executable, imports=true):
    '''
    parse an elf executable and return a list of (symbol,version) tuples
    for dynamic, imported symbols.
    '''
    p = subprocess.popen([readelf_cmd, '--dyn-syms', '-w', executable], stdout=subprocess.pipe, stderr=subprocess.pipe, stdin=subprocess.pipe)
    (stdout, stderr) = p.communicate()
    if p.returncode:
        raise ioerror('could not read symbols for %s: %s' % (executable, stderr.strip()))
    syms = []
    for line in stdout.split('\n'):
        line = line.split()
        if len(line)>7 and re.match('[0-9]+:$', line[0]):
            (sym, _, version) = line[7].partition('@')
            is_import = line[6] == 'und'
            if version.startswith('@'):
                version = version[1:]
            if is_import == imports:
                syms.append((sym, version))
    return syms

def check_version(max_versions, version):
    if '_' in version:
        (lib, _, ver) = version.rpartition('_')
    else:
        lib = version
        ver = '0'
    ver = tuple([int(x) for x in ver.split('.')])
    if not lib in max_versions:
        return false
    return ver <= max_versions[lib]

if __name__ == '__main__':
    cppfilt = cppfilt()
    retval = 0
    for filename in sys.argv[1:]:
        # check imported symbols
        for sym,version in read_symbols(filename, true):
            if version and not check_version(max_versions, version):
                print('%s: symbol %s from unsupported version %s' % (filename, cppfilt(sym), version))
                retval = 1
        # check exported symbols
        for sym,version in read_symbols(filename, false):
            if sym in ignore_exports:
                continue
            print('%s: export of symbol %s not allowed' % (filename, cppfilt(sym)))
            retval = 1

    exit(retval)


