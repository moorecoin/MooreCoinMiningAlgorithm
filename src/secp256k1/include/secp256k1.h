#ifndef _secp256k1_
# define _secp256k1_

# ifdef __cplusplus
extern "c" {
# endif

# if !defined(secp256k1_gnuc_prereq)
#  if defined(__gnuc__)&&defined(__gnuc_minor__)
#   define secp256k1_gnuc_prereq(_maj,_min) \
 ((__gnuc__<<16)+__gnuc_minor__>=((_maj)<<16)+(_min))
#  else
#   define secp256k1_gnuc_prereq(_maj,_min) 0
#  endif
# endif

# if (!defined(__stdc_version__) || (__stdc_version__ < 199901l) )
#  if secp256k1_gnuc_prereq(2,7)
#   define secp256k1_inline __inline__
#  elif (defined(_msc_ver))
#   define secp256k1_inline __inline
#  else
#   define secp256k1_inline
#  endif
# else
#  define secp256k1_inline inline
# endif

/**warning attributes
  * nonnull is not used if secp256k1_build is set to avoid the compiler optimizing out
  * some paranoid null checks. */
# if defined(__gnuc__) && secp256k1_gnuc_prereq(3, 4)
#  define secp256k1_warn_unused_result __attribute__ ((__warn_unused_result__))
# else
#  define secp256k1_warn_unused_result
# endif
# if !defined(secp256k1_build) && defined(__gnuc__) && secp256k1_gnuc_prereq(3, 4)
#  define secp256k1_arg_nonnull(_x)  __attribute__ ((__nonnull__(_x)))
# else
#  define secp256k1_arg_nonnull(_x)
# endif

/** opaque data structure that holds context information (precomputed tables etc.).
 *  only functions that take a pointer to a non-const context require exclusive
 *  access to it. multiple functions that take a pointer to a const context may
 *  run simultaneously.
 */
typedef struct secp256k1_context_struct secp256k1_context_t;

/** flags to pass to secp256k1_context_create. */
# define secp256k1_context_verify (1 << 0)
# define secp256k1_context_sign   (1 << 1)

/** create a secp256k1 context object.
 *  returns: a newly created context object.
 *  in:      flags: which parts of the context to initialize.
 */
secp256k1_context_t* secp256k1_context_create(
  int flags
) secp256k1_warn_unused_result;

/** copies a secp256k1 context object.
 *  returns: a newly created context object.
 *  in:      ctx: an existing context to copy
 */
secp256k1_context_t* secp256k1_context_clone(
  const secp256k1_context_t* ctx
) secp256k1_warn_unused_result;

/** destroy a secp256k1 context object.
 *  the context pointer may not be used afterwards.
 */
void secp256k1_context_destroy(
  secp256k1_context_t* ctx
) secp256k1_arg_nonnull(1);

/** verify an ecdsa signature.
 *  returns: 1: correct signature
 *           0: incorrect signature
 *          -1: invalid public key
 *          -2: invalid signature
 * in:       ctx:       a secp256k1 context object, initialized for verification.
 *           msg32:     the 32-byte message hash being verified (cannot be null)
 *           sig:       the signature being verified (cannot be null)
 *           siglen:    the length of the signature
 *           pubkey:    the public key to verify with (cannot be null)
 *           pubkeylen: the length of pubkey
 */
secp256k1_warn_unused_result int secp256k1_ecdsa_verify(
  const secp256k1_context_t* ctx,
  const unsigned char *msg32,
  const unsigned char *sig,
  int siglen,
  const unsigned char *pubkey,
  int pubkeylen
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3) secp256k1_arg_nonnull(5);

/** a pointer to a function to deterministically generate a nonce.
 * returns: 1 if a nonce was successfully generated. 0 will cause signing to fail.
 * in:      msg32:     the 32-byte message hash being verified (will not be null)
 *          key32:     pointer to a 32-byte secret key (will not be null)
 *          attempt:   how many iterations we have tried to find a nonce.
 *                     this will almost always be 0, but different attempt values
 *                     are required to result in a different nonce.
 *          data:      arbitrary data pointer that is passed through.
 * out:     nonce32:   pointer to a 32-byte array to be filled by the function.
 * except for test cases, this function should compute some cryptographic hash of
 * the message, the key and the attempt.
 */
typedef int (*secp256k1_nonce_function_t)(
  unsigned char *nonce32,
  const unsigned char *msg32,
  const unsigned char *key32,
  unsigned int attempt,
  const void *data
);

/** an implementation of rfc6979 (using hmac-sha256) as nonce generation function.
 * if a data pointer is passed, it is assumed to be a pointer to 32 bytes of
 * extra entropy.
 */
extern const secp256k1_nonce_function_t secp256k1_nonce_function_rfc6979;

/** a default safe nonce generation function (currently equal to secp256k1_nonce_function_rfc6979). */
extern const secp256k1_nonce_function_t secp256k1_nonce_function_default;


/** create an ecdsa signature.
 *  returns: 1: signature created
 *           0: the nonce generation function failed, the private key was invalid, or there is not
 *              enough space in the signature (as indicated by siglen).
 *  in:      ctx:    pointer to a context object, initialized for signing (cannot be null)
 *           msg32:  the 32-byte message hash being signed (cannot be null)
 *           seckey: pointer to a 32-byte secret key (cannot be null)
 *           noncefp:pointer to a nonce generation function. if null, secp256k1_nonce_function_default is used
 *           ndata:  pointer to arbitrary data used by the nonce generation function (can be null)
 *  out:     sig:    pointer to an array where the signature will be placed (cannot be null)
 *  in/out:  siglen: pointer to an int with the length of sig, which will be updated
 *                   to contain the actual signature length (<=72).
 *
 * the sig always has an s value in the lower half of the range (from 0x1
 * to 0x7fffffffffffffffffffffffffffffff5d576e7357a4501ddfe92f46681b20a0,
 * inclusive), unlike many other implementations.
 * with ecdsa a third-party can can forge a second distinct signature
 * of the same message given a single initial signature without knowing
 * the key by setting s to its additive inverse mod-order, 'flipping' the
 * sign of the random point r which is not included in the signature.
 * since the forgery is of the same message this isn't universally
 * problematic, but in systems where message malleability or uniqueness
 * of signatures is important this can cause issues.  this forgery can be
 * blocked by all verifiers forcing signers to use a canonical form. the
 * lower-s form reduces the size of signatures slightly on average when
 * variable length encodings (such as der) are used and is cheap to
 * verify, making it a good choice. security of always using lower-s is
 * assured because anyone can trivially modify a signature after the
 * fact to enforce this property.  adjusting it inside the signing
 * function avoids the need to re-serialize or have curve specific
 * constants outside of the library.  by always using a canonical form
 * even in applications where it isn't needed it becomes possible to
 * impose a requirement later if a need is discovered.
 * no other forms of ecdsa malleability are known and none seem likely,
 * but there is no formal proof that ecdsa, even with this additional
 * restriction, is free of other malleability.  commonly used serialization
 * schemes will also accept various non-unique encodings, so care should
 * be taken when this property is required for an application.
 */
int secp256k1_ecdsa_sign(
  const secp256k1_context_t* ctx,
  const unsigned char *msg32,
  unsigned char *sig,
  int *siglen,
  const unsigned char *seckey,
  secp256k1_nonce_function_t noncefp,
  const void *ndata
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3) secp256k1_arg_nonnull(4) secp256k1_arg_nonnull(5);

/** create a compact ecdsa signature (64 byte + recovery id).
 *  returns: 1: signature created
 *           0: the nonce generation function failed, or the secret key was invalid.
 *  in:      ctx:    pointer to a context object, initialized for signing (cannot be null)
 *           msg32:  the 32-byte message hash being signed (cannot be null)
 *           seckey: pointer to a 32-byte secret key (cannot be null)
 *           noncefp:pointer to a nonce generation function. if null, secp256k1_nonce_function_default is used
 *           ndata:  pointer to arbitrary data used by the nonce generation function (can be null)
 *  out:     sig:    pointer to a 64-byte array where the signature will be placed (cannot be null)
 *                   in case 0 is returned, the returned signature length will be zero.
 *           recid:  pointer to an int, which will be updated to contain the recovery id (can be null)
 */
int secp256k1_ecdsa_sign_compact(
  const secp256k1_context_t* ctx,
  const unsigned char *msg32,
  unsigned char *sig64,
  const unsigned char *seckey,
  secp256k1_nonce_function_t noncefp,
  const void *ndata,
  int *recid
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3) secp256k1_arg_nonnull(4);

/** recover an ecdsa public key from a compact signature.
 *  returns: 1: public key successfully recovered (which guarantees a correct signature).
 *           0: otherwise.
 *  in:      ctx:        pointer to a context object, initialized for verification (cannot be null)
 *           msg32:      the 32-byte message hash assumed to be signed (cannot be null)
 *           sig64:      signature as 64 byte array (cannot be null)
 *           compressed: whether to recover a compressed or uncompressed pubkey
 *           recid:      the recovery id (0-3, as returned by ecdsa_sign_compact)
 *  out:     pubkey:     pointer to a 33 or 65 byte array to put the pubkey (cannot be null)
 *           pubkeylen:  pointer to an int that will contain the pubkey length (cannot be null)
 */
secp256k1_warn_unused_result int secp256k1_ecdsa_recover_compact(
  const secp256k1_context_t* ctx,
  const unsigned char *msg32,
  const unsigned char *sig64,
  unsigned char *pubkey,
  int *pubkeylen,
  int compressed,
  int recid
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3) secp256k1_arg_nonnull(4) secp256k1_arg_nonnull(5);

/** verify an ecdsa secret key.
 *  returns: 1: secret key is valid
 *           0: secret key is invalid
 *  in:      ctx: pointer to a context object (cannot be null)
 *           seckey: pointer to a 32-byte secret key (cannot be null)
 */
secp256k1_warn_unused_result int secp256k1_ec_seckey_verify(
  const secp256k1_context_t* ctx,
  const unsigned char *seckey
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2);

/** just validate a public key.
 *  returns: 1: public key is valid
 *           0: public key is invalid
 *  in:      ctx:       pointer to a context object (cannot be null)
 *           pubkey:    pointer to a 33-byte or 65-byte public key (cannot be null).
 *           pubkeylen: length of pubkey
 */
secp256k1_warn_unused_result int secp256k1_ec_pubkey_verify(
  const secp256k1_context_t* ctx,
  const unsigned char *pubkey,
  int pubkeylen
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2);

/** compute the public key for a secret key.
 *  in:     ctx:        pointer to a context object, initialized for signing (cannot be null)
 *          compressed: whether the computed public key should be compressed
 *          seckey:     pointer to a 32-byte private key (cannot be null)
 *  out:    pubkey:     pointer to a 33-byte (if compressed) or 65-byte (if uncompressed)
 *                      area to store the public key (cannot be null)
 *          pubkeylen:  pointer to int that will be updated to contains the pubkey's
 *                      length (cannot be null)
 *  returns: 1: secret was valid, public key stores
 *           0: secret was invalid, try again
 */
secp256k1_warn_unused_result int secp256k1_ec_pubkey_create(
  const secp256k1_context_t* ctx,
  unsigned char *pubkey,
  int *pubkeylen,
  const unsigned char *seckey,
  int compressed
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3) secp256k1_arg_nonnull(4);

/** decompress a public key.
 * in:     ctx:       pointer to a context object (cannot be null)
 * in/out: pubkey:    pointer to a 65-byte array to put the decompressed public key.
 *                    it must contain a 33-byte or 65-byte public key already (cannot be null)
 *         pubkeylen: pointer to the size of the public key pointed to by pubkey (cannot be null)
 *                    it will be updated to reflect the new size.
 * returns: 0: pubkey was invalid
 *          1: pubkey was valid, and was replaced with its decompressed version
 */
secp256k1_warn_unused_result int secp256k1_ec_pubkey_decompress(
  const secp256k1_context_t* ctx,
  unsigned char *pubkey,
  int *pubkeylen
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3);

/** export a private key in der format.
 * in: ctx: pointer to a context object, initialized for signing (cannot be null)
 */
secp256k1_warn_unused_result int secp256k1_ec_privkey_export(
  const secp256k1_context_t* ctx,
  const unsigned char *seckey,
  unsigned char *privkey,
  int *privkeylen,
  int compressed
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3) secp256k1_arg_nonnull(4);

/** import a private key in der format. */
secp256k1_warn_unused_result int secp256k1_ec_privkey_import(
  const secp256k1_context_t* ctx,
  unsigned char *seckey,
  const unsigned char *privkey,
  int privkeylen
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3);

/** tweak a private key by adding tweak to it. */
secp256k1_warn_unused_result int secp256k1_ec_privkey_tweak_add(
  const secp256k1_context_t* ctx,
  unsigned char *seckey,
  const unsigned char *tweak
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3);

/** tweak a public key by adding tweak times the generator to it.
 * in: ctx: pointer to a context object, initialized for verification (cannot be null)
 */
secp256k1_warn_unused_result int secp256k1_ec_pubkey_tweak_add(
  const secp256k1_context_t* ctx,
  unsigned char *pubkey,
  int pubkeylen,
  const unsigned char *tweak
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(4);

/** tweak a private key by multiplying it with tweak. */
secp256k1_warn_unused_result int secp256k1_ec_privkey_tweak_mul(
  const secp256k1_context_t* ctx,
  unsigned char *seckey,
  const unsigned char *tweak
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(3);

/** tweak a public key by multiplying it with tweak.
 * in: ctx: pointer to a context object, initialized for verification (cannot be null)
 */
secp256k1_warn_unused_result int secp256k1_ec_pubkey_tweak_mul(
  const secp256k1_context_t* ctx,
  unsigned char *pubkey,
  int pubkeylen,
  const unsigned char *tweak
) secp256k1_arg_nonnull(1) secp256k1_arg_nonnull(2) secp256k1_arg_nonnull(4);

/** updates the context randomization.
 *  returns: 1: randomization successfully updated
 *           0: error
 *  in:      ctx:       pointer to a context object (cannot be null)
 *           seed32:    pointer to a 32-byte random seed (null resets to initial state)
 */
secp256k1_warn_unused_result int secp256k1_context_randomize(
  secp256k1_context_t* ctx,
  const unsigned char *seed32
) secp256k1_arg_nonnull(1);


# ifdef __cplusplus
}
# endif

#endif
