#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/aes.h"
#include "../include/pkcs.h"
#include "../include/hex2base64.h"
#include "../include/histogram.h"
#include "../include/rrand.h"

unsigned int aes_cbc_padding_oracle(unsigned char *ciphertext, unsigned char *key, unsigned char *iv)
{
	unsigned char *strings[10] = {
		"MDAwMDAwTm93IHRoYXQgdGhlIHBhcnR5IGlzIGp1bXBpbmc=",
		"MDAwMDAxV2l0aCB0aGUgYmFzcyBraWNrZWQgaW4gYW5kIHRoZSBWZWdhJ3MgYXJlIHB1bXBpbic=",
		"MDAwMDAyUXVpY2sgdG8gdGhlIHBvaW50LCB0byB0aGUgcG9pbnQsIG5vIGZha2luZw==",
		"MDAwMDAzQ29va2luZyBNQydzIGxpa2UgYSBwb3VuZCBvZiBiYWNvbg==",
		"MDAwMDA0QnVybmluZyAnZW0sIGlmIHlvdSBhaW4ndCBxdWljayBhbmQgbmltYmxl",
		"MDAwMDA1SSBnbyBjcmF6eSB3aGVuIEkgaGVhciBhIGN5bWJhbA==",
		"MDAwMDA2QW5kIGEgaGlnaCBoYXQgd2l0aCBhIHNvdXBlZCB1cCB0ZW1wbw==",
		"MDAwMDA3SSdtIG9uIGEgcm9sbCwgaXQncyB0aW1lIHRvIGdvIHNvbG8=",
		"MDAwMDA4b2xsaW4nIGluIG15IGZpdmUgcG9pbnQgb2g=",
		"MDAwMDA5aXRoIG15IHJhZy10b3AgZG93biBzbyBteSBoYWlyIGNhbiBibG93"
	};

	unsigned char *string_b64 = strings[rand() % 10];
	unsigned char *string;
	unsigned int string_len;

	// base64 decode
	string_len = base64decode(&string, string_b64, strlen(string_b64));
	string[string_len] = '\0';

	// perform PKCS#7 padding
	unsigned int str_pad_len = string_len + (16 - (string_len % 16));
	unsigned char str_pad[str_pad_len];

	str_pad_len = pkcs7_padding(str_pad, string, string_len, 16);

	free(string);

	// cbc encrypt
	return aes_cbc_encrypt(128, ciphertext, str_pad, str_pad_len, key, iv);
}

int aes_cbc_padding_oracle_decrypt(unsigned char *ciphertext, unsigned int ciphertext_len, unsigned char *key, unsigned char *iv)
{
	unsigned char plain[ciphertext_len];
	unsigned int plain_len;

	// decrypt
	plain_len = aes_cbc_decrypt(128, plain, ciphertext, ciphertext_len, key, iv);

	// PKCS#7 unpad
	unsigned char unpad[plain_len];

	return pkcs7_unpadding(unpad, plain, plain_len, 16);
}

unsigned int aes_cbc_padding_oracle_attack(unsigned char *plaintext, unsigned char *ciphertext, unsigned int ciphertext_len, unsigned char *key, unsigned char *iv)
{
	// NOTE: we're not going to use the key here
	// it is directly passed to the decryption routine
	int is_valid;
	unsigned int num_blocks = ciphertext_len / 16;
	unsigned char cipher_mod[ciphertext_len];
	unsigned int cipher_mod_len;
	unsigned char *ciphertext_hex;
	unsigned char plain_xor[ciphertext_len];

	int j, m;
	unsigned int i, k, l, hits=0, cnt=0;

// 	hex_encode(&ciphertext_hex, ciphertext, ciphertext_len);
// 	printf("[%02d] cipher = '%s'\n", 0, ciphertext_hex);
// 	free(ciphertext_hex);

	for(i=num_blocks; i>=1; i--) {
		// assemble modded ciphertext
		if(i>1) {
			l=0;
			cipher_mod_len = i*16;
			memcpy(cipher_mod, ciphertext, cipher_mod_len*sizeof(unsigned char));
			memset(cipher_mod+cipher_mod_len-32, 0, 16*sizeof(unsigned char));
		}
		else {
			l=1;
			cipher_mod_len = (i+1)*16;
			memset(cipher_mod, 0, cipher_mod_len*sizeof(unsigned char));
			memcpy(cipher_mod+16, ciphertext, 16*sizeof(unsigned char));
		}

		// iterate over byte position of n-1 ciphertext block
		for(j=15; j>=0; j--) {
			cnt++;

			// brute force
			for(k=0; k<256; k++) {
				cipher_mod[(i-2+l)*16+j] = k;
				is_valid = aes_cbc_padding_oracle_decrypt(cipher_mod, cipher_mod_len, key, iv);
				if(is_valid>0) {
					hits++;
					plain_xor[(i-1)*16+j] = (16-j) ^ k;
					// update solved ciphertext bits to match
					// next padding
					for(m=15; m>=j; m--) {
						cipher_mod[(i-2+l)*16+m] ^= (16-j) ^ ((16-j)+1);
					}
					break;
				}
			}

// 			hex_encode(&ciphertext_hex, cipher_mod, cipher_mod_len);
// 			printf("[%02d] cipher = '%s'\n", j, ciphertext_hex);
// 			free(ciphertext_hex);
		}
	}
	
// 	printf("len=%d, hits=%d, cnt=%d\n", ciphertext_len, hits, cnt);

	hex_encode(&ciphertext_hex, plain_xor, ciphertext_len);
// 	printf("[%02d] plain_xor = '%s'\n", 0, ciphertext_hex);
	free(ciphertext_hex);

	unsigned char *plain;
	unsigned char cipher_shift[ciphertext_len];

	memset(cipher_shift, 0, ciphertext_len);
	memcpy(cipher_shift, iv, 16*sizeof(unsigned char));
	memcpy(cipher_shift+16, ciphertext, (ciphertext_len-16)*sizeof(unsigned char));

// 	hex_encode(&ciphertext_hex, cipher_shift, ciphertext_len);
// 	printf("[%02d] cipher_sh = '%s'\n", 0, ciphertext_hex);
// 	free(ciphertext_hex);

	fixed_xor(&plain, cipher_shift, plain_xor, ciphertext_len);
	
	// remove PKCS#7 padding
	unsigned char unpad[ciphertext_len+1];
	unsigned int unpad_len;

	unpad_len = pkcs7_unpadding(unpad, plain, ciphertext_len, 16);
	memcpy(plaintext, unpad, unpad_len*sizeof(unsigned char));

	free(plain);

	return unpad_len;
}

