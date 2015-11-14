dist_subdirs = secp256k1
am_ldflags = $(pthread_cflags) $(libtool_ldflags)


if embedded_leveldb
leveldb_cppflags += -i$(srcdir)/leveldb/include
leveldb_cppflags += -i$(srcdir)/leveldb/helpers/memenv
libleveldb += $(builddir)/leveldb/libleveldb.a
libmemenv  += $(builddir)/leveldb/libmemenv.a

# note: this dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the leveldb build system in a race
$(libleveldb): $(libmemenv)

$(libleveldb) $(libmemenv):
	@echo "building leveldb ..." && $(make) -c $(@d) $(@f) cxx="$(cxx)" \
	  cc="$(cc)" platform=$(target_os) ar="$(ar)" $(leveldb_target_flags) \
          opt="$(cxxflags) $(cppflags) -d__stdc_limit_macros"
endif

bitcoin_config_includes=-i$(builddir)/config
bitcoin_includes=-i$(builddir) -i$(builddir)/obj $(boost_cppflags) $(leveldb_cppflags) $(crypto_cflags) $(ssl_cflags)

bitcoin_includes += -i$(srcdir)/secp256k1/include

libbitcoin_server=libbitcoin_server.a
libbitcoin_wallet=libbitcoin_wallet.a
libbitcoin_common=libbitcoin_common.a
libbitcoin_cli=libbitcoin_cli.a
libbitcoin_util=libbitcoin_util.a
libbitcoin_crypto=crypto/libbitcoin_crypto.a
libbitcoin_univalue=univalue/libbitcoin_univalue.a
libbitcoinqt=qt/libbitcoinqt.a
libsecp256k1=secp256k1/libsecp256k1.la

