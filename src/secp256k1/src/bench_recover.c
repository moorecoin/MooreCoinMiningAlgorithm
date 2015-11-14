/**********************************************************************
 * copyright (c) 2014 pieter wuille                                   *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#include "include/secp256k1.h"
#include "util.h"
#include "bench.h"

typedef struct {
    secp256k1_context_t *ctx;
    unsigned char msg[32];
    unsigned char sig[64];
} bench_recover_t;

void bench_recover(void* arg) {
    int i;
    bench_recover_t *data = (bench_recover_t*)arg;
    unsigned char pubkey[33];

    for (i = 0; i < 20000; i++) {
        int j;
        int pubkeylen = 33;
        check(secp256k1_ecdsa_recover_compact(data->ctx, data->msg, data->sig, pubkey, &pubkeylen, 1, i % 2));
        for (j = 0; j < 32; j++) {
            data->sig[j + 32] = data->msg[j];    /* move former message to s. */
            data->msg[j] = data->sig[j];         /* move former r to message. */
            data->sig[j] = pubkey[j + 1];        /* move recovered pubkey x coordinate to r (which must be a valid x coordinate). */
        }
    }
}

void bench_recover_setup(void* arg) {
    int i;
    bench_recover_t *data = (bench_recover_t*)arg;

    for (i = 0; i < 32; i++) data->msg[i] = 1 + i;
    for (i = 0; i < 64; i++) data->sig[i] = 65 + i;
}

int main(void) {
    bench_recover_t data;

    data.ctx = secp256k1_context_create(secp256k1_context_verify);

    run_benchmark("ecdsa_recover", bench_recover, bench_recover_setup, null, &data, 10, 20000);

    secp256k1_context_destroy(data.ctx);
    return 0;
}
