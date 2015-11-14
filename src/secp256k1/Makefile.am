aclocal_amflags = -i build-aux/m4

lib_ltlibraries = libsecp256k1.la
include_headers = include/secp256k1.h
noinst_headers =
noinst_headers += src/scalar.h
noinst_headers += src/scalar_4x64.h
noinst_headers += src/scalar_8x32.h
noinst_headers += src/scalar_impl.h
noinst_headers += src/scalar_4x64_impl.h
noinst_headers += src/scalar_8x32_impl.h
noinst_headers += src/group.h
noinst_headers += src/group_impl.h
noinst_headers += src/num_gmp.h
noinst_headers += src/num_gmp_impl.h
noinst_headers += src/ecdsa.h
noinst_headers += src/ecdsa_impl.h
noinst_headers += src/eckey.h
noinst_headers += src/eckey_impl.h
noinst_headers += src/ecmult.h
noinst_headers += src/ecmult_impl.h
noinst_headers += src/ecmult_gen.h
noinst_headers += src/ecmult_gen_impl.h
noinst_headers += src/num.h
noinst_headers += src/num_impl.h
noinst_headers += src/field_10x26.h
noinst_headers += src/field_10x26_impl.h
noinst_headers += src/field_5x52.h
noinst_headers += src/field_5x52_impl.h
noinst_headers += src/field_5x52_int128_impl.h
noinst_headers += src/field_5x52_asm_impl.h
noinst_headers += src/java/org_bitcoin_nativesecp256k1.h
noinst_headers += src/util.h
noinst_headers += src/testrand.h
noinst_headers += src/testrand_impl.h
noinst_headers += src/hash.h
noinst_headers += src/hash_impl.h
noinst_headers += src/field.h
noinst_headers += src/field_impl.h
noinst_headers += src/bench.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_data = libsecp256k1.pc

libsecp256k1_la_sources = src/secp256k1.c
libsecp256k1_la_cppflags = -i$(top_srcdir)/include $(secp_includes)
libsecp256k1_la_libadd = $(secp_libs)


noinst_programs =
if use_benchmark
noinst_programs += bench_verify bench_recover bench_sign bench_internal
bench_verify_sources = src/bench_verify.c
bench_verify_ldadd = libsecp256k1.la $(secp_libs)
bench_verify_ldflags = -static
bench_recover_sources = src/bench_recover.c
bench_recover_ldadd = libsecp256k1.la $(secp_libs)
bench_recover_ldflags = -static
bench_sign_sources = src/bench_sign.c
bench_sign_ldadd = libsecp256k1.la $(secp_libs)
bench_sign_ldflags = -static
bench_internal_sources = src/bench_internal.c
bench_internal_ldadd = $(secp_libs)
bench_internal_ldflags = -static
bench_internal_cppflags = $(secp_includes)
endif

if use_tests
noinst_programs += tests
tests_sources = src/tests.c
tests_cppflags = -dverify $(secp_includes) $(secp_test_includes)
tests_ldadd = $(secp_libs) $(secp_test_libs)
tests_ldflags = -static
tests = tests
endif

extra_dist = autogen.sh
