libsecp256k1
============

[![build status](https://travis-ci.org/bitcoin/secp256k1.svg?branch=master)](https://travis-ci.org/bitcoin/secp256k1)

optimized c library for ec operations on curve secp256k1.

this library is a work in progress and is being used to research best practices. use at your own risk.

features:
* secp256k1 ecdsa signing/verification and key generation.
* adding/multiplying private/public keys.
* serialization/parsing of private keys, public keys, signatures.
* constant time, constant memory access signing and pubkey generation.
* derandomized dsa (via rfc6979 or with a caller provided function.)
* very efficient implementation.

implementation details
----------------------

* general
  * no runtime heap allocation.
  * extensive testing infrastructure.
  * structured to facilitate review and analysis.
  * intended to be portable to any system with a c89 compiler and uint64_t support.
  * expose only higher level interfaces to minimize the api surface and improve application security. ("be difficult to use insecurely.")
* field operations
  * optimized implementation of arithmetic modulo the curve's field size (2^256 - 0x1000003d1).
    * using 5 52-bit limbs (including hand-optimized assembly for x86_64, by diederik huys).
    * using 10 26-bit limbs.
  * field inverses and square roots using a sliding window over blocks of 1s (by peter dettman).
* scalar operations
  * optimized implementation without data-dependent branches of arithmetic modulo the curve's order.
    * using 4 64-bit limbs (relying on __int128 support in the compiler).
    * using 8 32-bit limbs.
* group operations
  * point addition formula specifically simplified for the curve equation (y^2 = x^3 + 7).
  * use addition between points in jacobian and affine coordinates where possible.
  * use a unified addition/doubling formula where necessary to avoid data-dependent branches.
  * point/x comparison without a field inversion by comparison in the jacobian coordinate space.
* point multiplication for verification (a*p + b*g).
  * use wnaf notation for point multiplicands.
  * use a much larger window for multiples of g, using precomputed multiples.
  * use shamir's trick to do the multiplication with the public key and the generator simultaneously.
  * optionally (off by default) use secp256k1's efficiently-computable endomorphism to split the p multiplicand into 2 half-sized ones.
* point multiplication for signing
  * use a precomputed table of multiples of powers of 16 multiplied with the generator, so general multiplication becomes a series of additions.
  * access the table with branch-free conditional moves so memory access is uniform.
  * no data-dependent branches
  * the precomputed tables add and eventually subtract points for which no known scalar (private key) is known, preventing even an attacker with control over the private key used to control the data internally.

build steps
-----------

libsecp256k1 is built using autotools:

    $ ./autogen.sh
    $ ./configure
    $ make
    $ ./tests
    $ sudo make install  # optional
