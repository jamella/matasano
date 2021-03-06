/*
 * hash.h
 *
 * Created on: 04.11.2014
 * Author:     rc0r
 */

#ifndef INCLUDE_HASH_H_
#define INCLUDE_HASH_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/sha.h>
#include <openssl/md5.h>

unsigned int hash_sha256(unsigned char *o_hash_str, unsigned char *i_msg, unsigned int i_msg_len);
unsigned int hash_sha1(unsigned char *o_hash_str, unsigned char *i_msg, unsigned int i_msg_len);
unsigned int hash_md5(unsigned char *o_hash_str, unsigned char *i_msg, unsigned int i_msg_len);

#endif /* INCLUDE_HASH_H_ */
