contents
===========
this directory contains tools for developers working on this repository.

github-merge.sh
==================

a small script to automate merging pull-requests securely and sign them with gpg.

for example:

  ./github-merge.sh bitcoin/bitcoin 3077

(in any git repository) will help you merge pull request #3077 for the
bitcoin/bitcoin repository.

what it does:
* fetch master and the pull request.
* locally construct a merge commit.
* show the diff that merge results in.
* ask you to verify the resulting source tree (so you can do a make
check or whatever).
* ask you whether to gpg sign the merge commit.
* ask you whether to push the result upstream.

this means that there are no potential race conditions (where a
pullreq gets updated while you're reviewing it, but before you click
merge), and when using gpg signatures, that even a compromised github
couldn't mess with the sources.

setup
---------
configuring the github-merge tool for the bitcoin repository is done in the following way:

    git config githubmerge.repository bitcoin/bitcoin
    git config githubmerge.testcmd "make -j4 check" (adapt to whatever you want to use for testing)
    git config --global user.signingkey mykeyid (if you want to gpg sign)

fix-copyright-headers.py
===========================

every year newly updated files need to have its copyright headers updated to reflect the current year.
if you run this script from src/ it will automatically update the year on the copyright header for all
.cpp and .h files if these have a git commit from the current year.

for example a file changed in 2014 (with 2014 being the current year):
```// copyright (c) 2009-2013 the bitcoin core developers```

would be changed to:
```// copyright (c) 2009-2014 the bitcoin core developers```

symbol-check.py
==================

a script to check that the (linux) executables produced by gitian only contain
allowed gcc, glibc and libstdc++ version symbols.  this makes sure they are
still compatible with the minimum supported linux distribution versions.

example usage after a gitian build:

    find ../gitian-builder/build -type f -executable | xargs python contrib/devtools/symbol-check.py 

if only supported symbols are used the return value will be 0 and the output will be empty.

if there are 'unsupported' symbols, the return value will be 1 a list like this will be printed:

    .../64/test_bitcoin: symbol memcpy from unsupported version glibc_2.14
    .../64/test_bitcoin: symbol __fdelt_chk from unsupported version glibc_2.15
    .../64/test_bitcoin: symbol std::out_of_range::~out_of_range() from unsupported version glibcxx_3.4.15
    .../64/test_bitcoin: symbol _znst8__detail15_list_nod from unsupported version glibcxx_3.4.15

update-translations.py
=======================

run this script from the root of the repository to update all translations from transifex.
it will do the following automatically:

- fetch all translations
- post-process them into valid and committable format
- add missing translations to the build system (todo)

see doc/translation-process.md for more information.

git-subtree-check.sh
====================

run this script from the root of the repository to verify that a subtree matches the contents of
the commit it claims to have been updated to.

to use, make sure that you have fetched the upstream repository branch in which the subtree is
maintained:
* for src/secp256k1: https://github.com/bitcoin/secp256k1.git (branch master)
* for sec/leveldb: https://github.com/bitcoin/leveldb.git (branch bitcoin-fork)

usage: git-subtree-check.sh dir commit
commit may be omitted, in which case head is used.