$(libsecp256k1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(am_v_at)$(make) $(am_makeflags) -c $(@d) $(@f)

# make is not made aware of per-object dependencies to avoid limiting building parallelization
# but to build the less dependent modules first, we manually select their order here:
extra_libraries = \
  crypto/libbitcoin_crypto.a \
  libbitcoin_util.a \
  libbitcoin_common.a \
  univalue/libbitcoin_univalue.a \
  libbitcoin_server.a \
  libbitcoin_cli.a
if enable_wallet
bitcoin_includes += $(bdb_cppflags)
extra_libraries += libbitcoin_wallet.a
endif

if build_bitcoin_libs
lib_ltlibraries = libbitcoinconsensus.la
libbitcoin_consensus=libbitcoinconsensus.la
else
libbitcoin_consensus=
endif

bin_programs =
tests =

if build_bitcoind
  bin_programs += bitcoind
endif

if build_bitcoin_utils
  bin_programs += bitcoin-cli bitcoin-tx
endif

.phony: force
# bitcoin core #
bitcoin_core_h = \
  addrman.h \
  alert.h \
  amount.h \
  arith_uint256.h \
  base58.h \
  bloom.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/params.h \
  consensus/validation.h \
  core_io.h \
  eccryptoverify.h \
  ecwrapper.h \
  hash.h \
  init.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  net.h \
  netbase.h \
  noui.h \
  policy/fees.h \
  pow.h \
  primitives/block.h \
  primitives/transaction.h \
  protocol.h \
  pubkey.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  scheduler.h \
  script/interpreter.h \
  script/script.h \
  script/script_error.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  serialize.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/pagelocker.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  utilmoneystr.h \
  utilstrencodings.h \
  utiltime.h \
  validationinterface.h \
  version.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/wallet.h \
  wallet/wallet_ismine.h \
  wallet/walletdb.h

obj/build.h: force
	@$(mkdir_p) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libbitcoin_util_a-clientversion.$(objext): obj/build.h

# server: shared between bitcoind and bitcoin-qt
libbitcoin_server_a_cppflags = $(bitcoin_includes) $(miniupnpc_cppflags)
libbitcoin_server_a_sources = \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  policy/fees.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  script/sigcache.cpp \
  timedata.cpp \
  txdb.cpp \
  txmempool.cpp \
  validationinterface.cpp \
  $(bitcoin_core_h)

# wallet: shared between bitcoind and bitcoin-qt, but only linked
# when wallet enabled
libbitcoin_wallet_a_cppflags = $(bitcoin_includes)
libbitcoin_wallet_a_sources = \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/walletdb.cpp \
  $(bitcoin_core_h)

# crypto primitives library
crypto_libbitcoin_crypto_a_cppflags = $(bitcoin_config_includes)
crypto_libbitcoin_crypto_a_sources = \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h

# univalue json library
univalue_libbitcoin_univalue_a_sources = \
  univalue/univalue.cpp \
  univalue/univalue.h \
  univalue/univalue_escapes.h \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp

# common: shared between bitcoind, and bitcoin-qt and non-server tools
libbitcoin_common_a_cppflags = $(bitcoin_includes)
libbitcoin_common_a_sources = \
  amount.cpp \
  arith_uint256.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  primitives/block.cpp \
  primitives/transaction.cpp \
  protocol.cpp \
  pubkey.cpp \
  scheduler.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/script_error.cpp \
  script/sign.cpp \
  script/standard.cpp \
  $(bitcoin_core_h)

# util: shared between all executables.
# this library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libbitcoin_util_a_cppflags = $(bitcoin_includes)
libbitcoin_util_a_sources = \
  support/pagelocker.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  random.cpp \
  rpcprotocol.cpp \
  support/cleanse.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(bitcoin_core_h)

if glibc_back_compat
libbitcoin_util_a_sources += compat/glibc_compat.cpp
endif

# cli: shared between bitcoin-cli and bitcoin-qt
libbitcoin_cli_a_cppflags = $(bitcoin_includes)
libbitcoin_cli_a_sources = \
  rpcclient.cpp \
  $(bitcoin_core_h)

nodist_libbitcoin_util_a_sources = $(srcdir)/obj/build.h
#

# bitcoind binary #
bitcoind_sources = bitcoind.cpp
bitcoind_cppflags = $(bitcoin_includes)
bitcoind_ldflags = $(reldflags) $(am_ldflags) $(libtool_app_ldflags)

if target_windows
bitcoind_sources += bitcoind-res.rc
endif

bitcoind_ldadd = \
  $(libbitcoin_server) \
  $(libbitcoin_common) \
  $(libbitcoin_univalue) \
  $(libbitcoin_util) \
  $(libbitcoin_crypto) \
  $(libleveldb) \
  $(libmemenv) \
  $(libsecp256k1)

if enable_wallet
bitcoind_ldadd += libbitcoin_wallet.a
endif

bitcoind_ldadd += $(boost_libs) $(bdb_libs) $(ssl_libs) $(crypto_libs) $(miniupnpc_libs)
#

# bitcoin-cli binary #
bitcoin_cli_sources = bitcoin-cli.cpp
bitcoin_cli_cppflags = $(bitcoin_includes)
bitcoin_cli_ldflags = $(reldflags) $(am_ldflags) $(libtool_app_ldflags)

if target_windows
bitcoin_cli_sources += bitcoin-cli-res.rc
endif

bitcoin_cli_ldadd = \
  $(libbitcoin_cli) \
  $(libbitcoin_univalue) \
  $(libbitcoin_util) \
  $(libsecp256k1)

bitcoin_cli_ldadd += $(boost_libs) $(ssl_libs) $(crypto_libs)
#

# bitcoin-tx binary #
bitcoin_tx_sources = bitcoin-tx.cpp
bitcoin_tx_cppflags = $(bitcoin_includes)
bitcoin_tx_ldflags = $(reldflags) $(am_ldflags) $(libtool_app_ldflags)

if target_windows
bitcoin_tx_sources += bitcoin-tx-res.rc
endif

bitcoin_tx_ldadd = \
  $(libbitcoin_univalue) \
  $(libbitcoin_common) \
  $(libbitcoin_util) \
  $(libbitcoin_crypto) \
  $(libsecp256k1)

bitcoin_tx_ldadd += $(boost_libs) $(crypto_libs)
#

# bitcoinconsensus library #
if build_bitcoin_libs
include_headers = script/bitcoinconsensus.h
libbitcoinconsensus_la_sources = \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  primitives/transaction.cpp \
  pubkey.cpp \
  script/bitcoinconsensus.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if glibc_back_compat
  libbitcoinconsensus_la_sources += compat/glibc_compat.cpp
endif

libbitcoinconsensus_la_ldflags = -no-undefined $(reldflags)
libbitcoinconsensus_la_libadd = $(crypto_libs)
libbitcoinconsensus_la_cppflags = $(crypto_cflags) -i$(builddir)/obj -dbuild_bitcoin_internal

endif
#

cleanfiles = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

distcleanfiles = obj/build.h

extra_dist = leveldb

clean-local:
	-$(make) -c leveldb clean
	-$(make) -c secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(windres)
	$(am_v_gen) $(windres) -dwindres_preproc -i $< -o $@

.mm.o:
	$(am_v_cxx) $(objcxx) $(defs) $(default_includes) $(includes) $(am_cppflags) \
	  $(cppflags) $(am_cxxflags) $(qt_includes) $(cxxflags)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(protoc)
	$(am_v_gen) $(protoc) --cpp_out=$(@d) --proto_path=$(abspath $(<d) $<)

if enable_tests
include makefile.test.include
endif

if enable_qt
include makefile.qt.include
endif

if enable_qt_tests
include makefile.qttest.include
endif
