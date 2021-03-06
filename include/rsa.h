/*
 * rsa.h
 *
 * Created on: 25.10.2014
 * Author:     rc0r
 */

#ifndef INCLUDE_RSA_H_
#define INCLUDE_RSA_H_

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rand.h>

#include "ring.h"

/** RSA functions and stuff **/
// we're using some hard coded dummy ASN1 field here
static const unsigned char *ASN1_field = "\xee\xee\xee\xee";
static const unsigned int ASN1_field_len = 0;
static const char *BN_e_str = "3";

struct rsa_key {
	BIGNUM *e;
	BIGNUM *n;
};

typedef struct rsa_key rsa_key_t;

// generate RSA public and private keys
void rsa_generate_keypair(rsa_key_t *o_pubkey, rsa_key_t *o_privkey, unsigned long bits);
// perform core bignum encryption
void rsa_bn_encrypt(BIGNUM *o_crypt, BIGNUM *i_plain, rsa_key_t *i_pubkey);
// perform data encryption
unsigned int rsa_encrypt(unsigned char **o_crypt, unsigned char *i_plain, unsigned int i_plain_len, rsa_key_t *i_pubkey);
// perform core bignum decryption
void rsa_bn_decrypt(BIGNUM *o_plain, BIGNUM *i_crypt, rsa_key_t *i_privkey);
// perform data decryption
unsigned int rsa_decrypt(unsigned char **o_plain, unsigned char *i_crypt, unsigned int i_crypt_len, rsa_key_t *i_privkey);
// perform simplified PKCS#1 v1.5 encoding
int rsa_simple_pad(unsigned char *o_padded_msg, unsigned char *i_msg, unsigned int i_msg_len, unsigned int i_padded_msg_len, unsigned char i_pad_char);
// RSA sign a message (using SHA-256)
int rsa_sign(unsigned char **o_signature, unsigned char *i_msg, unsigned int i_msg_len, rsa_key_t *i_privkey);
// Verifies an RSA signature in a vulnerable way
int rsa_sign_verify(unsigned char *i_msg, unsigned int i_msg_len, unsigned char *i_sign, unsigned int i_sign_len, rsa_key_t *i_pubkey);
// Forges an RSA signature by using Bleichenbacher's e=3 attack
int rsa_sign_forge(unsigned char **o_sign_forged, unsigned char *i_msg, unsigned int i_msg_len, rsa_key_t *i_pubkey);
// perform RSA broadcast attack
int rsa_broadcast_attack(unsigned char **o_plain, unsigned char *i_crypted[], unsigned int i_crypted_len[], rsa_key_t *i_pubkeys[], unsigned int len);
// provide a RSA unpadded message oracle
void rsa_unpadded_msg_oracle(BIGNUM *o_plain, BIGNUM *i_cipher, rsa_key_t *i_privkey);
// perform RSA unpadding message oracle attack
int rsa_unpadded_msg_oracle_attack(unsigned char **o_plain, unsigned char *i_ciphertext, unsigned int i_ciphertext_len, rsa_key_t *i_pubkey, rsa_key_t *i_privkey);

/** test funcs **/
void rsa_broadcast_attack_test(void);
void rsa_unpadded_msg_oracle_attack_test(void);
void rsa_simple_pad_test(void);

#endif /* INCLUDE_RSA_H_ */
