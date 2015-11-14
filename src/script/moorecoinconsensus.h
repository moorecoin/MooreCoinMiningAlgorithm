// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_moorecoinconsensus_h
#define moorecoin_moorecoinconsensus_h

#if defined(build_moorecoin_internal) && defined(have_config_h)
#include "config/moorecoin-config.h"
  #if defined(_win32)
    #if defined(dll_export)
      #if defined(have_func_attribute_dllexport)
        #define export_symbol __declspec(dllexport)
      #else
        #define export_symbol
      #endif
    #endif
  #elif defined(have_func_attribute_visibility)
    #define export_symbol __attribute__ ((visibility ("default")))
  #endif
#elif defined(msc_ver) && !defined(static_libmoorecoinconsensus)
  #define export_symbol __declspec(dllimport)
#endif

#ifndef export_symbol
  #define export_symbol
#endif

#ifdef __cplusplus
extern "c" {
#endif

#define moorecoinconsensus_api_ver 0

typedef enum moorecoinconsensus_error_t
{
    moorecoinconsensus_err_ok = 0,
    moorecoinconsensus_err_tx_index,
    moorecoinconsensus_err_tx_size_mismatch,
    moorecoinconsensus_err_tx_deserialize,
} moorecoinconsensus_error;

/** script verification flags */
enum
{
    moorecoinconsensus_script_flags_verify_none      = 0,
    moorecoinconsensus_script_flags_verify_p2sh      = (1u << 0), // evaluate p2sh (bip16) subscripts
    moorecoinconsensus_script_flags_verify_dersig    = (1u << 2), // enforce strict der (bip66) compliance
};

/// returns 1 if the input nin of the serialized transaction pointed to by
/// txto correctly spends the scriptpubkey pointed to by scriptpubkey under
/// the additional constraints specified by flags.
/// if not null, err will contain an error/success code for the operation
export_symbol int moorecoinconsensus_verify_script(const unsigned char *scriptpubkey, unsigned int scriptpubkeylen,
                                    const unsigned char *txto        , unsigned int txtolen,
                                    unsigned int nin, unsigned int flags, moorecoinconsensus_error* err);

export_symbol unsigned int moorecoinconsensus_version();

#ifdef __cplusplus
} // extern "c"
#endif

#undef export_symbol

#endif // moorecoin_moorecoinconsensus_h