int main(void) {
	// init rng
	srand((unsigned int) time(NULL));
	
	// init key, iv
	unsigned char key[16], iv[16];

	aes_random_key(key, 16);
	aes_random_key(iv, 16);

	/** Set 3 Challenge 17 **/
	/** CBC Padding Oracle **/
	unsigned char s3c1_cipher[128];
	unsigned int s3c1_cipher_len;

	// call oracle
	s3c1_cipher_len = aes_cbc_padding_oracle(s3c1_cipher, key, iv);

	// call decrypter
	int is_valid;

	is_valid = aes_cbc_padding_oracle_decrypt(s3c1_cipher, s3c1_cipher_len, key, iv);
	printf("[s3c1] valid ... [%s]\n", (is_valid>0) ? "yes": "no");

	// attack
	unsigned char s3c1_plain[s3c1_cipher_len+1];
	memset(s3c1_plain, 0, (s3c1_cipher_len+1)*sizeof(unsigned char));

	aes_cbc_padding_oracle_attack(s3c1_plain, s3c1_cipher, s3c1_cipher_len, key, iv);

	printf("[s3c1] plain = '%s'\n", s3c1_plain);

	/** Set 3 Challenge 18 **/
	/**  CTR  CIPHER MODE  **/
	unsigned char s3c2_in_b64[] = "L77na/nrFsKvynd6HzOoG7GHTLXsTVu9qvY/2syLXzhPweyyMTJULu/6/kXX0KSvoOLSFQ==";

	unsigned char *s3c2_in;
	unsigned int s3c2_in_len;

	unsigned char s3c2_plain[128];
	unsigned int s3c2_plain_len;
	memset(s3c2_plain, 0, 128*sizeof(unsigned char));
	
	// base64 decode
	s3c2_in_len = base64decode(&s3c2_in, s3c2_in_b64, strlen(s3c2_in_b64));
// 	s3c2_in[s3c2_in_len] = '\0';
	
	// crypt
	strncpy(key, "YELLOW SUBMARINE", 16);
	s3c2_plain_len = aes_ctr_crypt(s3c2_plain, s3c2_in, s3c2_in_len, key, 0);
// 	s3c2_plain[s3c2_plain_len] = '\0';

	printf("[s3c2] plain = '%s'\n", s3c2_plain);
	free(s3c2_in);

	/**    Set 3 Challenge 19     **/
	/** CTR STATIC NONCE CRACKING **/
	unsigned char *s3c3_plain_b64[40] = {
		"SSBoYXZlIG1ldCB0aGVtIGF0IGNsb3NlIG9mIGRheQ==",
		"Q29taW5nIHdpdGggdml2aWQgZmFjZXM=",
		"RnJvbSBjb3VudGVyIG9yIGRlc2sgYW1vbmcgZ3JleQ==",
		"RWlnaHRlZW50aC1jZW50dXJ5IGhvdXNlcy4=",
		"SSBoYXZlIHBhc3NlZCB3aXRoIGEgbm9kIG9mIHRoZSBoZWFk",
		"T3IgcG9saXRlIG1lYW5pbmdsZXNzIHdvcmRzLA==",
		"T3IgaGF2ZSBsaW5nZXJlZCBhd2hpbGUgYW5kIHNhaWQ=",
		"UG9saXRlIG1lYW5pbmdsZXNzIHdvcmRzLA==",
		"QW5kIHRob3VnaHQgYmVmb3JlIEkgaGFkIGRvbmU=",
		"T2YgYSBtb2NraW5nIHRhbGUgb3IgYSBnaWJl",
		"VG8gcGxlYXNlIGEgY29tcGFuaW9u",
		"QXJvdW5kIHRoZSBmaXJlIGF0IHRoZSBjbHViLA==",
		"QmVpbmcgY2VydGFpbiB0aGF0IHRoZXkgYW5kIEk=",
		"QnV0IGxpdmVkIHdoZXJlIG1vdGxleSBpcyB3b3JuOg==",
		"QWxsIGNoYW5nZWQsIGNoYW5nZWQgdXR0ZXJseTo=",
		"QSB0ZXJyaWJsZSBiZWF1dHkgaXMgYm9ybi4=",
		"VGhhdCB3b21hbidzIGRheXMgd2VyZSBzcGVudA==",
		"SW4gaWdub3JhbnQgZ29vZCB3aWxsLA==",
		"SGVyIG5pZ2h0cyBpbiBhcmd1bWVudA==",
		"VW50aWwgaGVyIHZvaWNlIGdyZXcgc2hyaWxsLg==",
		"V2hhdCB2b2ljZSBtb3JlIHN3ZWV0IHRoYW4gaGVycw==",
		"V2hlbiB5b3VuZyBhbmQgYmVhdXRpZnVsLA==",
		"U2hlIHJvZGUgdG8gaGFycmllcnM/",
		"VGhpcyBtYW4gaGFkIGtlcHQgYSBzY2hvb2w=",
		"QW5kIHJvZGUgb3VyIHdpbmdlZCBob3JzZS4=",
		"VGhpcyBvdGhlciBoaXMgaGVscGVyIGFuZCBmcmllbmQ=",
		"V2FzIGNvbWluZyBpbnRvIGhpcyBmb3JjZTs=",
		"SGUgbWlnaHQgaGF2ZSB3b24gZmFtZSBpbiB0aGUgZW5kLA==",
		"U28gc2Vuc2l0aXZlIGhpcyBuYXR1cmUgc2VlbWVkLA==",
		"U28gZGFyaW5nIGFuZCBzd2VldCBoaXMgdGhvdWdodC4=",
		"VGhpcyBvdGhlciBtYW4gSSBoYWQgZHJlYW1lZA==",
		"QSBkcnVua2VuLCB2YWluLWdsb3Jpb3VzIGxvdXQu",
		"SGUgaGFkIGRvbmUgbW9zdCBiaXR0ZXIgd3Jvbmc=",
		"VG8gc29tZSB3aG8gYXJlIG5lYXIgbXkgaGVhcnQs",
		"WWV0IEkgbnVtYmVyIGhpbSBpbiB0aGUgc29uZzs=",
		"SGUsIHRvbywgaGFzIHJlc2lnbmVkIGhpcyBwYXJ0",
		"SW4gdGhlIGNhc3VhbCBjb21lZHk7",
		"SGUsIHRvbywgaGFzIGJlZW4gY2hhbmdlZCBpbiBoaXMgdHVybiw=",
		"VHJhbnNmb3JtZWQgdXR0ZXJseTo=",
		"QSB0ZXJyaWJsZSBiZWF1dHkgaXMgYm9ybi4="
	};
	unsigned char *s3c3_plain[40];
	unsigned int s3c3_plain_len[40];
	unsigned char s3c3_cipher[40][128];
	unsigned char *s3c3_cipher_hex[40];
	unsigned int s3c3_cipher_len[40];
	// first block transposed
	unsigned char s3c3_cipher_trans[32][40];
	unsigned char *s3c3_plain_trans[32];

	unsigned char ks[32];
	unsigned int i, j;

	// generate random key
	aes_random_key(key, 16);

	// encrypt strings
	for(i=0; i<40; i++) {
		// base64 decode
		s3c3_plain_len[i] = base64decode(&s3c3_plain[i], s3c3_plain_b64[i], strlen(s3c3_plain_b64[i]));

		// aes ctr crypt
		s3c3_cipher_len[i] = aes_ctr_crypt(s3c3_cipher[i], s3c3_plain[i], s3c3_plain_len[i], key, 0);

		// free plain
		free(s3c3_plain[i]);
	}

	// crckng...
	for(i=0; i<40; i++) {
		// transpose
// 		for(j=0; j<16; j++) {
		for(j=0; j<s3c3_cipher_len[i]; j++) {
			s3c3_cipher_trans[j][i] = s3c3_cipher[i][j];
		}

		// debug print orig ciphertexts
// 		hex_encode(&s3c3_cipher_hex[i], s3c3_cipher[i], s3c3_cipher_len[i]);
// 		printf("[s3c3] %02d: len=%2d, %s\n", i, s3c3_cipher_len[i], s3c3_cipher_hex[i]);
// 		free(s3c3_cipher_hex[i]);
	}

	max_hist_t hist;
	max_hist2_t hist2;
	max_hist3_t hist3;

	for(j=0; j<32; j++) {
		// debug print transposed ciphertexts
// 		hex_encode(&s3c3_cipher_hex[j], s3c3_cipher_trans[j], 40);
// 		printf("[s3c3] %02d: %s\n", j, s3c3_cipher_hex[j]);
// 		free(s3c3_cipher_hex[j]);
		// generate histograms
		init_histogram(&hist);
		init_histogram2(&hist2);
		init_histogram3(&hist3);
		hist = histogram(s3c3_cipher_trans[j], 40, 0);
		hist2 = histogram2(s3c3_cipher_trans[j], 40, 0);
		hist3 = histogram3(s3c3_cipher_trans[j], 40, 0);
		for(i=0; i<HIST_DEPTH; i++) {
			/**** SINGLE BYTE CRCKNG ****/
			// skip obviously wrong counts occuring in messages
			// with 2 blocks (we've got loads of 0x00 in the
			// transposed ciphertexts, so discard them
			if((hist.num[i] > 30) && (hist.byte[i] == 0x00))
				continue;
			// guess keystream
			switch(j) {
				// we don't start with spaces, right?
				case 0: ks[j] = hist.byte[i] ^ 0x54;
					 break;
				default: ks[j] = hist.byte[i] ^ 0x20;
					 break;
			}
			xor_key(&s3c3_plain_trans[j], s3c3_cipher_trans[j], 40, &ks[j], 1);
			// validate by analyzing produced plaintext
			if(is_cleartext(s3c3_plain_trans[j], 40)==0) {
				break;
				free(s3c3_plain_trans[j]);
			}
			free(s3c3_plain_trans[j]);
			/**** ****************** ****/
			/**** DOUBLE BYTE CRCKNG ****//*
			if((hist2.num[i] > 10) &&
			   (hist2.byte[i][0] == 0x00) &&
			   (hist2.byte[i][1] == 0x00))
				continue;

			if(j<31) {
				ks[j] = hist2.byte[i][0] ^ 'e';
				ks[j+1] = hist2.byte[i][1] ^ ' ';
				xor_key(&s3c3_plain_trans[j], s3c3_cipher_trans[j], 40, &ks[j], 2);
				// validate by analyzing produced plaintext
				if(is_cleartext(s3c3_plain_trans[j], 40)==0) {
					break;
					free(s3c3_plain_trans[j]);
				}
				free(s3c3_plain_trans[j]);
			}
			*//**** ****************** ****/
			/**** TRIPLE BYTE CRCKNG ****//*
			if((hist3.num[i] > 5) &&
			   (hist3.byte[i][0] == 0x00) &&
			   (hist3.byte[i][1] == 0x00) &&
			   (hist3.byte[i][2] == 0x00))
				continue;

			if(j<30) {
				ks[j] = hist3.byte[i][0] ^ 't';
				ks[j+1] = hist3.byte[i][1] ^ 'h';
				ks[j+2] = hist3.byte[i][2] ^ 'e';
				xor_key(&s3c3_plain_trans[j], s3c3_cipher_trans[j], 40, &ks[j], 3);
				// validate by analyzing produced plaintext
				if(is_cleartext(s3c3_plain_trans[j], 40)==0) {
					break;
					free(s3c3_plain_trans[j]);
				}
				free(s3c3_plain_trans[j]);
			}
			*//**** ****************** ****/
		}
	}

	// decrypt
	for(j=0; j<40; j++) {
		fixed_xor(&s3c3_plain[j], s3c3_cipher[j], ks, s3c3_cipher_len[j]);
		s3c3_plain[j][s3c3_cipher_len[j]] = 0;
		printf("[s3c3] %02d: plain = '%s'\n", j, s3c3_plain[j]);
		free(s3c3_plain[j]);
	}

	/**    Set 3 Challenge 20        **/
	/** CTR STATIC NONCE CRACKING II **/
	unsigned char *s3c4_plain_b64[60] = {
		"SSdtIHJhdGVkICJSIi4uLnRoaXMgaXMgYSB3YXJuaW5nLCB5YSBiZXR0ZXIgdm9pZCAvIFBvZXRzIGFyZSBwYXJhbm9pZCwgREoncyBELXN0cm95ZWQ=",
		"Q3V6IEkgY2FtZSBiYWNrIHRvIGF0dGFjayBvdGhlcnMgaW4gc3BpdGUtIC8gU3RyaWtlIGxpa2UgbGlnaHRuaW4nLCBJdCdzIHF1aXRlIGZyaWdodGVuaW4nIQ==",
		"QnV0IGRvbid0IGJlIGFmcmFpZCBpbiB0aGUgZGFyaywgaW4gYSBwYXJrIC8gTm90IGEgc2NyZWFtIG9yIGEgY3J5LCBvciBhIGJhcmssIG1vcmUgbGlrZSBhIHNwYXJrOw==",
		"WWEgdHJlbWJsZSBsaWtlIGEgYWxjb2hvbGljLCBtdXNjbGVzIHRpZ2h0ZW4gdXAgLyBXaGF0J3MgdGhhdCwgbGlnaHRlbiB1cCEgWW91IHNlZSBhIHNpZ2h0IGJ1dA==",
		"U3VkZGVubHkgeW91IGZlZWwgbGlrZSB5b3VyIGluIGEgaG9ycm9yIGZsaWNrIC8gWW91IGdyYWIgeW91ciBoZWFydCB0aGVuIHdpc2ggZm9yIHRvbW9ycm93IHF1aWNrIQ==",
		"TXVzaWMncyB0aGUgY2x1ZSwgd2hlbiBJIGNvbWUgeW91ciB3YXJuZWQgLyBBcG9jYWx5cHNlIE5vdywgd2hlbiBJJ20gZG9uZSwgeWEgZ29uZSE=",
		"SGF2ZW4ndCB5b3UgZXZlciBoZWFyZCBvZiBhIE1DLW11cmRlcmVyPyAvIFRoaXMgaXMgdGhlIGRlYXRoIHBlbmFsdHksYW5kIEknbSBzZXJ2aW4nIGE=",
		"RGVhdGggd2lzaCwgc28gY29tZSBvbiwgc3RlcCB0byB0aGlzIC8gSHlzdGVyaWNhbCBpZGVhIGZvciBhIGx5cmljYWwgcHJvZmVzc2lvbmlzdCE=",
		"RnJpZGF5IHRoZSB0aGlydGVlbnRoLCB3YWxraW5nIGRvd24gRWxtIFN0cmVldCAvIFlvdSBjb21lIGluIG15IHJlYWxtIHlhIGdldCBiZWF0IQ==",
		"VGhpcyBpcyBvZmYgbGltaXRzLCBzbyB5b3VyIHZpc2lvbnMgYXJlIGJsdXJyeSAvIEFsbCB5YSBzZWUgaXMgdGhlIG1ldGVycyBhdCBhIHZvbHVtZQ==",
		"VGVycm9yIGluIHRoZSBzdHlsZXMsIG5ldmVyIGVycm9yLWZpbGVzIC8gSW5kZWVkIEknbSBrbm93bi15b3VyIGV4aWxlZCE=",
		"Rm9yIHRob3NlIHRoYXQgb3Bwb3NlIHRvIGJlIGxldmVsIG9yIG5leHQgdG8gdGhpcyAvIEkgYWluJ3QgYSBkZXZpbCBhbmQgdGhpcyBhaW4ndCB0aGUgRXhvcmNpc3Qh",
		"V29yc2UgdGhhbiBhIG5pZ2h0bWFyZSwgeW91IGRvbid0IGhhdmUgdG8gc2xlZXAgYSB3aW5rIC8gVGhlIHBhaW4ncyBhIG1pZ3JhaW5lIGV2ZXJ5IHRpbWUgeWEgdGhpbms=",
		"Rmxhc2hiYWNrcyBpbnRlcmZlcmUsIHlhIHN0YXJ0IHRvIGhlYXI6IC8gVGhlIFItQS1LLUktTSBpbiB5b3VyIGVhcjs=",
		"VGhlbiB0aGUgYmVhdCBpcyBoeXN0ZXJpY2FsIC8gVGhhdCBtYWtlcyBFcmljIGdvIGdldCBhIGF4IGFuZCBjaG9wcyB0aGUgd2Fjaw==",
		"U29vbiB0aGUgbHlyaWNhbCBmb3JtYXQgaXMgc3VwZXJpb3IgLyBGYWNlcyBvZiBkZWF0aCByZW1haW4=",
		"TUMncyBkZWNheWluZywgY3V6IHRoZXkgbmV2ZXIgc3RheWVkIC8gVGhlIHNjZW5lIG9mIGEgY3JpbWUgZXZlcnkgbmlnaHQgYXQgdGhlIHNob3c=",
		"VGhlIGZpZW5kIG9mIGEgcmh5bWUgb24gdGhlIG1pYyB0aGF0IHlvdSBrbm93IC8gSXQncyBvbmx5IG9uZSBjYXBhYmxlLCBicmVha3MtdGhlIHVuYnJlYWthYmxl",
		"TWVsb2RpZXMtdW5tYWthYmxlLCBwYXR0ZXJuLXVuZXNjYXBhYmxlIC8gQSBob3JuIGlmIHdhbnQgdGhlIHN0eWxlIEkgcG9zc2Vz",
		"SSBibGVzcyB0aGUgY2hpbGQsIHRoZSBlYXJ0aCwgdGhlIGdvZHMgYW5kIGJvbWIgdGhlIHJlc3QgLyBGb3IgdGhvc2UgdGhhdCBlbnZ5IGEgTUMgaXQgY2FuIGJl",
		"SGF6YXJkb3VzIHRvIHlvdXIgaGVhbHRoIHNvIGJlIGZyaWVuZGx5IC8gQSBtYXR0ZXIgb2YgbGlmZSBhbmQgZGVhdGgsIGp1c3QgbGlrZSBhIGV0Y2gtYS1za2V0Y2g=",
		"U2hha2UgJ3RpbGwgeW91ciBjbGVhciwgbWFrZSBpdCBkaXNhcHBlYXIsIG1ha2UgdGhlIG5leHQgLyBBZnRlciB0aGUgY2VyZW1vbnksIGxldCB0aGUgcmh5bWUgcmVzdCBpbiBwZWFjZQ==",
		"SWYgbm90LCBteSBzb3VsJ2xsIHJlbGVhc2UhIC8gVGhlIHNjZW5lIGlzIHJlY3JlYXRlZCwgcmVpbmNhcm5hdGVkLCB1cGRhdGVkLCBJJ20gZ2xhZCB5b3UgbWFkZSBpdA==",
		"Q3V6IHlvdXIgYWJvdXQgdG8gc2VlIGEgZGlzYXN0cm91cyBzaWdodCAvIEEgcGVyZm9ybWFuY2UgbmV2ZXIgYWdhaW4gcGVyZm9ybWVkIG9uIGEgbWljOg==",
		"THlyaWNzIG9mIGZ1cnkhIEEgZmVhcmlmaWVkIGZyZWVzdHlsZSEgLyBUaGUgIlIiIGlzIGluIHRoZSBob3VzZS10b28gbXVjaCB0ZW5zaW9uIQ==",
		"TWFrZSBzdXJlIHRoZSBzeXN0ZW0ncyBsb3VkIHdoZW4gSSBtZW50aW9uIC8gUGhyYXNlcyB0aGF0J3MgZmVhcnNvbWU=",
		"WW91IHdhbnQgdG8gaGVhciBzb21lIHNvdW5kcyB0aGF0IG5vdCBvbmx5IHBvdW5kcyBidXQgcGxlYXNlIHlvdXIgZWFyZHJ1bXM7IC8gSSBzaXQgYmFjayBhbmQgb2JzZXJ2ZSB0aGUgd2hvbGUgc2NlbmVyeQ==",
		"VGhlbiBub25jaGFsYW50bHkgdGVsbCB5b3Ugd2hhdCBpdCBtZWFuIHRvIG1lIC8gU3RyaWN0bHkgYnVzaW5lc3MgSSdtIHF1aWNrbHkgaW4gdGhpcyBtb29k",
		"QW5kIEkgZG9uJ3QgY2FyZSBpZiB0aGUgd2hvbGUgY3Jvd2QncyBhIHdpdG5lc3MhIC8gSSdtIGEgdGVhciB5b3UgYXBhcnQgYnV0IEknbSBhIHNwYXJlIHlvdSBhIGhlYXJ0",
		"UHJvZ3JhbSBpbnRvIHRoZSBzcGVlZCBvZiB0aGUgcmh5bWUsIHByZXBhcmUgdG8gc3RhcnQgLyBSaHl0aG0ncyBvdXQgb2YgdGhlIHJhZGl1cywgaW5zYW5lIGFzIHRoZSBjcmF6aWVzdA==",
		"TXVzaWNhbCBtYWRuZXNzIE1DIGV2ZXIgbWFkZSwgc2VlIGl0J3MgLyBOb3cgYW4gZW1lcmdlbmN5LCBvcGVuLWhlYXJ0IHN1cmdlcnk=",
		"T3BlbiB5b3VyIG1pbmQsIHlvdSB3aWxsIGZpbmQgZXZlcnkgd29yZCdsbCBiZSAvIEZ1cmllciB0aGFuIGV2ZXIsIEkgcmVtYWluIHRoZSBmdXJ0dXJl",
		"QmF0dGxlJ3MgdGVtcHRpbmcuLi53aGF0ZXZlciBzdWl0cyB5YSEgLyBGb3Igd29yZHMgdGhlIHNlbnRlbmNlLCB0aGVyZSdzIG5vIHJlc2VtYmxhbmNl",
		"WW91IHRoaW5rIHlvdSdyZSBydWZmZXIsIHRoZW4gc3VmZmVyIHRoZSBjb25zZXF1ZW5jZXMhIC8gSSdtIG5ldmVyIGR5aW5nLXRlcnJpZnlpbmcgcmVzdWx0cw==",
		"SSB3YWtlIHlhIHdpdGggaHVuZHJlZHMgb2YgdGhvdXNhbmRzIG9mIHZvbHRzIC8gTWljLXRvLW1vdXRoIHJlc3VzY2l0YXRpb24sIHJoeXRobSB3aXRoIHJhZGlhdGlvbg==",
		"Tm92b2NhaW4gZWFzZSB0aGUgcGFpbiBpdCBtaWdodCBzYXZlIGhpbSAvIElmIG5vdCwgRXJpYyBCLidzIHRoZSBqdWRnZSwgdGhlIGNyb3dkJ3MgdGhlIGp1cnk=",
		"WW8gUmFraW0sIHdoYXQncyB1cD8gLyBZbywgSSdtIGRvaW5nIHRoZSBrbm93bGVkZ2UsIEUuLCBtYW4gSSdtIHRyeWluZyB0byBnZXQgcGFpZCBpbiBmdWxs",
		"V2VsbCwgY2hlY2sgdGhpcyBvdXQsIHNpbmNlIE5vcmJ5IFdhbHRlcnMgaXMgb3VyIGFnZW5jeSwgcmlnaHQ/IC8gVHJ1ZQ==",
		"S2FyYSBMZXdpcyBpcyBvdXIgYWdlbnQsIHdvcmQgdXAgLyBaYWtpYSBhbmQgNHRoIGFuZCBCcm9hZHdheSBpcyBvdXIgcmVjb3JkIGNvbXBhbnksIGluZGVlZA==",
		"T2theSwgc28gd2hvIHdlIHJvbGxpbicgd2l0aCB0aGVuPyBXZSByb2xsaW4nIHdpdGggUnVzaCAvIE9mIFJ1c2h0b3duIE1hbmFnZW1lbnQ=",
		"Q2hlY2sgdGhpcyBvdXQsIHNpbmNlIHdlIHRhbGtpbmcgb3ZlciAvIFRoaXMgZGVmIGJlYXQgcmlnaHQgaGVyZSB0aGF0IEkgcHV0IHRvZ2V0aGVy",
		"SSB3YW5uYSBoZWFyIHNvbWUgb2YgdGhlbSBkZWYgcmh5bWVzLCB5b3Uga25vdyB3aGF0IEknbSBzYXlpbic/IC8gQW5kIHRvZ2V0aGVyLCB3ZSBjYW4gZ2V0IHBhaWQgaW4gZnVsbA==",
		"VGhpbmtpbicgb2YgYSBtYXN0ZXIgcGxhbiAvICdDdXogYWluJ3QgbnV0aGluJyBidXQgc3dlYXQgaW5zaWRlIG15IGhhbmQ=",
		"U28gSSBkaWcgaW50byBteSBwb2NrZXQsIGFsbCBteSBtb25leSBpcyBzcGVudCAvIFNvIEkgZGlnIGRlZXBlciBidXQgc3RpbGwgY29taW4nIHVwIHdpdGggbGludA==",
		"U28gSSBzdGFydCBteSBtaXNzaW9uLCBsZWF2ZSBteSByZXNpZGVuY2UgLyBUaGlua2luJyBob3cgY291bGQgSSBnZXQgc29tZSBkZWFkIHByZXNpZGVudHM=",
		"SSBuZWVkIG1vbmV5LCBJIHVzZWQgdG8gYmUgYSBzdGljay11cCBraWQgLyBTbyBJIHRoaW5rIG9mIGFsbCB0aGUgZGV2aW91cyB0aGluZ3MgSSBkaWQ=",
		"SSB1c2VkIHRvIHJvbGwgdXAsIHRoaXMgaXMgYSBob2xkIHVwLCBhaW4ndCBudXRoaW4nIGZ1bm55IC8gU3RvcCBzbWlsaW5nLCBiZSBzdGlsbCwgZG9uJ3QgbnV0aGluJyBtb3ZlIGJ1dCB0aGUgbW9uZXk=",
		"QnV0IG5vdyBJIGxlYXJuZWQgdG8gZWFybiAnY3V6IEknbSByaWdodGVvdXMgLyBJIGZlZWwgZ3JlYXQsIHNvIG1heWJlIEkgbWlnaHQganVzdA==",
		"U2VhcmNoIGZvciBhIG5pbmUgdG8gZml2ZSwgaWYgSSBzdHJpdmUgLyBUaGVuIG1heWJlIEknbGwgc3RheSBhbGl2ZQ==",
		"U28gSSB3YWxrIHVwIHRoZSBzdHJlZXQgd2hpc3RsaW4nIHRoaXMgLyBGZWVsaW4nIG91dCBvZiBwbGFjZSAnY3V6LCBtYW4sIGRvIEkgbWlzcw==",
		"QSBwZW4gYW5kIGEgcGFwZXIsIGEgc3RlcmVvLCBhIHRhcGUgb2YgLyBNZSBhbmQgRXJpYyBCLCBhbmQgYSBuaWNlIGJpZyBwbGF0ZSBvZg==",
		"RmlzaCwgd2hpY2ggaXMgbXkgZmF2b3JpdGUgZGlzaCAvIEJ1dCB3aXRob3V0IG5vIG1vbmV5IGl0J3Mgc3RpbGwgYSB3aXNo",
		"J0N1eiBJIGRvbid0IGxpa2UgdG8gZHJlYW0gYWJvdXQgZ2V0dGluJyBwYWlkIC8gU28gSSBkaWcgaW50byB0aGUgYm9va3Mgb2YgdGhlIHJoeW1lcyB0aGF0IEkgbWFkZQ==",
		"U28gbm93IHRvIHRlc3QgdG8gc2VlIGlmIEkgZ290IHB1bGwgLyBIaXQgdGhlIHN0dWRpbywgJ2N1eiBJJ20gcGFpZCBpbiBmdWxs",
		"UmFraW0sIGNoZWNrIHRoaXMgb3V0LCB5byAvIFlvdSBnbyB0byB5b3VyIGdpcmwgaG91c2UgYW5kIEknbGwgZ28gdG8gbWluZQ==",
		"J0NhdXNlIG15IGdpcmwgaXMgZGVmaW5pdGVseSBtYWQgLyAnQ2F1c2UgaXQgdG9vayB1cyB0b28gbG9uZyB0byBkbyB0aGlzIGFsYnVt",
		"WW8sIEkgaGVhciB3aGF0IHlvdSdyZSBzYXlpbmcgLyBTbyBsZXQncyBqdXN0IHB1bXAgdGhlIG11c2ljIHVw",
		"QW5kIGNvdW50IG91ciBtb25leSAvIFlvLCB3ZWxsIGNoZWNrIHRoaXMgb3V0LCB5byBFbGk=",
		"VHVybiBkb3duIHRoZSBiYXNzIGRvd24gLyBBbmQgbGV0IHRoZSBiZWF0IGp1c3Qga2VlcCBvbiByb2NraW4n",
		"QW5kIHdlIG91dHRhIGhlcmUgLyBZbywgd2hhdCBoYXBwZW5lZCB0byBwZWFjZT8gLyBQZWFjZQ=="
	};
	unsigned char *s3c4_plain[60];
	unsigned int s3c4_plain_len[60];
	unsigned char s3c4_cipher[60][128];
	unsigned char *s3c4_cipher_hex[60];
	unsigned int s3c4_cipher_len[60];
	// first block transposed
	unsigned char s3c4_cipher_trans[96][60];
	unsigned char *s3c4_plain_trans[96];

	unsigned char s3c4_ks[96];

	// generate random key
	aes_random_key(key, 16);

	// encrypt strings
	for(i=0; i<60; i++) {
		// base64 decode
		s3c4_plain_len[i] = base64decode(&s3c4_plain[i], s3c4_plain_b64[i], strlen(s3c4_plain_b64[i]));

		// aes ctr crypt
		s3c4_cipher_len[i] = aes_ctr_crypt(s3c4_cipher[i], s3c4_plain[i], s3c4_plain_len[i], key, 0);

		// free plain
		free(s3c4_plain[i]);
	}

	// crckng...
	for(i=0; i<60; i++) {
		// transpose
// 		for(j=0; j<16; j++) {
// 		for(j=0; j<s3c4_cipher_len[i]; j++) {
		for(j=0; j<96; j++) {
			s3c4_cipher_trans[j][i] = s3c4_cipher[i][j];
		}

		// debug print orig ciphertexts
		hex_encode(&s3c4_cipher_hex[i], s3c4_cipher[i], s3c4_cipher_len[i]);
// 		printf("[s3c4] %02d: len=%2d, %s\n", i, s3c4_cipher_len[i], s3c4_cipher_hex[i]);
		free(s3c4_cipher_hex[i]);
	}

	for(j=0; j<96; j++) {
		// debug print transposed ciphertexts
// 		hex_encode(&s3c4_cipher_hex[j], s3c4_cipher_trans[j], 60);
// 		printf("[s3c4] %02d: %s\n", j, s3c4_cipher_hex[j]);
// 		free(s3c4_cipher_hex[j]);
		// generate histograms
		init_histogram(&hist);
		hist = histogram(s3c4_cipher_trans[j], 60, 0);
		for(i=0; i<HIST_DEPTH; i++) {
			/**** SINGLE BYTE CRCKNG ****/
			// skip obviously wrong counts occuring in messages
			// with 2 blocs3c4_ks (we've got loads of 0x00 in the
			// transposed ciphertexts, so discard them
			if((hist.num[i] > 30) && (hist.byte[i] == 0x00))
				continue;
			// guess keystream
			switch(j) {
				// we don't start with spaces, right?
				case 0: s3c4_ks[j] = hist.byte[i] ^ 0x54;
					 break;
				default: s3c4_ks[j] = hist.byte[i] ^ 0x20;
					 break;
			}
			xor_key(&s3c4_plain_trans[j], s3c4_cipher_trans[j], 60, &s3c4_ks[j], 1);
			// validate by analyzing produced plaintext
			if(is_cleartext(s3c4_plain_trans[j], 60)==0) {
				break;
				free(s3c4_plain_trans[j]);
			}
			free(s3c4_plain_trans[j]);
		}
	}

	// decrypt
	for(j=0; j<60; j++) {
		fixed_xor(&s3c4_plain[j], s3c4_cipher[j], s3c4_ks, s3c4_cipher_len[j]);
		s3c4_plain[j][s3c4_cipher_len[j]] = 0;
		printf("[s3c4] %02d: plain = '%s'\n", j, s3c4_plain[j]);
		free(s3c4_plain[j]);
	}

	/** Set 3 Challenge 21 **/
	/**     MT19937 RNG    **/
	mt19937_srand((unsigned int) time(NULL));
	printf("[s3c5] mt19937_rand() = %u\n", mt19937_rand());

	/** Set 3 Challenge 22  **/
	/** MT19937 SEED CRCKNG **/
// 	printf("[s3c6] mt19937_seed = %08x\n", mt19937_brute_timeseed());

	/**  Set 3 Challenge 23  **/
	/** MT19937 STATE CRCKNG **/
	unsigned int out_arr[624];
	unsigned int next_out_arr[624];
	unsigned int predicted_out_arr[624];
	unsigned int states[624];
	unsigned int prediction_errors = 0;
	
	mt19937_srand((unsigned int) time(NULL));

	// obtain 624 consecutive outputs
	for(i=0; i<624; i++) {
		out_arr[i] = mt19937_rand();
	}

	// obtain next 624 outputs
	for(i=0; i<624; i++) {
		next_out_arr[i] = mt19937_rand();
	}

	// recover internal states
	mt19937_recover_states(out_arr, states);

	// reset mt
	mt19937_srand(time(NULL));
	mt19937_rand();
	mt19937_rand();
	mt19937_rand();

	// initialize new MT instance with recovered
	// states and index
	mt19937_srand_states(states);

	// predict next 624 RNG outputs
	// (and check vs next_out_arr)
	for(i=0; i<624; i++) {
		predicted_out_arr[i] = mt19937_rand();
		if(predicted_out_arr[i] != next_out_arr[i]) {
			prediction_errors++;
		}
	}

	printf("[s3c7] Predicted 624 random numbers. Prediction errors: %d\n", prediction_errors);
	
	/**  Set 3  Challenge 24  **/
	/** MT19937 STREAM CIPHER **/
	unsigned char s3c8_plain[24] = "AAAAAAAAAAAAAA"; // 14
	unsigned char s3c8_crypted[64];
	unsigned char *s3c8_crypted_hex;
	unsigned char s3c8_uncrypted[64];
	unsigned int s3c8_bytes = 0, s3c8_crypted_bytes;
	unsigned int cracked_seed = 0;

	s3c8_crypted_bytes = mt19937_ctr_oracle(s3c8_crypted, s3c8_plain, 14);

	hex_encode(&s3c8_crypted_hex, s3c8_crypted, s3c8_crypted_bytes);
	printf("[s3c8] s3c8_crypted(%d) = '%s'\n", s3c8_crypted_bytes, s3c8_crypted_hex);
	free(s3c8_crypted_hex);

	// we know a 16 bit seed was used, so brute force should crack
	// it in secs...
	for(i=0; i<65536; i++) {
		memset(s3c8_uncrypted, 0, 64*sizeof(unsigned char));
		s3c8_bytes = mt19937_ctr_crypt(s3c8_uncrypted, s3c8_crypted, s3c8_crypted_bytes, i);
		if(!strncmp(s3c8_uncrypted+(s3c8_crypted_bytes-14), s3c8_plain, 14)) {
			cracked_seed = i;
			break;
		}
	}

	printf("[s3c8] s3c8_seed = %d, s3c8_plain(%d) = '%s'\n", cracked_seed, s3c8_bytes, s3c8_uncrypted+(s3c8_bytes-14));

	unsigned int s3c8_token = mt19937_generate_token();

	printf("[s3c8] token = '%08x' %s time seeded!\n", s3c8_token, (mt19937_is_timeseeded(s3c8_token, 120) == 0) ? "is" : "is not" );
	mt19937_srand(rand());
	s3c8_token = mt19937_rand();
	printf("[s3c8] token = '%08x' %s time seeded!\n", s3c8_token, (mt19937_is_timeseeded(s3c8_token, 120) == 0) ? "is" : "is not" );

	return 0;
}
