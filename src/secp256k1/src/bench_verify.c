/**********************************************************************
 * copyright (c) 2014 pieter wuille                                   *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#include <stdio.h>
#include <string.h>

#include "include/secp256k1.h"
#include "util.h"
#include "bench.h"

typedef struct {
    secp256k1_context_t *ctx;
    unsigned char msg[32];
    unsigned char key[32];
    unsigned char sig[72];
    int siglen;
    unsigned char pubkey[33];
    int pubkeylen;
} benchmark_verify_t;

static void benchmark_verify(void* arg) {
    int i;
    benchmark_verify_t* data = (benchmark_verify_t*)arg;

    for (i = 0; i < 20000; i++) {
        data->sig[data->siglen - 1] ^= (i & 0xff);
        data->sig[data->siglen - 2] ^= ((i >> 8) & 0xff);
        data->sig[data->siglen - 3] ^= ((i >> 16) & 0xff);
        check(secp256k1_ecdsa_verify(data->ctx, data->msg, data->sig, data->siglen, data->pubkey, data->pubkeylen) == (i == 0));
        data->sig[data->siglen - 1] ^= (i & 0xff);
        data->sig[data->siglen - 2] ^= ((i >> 8) & 0xff);
        data->sig[data->siglen - 3] ^= ((i >> 16) & 0xff);
    }
}

int main(void) {
    int i;
    benchmark_verify_t data;

    data.ctx = secp256k1_context_create(secp256k1_context_sign | secp256k1_context_verify);

    for (i = 0; i < 32; i++) data.msg[i] = 1 + i;
    for (i = 0; i < 32; i++) data.key[i] = 33 + i;
    data.siglen = 72;
    secp256k1_ecdsa_sign(data.ctx, data.msg, data.sig, &data.siglen, data.key, null, null);
    data.pubkeylen = 33;
    check(secp256k1_ec_pubkey_create(data.ctx, data.pubkey, &data.pubkeylen, data.key, 1));

    run_benchmark("ecdsa_verify", benchmark_verify, null, null, &data, 10, 20000);

    secp256k1_context_destroy(data.ctx);
    return 0;
}
