/**********************************************************************
 * copyright (c) 2014 pieter wuille                                   *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#include "include/secp256k1.h"
#include "util.h"
#include "bench.h"

typedef struct {
    secp256k1_context_t* ctx;
    unsigned char msg[32];
    unsigned char key[32];
} bench_sign_t;

static void bench_sign_setup(void* arg) {
    int i;
    bench_sign_t *data = (bench_sign_t*)arg;

    for (i = 0; i < 32; i++) data->msg[i] = i + 1;
    for (i = 0; i < 32; i++) data->key[i] = i + 65;
}

static void bench_sign(void* arg) {
    int i;
    bench_sign_t *data = (bench_sign_t*)arg;

    unsigned char sig[64];
    for (i = 0; i < 20000; i++) {
        int j;
        int recid = 0;
        check(secp256k1_ecdsa_sign_compact(data->ctx, data->msg, sig, data->key, null, null, &recid));
        for (j = 0; j < 32; j++) {
            data->msg[j] = sig[j];             /* move former r to message. */
            data->key[j] = sig[j + 32];        /* move former s to key.     */
        }
    }
}

int main(void) {
    bench_sign_t data;

    data.ctx = secp256k1_context_create(secp256k1_context_sign);

    run_benchmark("ecdsa_sign", bench_sign, bench_sign_setup, null, &data, 10, 20000);

    secp256k1_context_destroy(data.ctx);
    return 0;
}
