#include "string.h"
#include "test_ntru.h"
#include "test_util.h"
#include "ntru.h"
#include "rand.h"

void encrypt_poly(NtruIntPoly *m, NtruTernPoly *r, NtruIntPoly *h, NtruIntPoly *e, uint16_t q) {
    ntru_mult_tern(h, r, e);
    ntru_add_int_mod(e, m, q);
}

void decrypt_poly(NtruIntPoly *e, NtruEncPrivKey *priv, uint16_t q, NtruIntPoly *d) {
#ifndef NTRU_AVOID_HAMMING_WT_PATENT
    if (priv->prod_flag)
        ntru_mult_prod(e, &priv->t.prod, d);
    else
#endif   /* NTRU_AVOID_HAMMING_WT_PATENT */
        ntru_mult_tern(e, &priv->t.tern, d);
    ntru_mod(d, q);
    ntru_mult_fac(d, 3);
    ntru_add_int(d, e);
    ntru_mod_center(d, q);
    ntru_mod3(d);
}

uint8_t gen_key_pair_det(char *seed, NtruEncParams *params, NtruEncKeyPair *kp) {
    uint16_t seed_len = strlen(seed);
    uint8_t seed_uint8[seed_len];
    str_to_uint8(seed, seed_uint8);
    return ntru_gen_key_pair_det(params, kp, ntru_rand_igf2, seed_uint8, seed_len);
}

uint8_t test_keygen() {
    NtruEncParams param_arr[] = ALL_PARAM_SETS;
    uint8_t valid = 1;

    uint8_t i;
    for (i=0; i<sizeof(param_arr)/sizeof(param_arr[0]); i++) {
        NtruEncParams params = param_arr[i];
        NtruEncKeyPair kp;
        valid &= ntru_gen_key_pair(&params, &kp, ntru_rand_default) == NTRU_SUCCESS;

        /* encrypt a random message */
        NtruTernPoly m;
        ntru_rand_tern(params.N, params.N/3, params.N/3, &m, ntru_rand_default, NULL);
        NtruIntPoly m_int;
        ntru_tern_to_int(&m, &m_int);

        NtruTernPoly r;
        ntru_rand_tern(params.N, params.N/3, params.N/3, &r, ntru_rand_default, NULL);
        NtruIntPoly e;
        encrypt_poly(&m_int, &r, &kp.pub.h, &e, params.q);

        /* decrypt and verify */
        NtruIntPoly c;
        decrypt_poly(&e, &kp.priv, params.q, &c);
        valid &= ntru_equals_int(&m_int, &c);

        /* test deterministic key generation */
        valid &= gen_key_pair_det("my test password", &params, &kp) == NTRU_SUCCESS;
        char seed2_char[19];
        strcpy(seed2_char, "my test password");
        uint8_t seed2[strlen(seed2_char)];
        str_to_uint8(seed2_char, seed2);
        NtruEncKeyPair kp2;
        valid &= ntru_gen_key_pair_det(&params, &kp2, ntru_rand_igf2, seed2, strlen(seed2_char)) == NTRU_SUCCESS;
        valid &= equals_key_pair(&kp, &kp2);
    }

    print_result("test_keygen", valid);
    return valid;
}

/* tests ntru_encrypt() */
uint8_t test_encr_decr_nondet(NtruEncParams *params) {
    NtruEncKeyPair kp;
    uint8_t valid = ntru_gen_key_pair(params, &kp, ntru_rand_default) == NTRU_SUCCESS;

    uint16_t enc_len = ntru_enc_len(params);
    char plain_char[19];
    strcpy(plain_char, "test message 12345");
    size_t plain_len = strlen(plain_char);
    uint8_t plain[plain_len];
    str_to_uint8(plain_char, plain);
    uint8_t encrypted[enc_len];
    valid &= ntru_encrypt((uint8_t*)&plain, plain_len, &kp.pub, params, ntru_rand_default, (uint8_t*)&encrypted) == NTRU_SUCCESS;
    uint8_t decrypted[plain_len];
    uint16_t dec_len;
    valid &= ntru_decrypt((uint8_t*)&encrypted, &kp, params, (uint8_t*)&decrypted, &dec_len) == NTRU_SUCCESS;
    valid &= equals_arr((uint8_t*)&plain, (uint8_t*)&decrypted, plain_len);

    return valid;
}

/* tests ntru_encrypt_det() */
uint8_t test_encr_decr_det(NtruEncParams *params) {
    NtruEncKeyPair kp;
    uint8_t valid = gen_key_pair_det("seed value for key generation", params, &kp) == NTRU_SUCCESS;
    uint8_t pub_arr[ntru_pub_len(params)];
    ntru_export_pub(&kp.pub, pub_arr);
    NtruEncPubKey pub2;
    ntru_import_pub(pub_arr, &pub2);
    valid &= ntru_equals_int(&kp.pub.h, &pub2.h);
    char seed_char[11];
    strcpy(seed_char, "seed value");
    uint8_t seed[11];
    uint16_t enc_len = ntru_enc_len(params);
    char plain_char[19];
    strcpy(plain_char, "test message 12345");
    size_t plain_len = strlen(plain_char);
    uint8_t plain[plain_len];
    str_to_uint8(plain_char, plain);
    str_to_uint8(seed_char, seed);
    uint8_t encrypted[enc_len];
    valid &= ntru_encrypt_det((uint8_t*)&plain, plain_len, &kp.pub, params, ntru_rand_igf2, seed, strlen(seed_char), (uint8_t*)&encrypted) == NTRU_SUCCESS;
    char plain2_char[19];
    strcpy(plain2_char, "test message 12345");
    uint8_t plain2[plain_len];
    str_to_uint8(plain2_char, plain2);
    char seed2_char[11];
    strcpy(seed2_char, "seed value");
    uint8_t seed2[11];
    str_to_uint8(seed2_char, seed2);
    uint8_t encrypted2[enc_len];
    valid &= ntru_encrypt_det((uint8_t*)&plain2, plain_len, &pub2, params, ntru_rand_igf2, seed2, strlen(seed2_char), (uint8_t*)&encrypted2) == NTRU_SUCCESS;
    valid &= memcmp(encrypted, encrypted2, enc_len) == 0;
    return valid;
}

uint8_t test_encr_decr() {
    NtruEncParams param_arr[] = ALL_PARAM_SETS;
    uint8_t valid = 1;
    uint8_t i;
    for (i=0; i<sizeof(param_arr)/sizeof(param_arr[0]); i++) {
        valid &= test_encr_decr_nondet(&param_arr[i]);
        valid &= test_encr_decr_det(&param_arr[i]);
    }

#ifndef WIN32
    /* test encryption with ntru_rand_devrandom */
    NtruEncParams params = EES401EP1;
    valid &= test_encr_decr_nondet(&params);
#endif

    print_result("test_encr_decr", valid);
    return valid;
}

uint8_t test_ntru() {
    uint8_t valid = test_keygen();
    valid &= test_encr_decr();
    return valid;
}
