#ifndef CRYPTO_H
#define CRYPTO_H

#include <zlib.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Calculate the checksum for the input data
 * const unsigned char *data: the null terminated data to process
 * return: the checksum as an unsigned long
 */
unsigned long checksum(char *data) {
    unsigned long crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const unsigned char*)data, strlen(data));
    return crc;
}

/*
 * All functions return a malloc'd string, so make sure to free()!
 * 
 * Disclaimer: This code was designed for educational purposes.
 * It *might* be safe to use for security, but *probably* has
 * vulnerabilities.
 */

// Store this host's key pair as a global variable
EVP_PKEY *key;
int initialized = 0;

// Base64 method declarations
char* base64( const void* binaryData, int len, int *flen );
unsigned char* unbase64( const char* ascii, int len, int *flen );

void init() {
    // Only run once
    if(initialized) return;
    initialized = 1;
    // Generate RSA key
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if(EVP_PKEY_keygen_init(ctx) <= 0) printf("Error in init\n");
    if(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) printf("Error in set bits\n");
    if(EVP_PKEY_keygen(ctx, &key) <= 0) printf("Error in keygen\n");
    EVP_PKEY_CTX_free(ctx);
}

/* Generate and return an encryption key
 * (won't generate a new one if one exists already)
 * return: this host's encryption key
 */
char* getPubKey() {
    init();
    // Get key into memory
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, key);
    BUF_MEM *bufferPtr = NULL;
    BIO_get_mem_ptr(bio, &bufferPtr);
    // Base64 encode (it is already a PEM, but whatever)
    int flen;
    char *b64 = base64(bufferPtr->data, bufferPtr->length, &flen);
    free(bufferPtr);
    free(bio);
    return b64;
}

/* Encrypt a message with peer's encryption key
 * unsigned char *message: plaintext message to encrypt
 * char *pubkey: peer's encryption key (formatted as output to getPubKey())
 * return: base64 encoded ciphertext
 */
char* encrypt(char *message, char *pubkey) {
    init();
    // Create context
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(ctx);
    // Allocate space for encrypted key (hybrid AES) and iv (initialization vector)
    unsigned char *ek = (unsigned char*) malloc(sizeof(unsigned char) * EVP_PKEY_size(key));
    int ekl;
    unsigned char *iv = (unsigned char*) malloc(sizeof(unsigned char) * EVP_MAX_IV_LENGTH);
    int ivl = EVP_MAX_IV_LENGTH;
    // Read public key from parameter
    int pklen;
    unsigned char *unbasedPubkey = unbase64(pubkey, strlen(pubkey), &pklen);
    BIO *bio = BIO_new_mem_buf(unbasedPubkey, pklen);
    EVP_PKEY *pub_key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    // Generate encrypted key and initialization vector
    EVP_SealInit(ctx, EVP_aes_256_cbc(), &ek, &ekl, iv, &pub_key, 1);
    int messagelen = strlen(message);
    unsigned char ciphertext[messagelen + EVP_MAX_BLOCK_LENGTH];
    int len;
    // Encrypt the message
    EVP_SealUpdate(ctx, ciphertext, &len, (unsigned char*) message, messagelen);
    int cipherlen = len;
    EVP_SealFinal(ctx, ciphertext + len, &len);
    cipherlen += len;
    EVP_CIPHER_CTX_free(ctx);
    // Base64 encode initialization vector, encrypted key, and ciphertext
    int b64ivl;
    char *b64iv = base64(iv, ivl, &b64ivl);
    int b64ekl;
    char *b64ek = base64(ek, ekl, &b64ekl);
    int b64ctl;
    char *b64ct = base64(ciphertext, cipherlen, &b64ctl);
    free(ek);
    free(iv);
    // Squash those three things into a string
    char *buffer = (char*) malloc(sizeof(char) * (b64ivl + b64ekl + b64ctl + 3));
    sprintf(buffer, "%s;%s;%s", b64iv, b64ek, b64ct);
    free(b64iv);
    free(b64ek);
    free(b64ct);
    return buffer;
}

/* Decrypt a message with this host's private key
 * char *cipher: base64 encoded ciphertext
 * return: decrypted plaintext message
 */
char* decrypt(char *cipher) {
    init();
    // Chop up the message string
    char c2[strlen(cipher)+1];
    strcpy(c2, cipher);
    char *b64iv = c2;
    char *b64ek = strchr(b64iv, ';') + 1;
    *(b64ek-1) = '\0';
    char *b64ct = strchr(b64ek, ';') + 1;
    *(b64ct-1) = '\0';
    // Convert base64 data back to numbers
    int ivl;
    unsigned char *iv = unbase64(b64iv, strlen(b64iv), &ivl);
    int ekl;
    unsigned char *ek = unbase64(b64ek, strlen(b64ek), &ekl);
    int ctl;
    unsigned char *ct = unbase64(b64ct, strlen(b64ct), &ctl);
    // Decrypt the message
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_OpenInit(ctx, EVP_aes_256_cbc(), ek, ekl, iv, key);
    unsigned char *pt = (unsigned char*) malloc(sizeof(unsigned char) * ctl);
    int len;
    EVP_OpenUpdate(ctx, pt, &len, ct, ctl);
    int ptl = len;
    EVP_OpenFinal(ctx, pt + len, &len);
    ptl += len;
    pt[ptl] = '\0';
    EVP_CIPHER_CTX_free(ctx);
    free(iv);
    free(ek);
    free(ct);
    return (char*) pt;
}

/*

  https://github.com/superwills/NibbleAndAHalf
  base64.h -- Fast base64 encoding and decoding.
  version 1.0.0, April 17, 2013 143a

  Copyright (C) 2013 William Sherif

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  William Sherif
  will.sherif@gmail.com

  YWxsIHlvdXIgYmFzZSBhcmUgYmVsb25nIHRvIHVz

*/

const static char* b64="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ;

// maps A=>0,B=>1..
const static unsigned char unb64[]={
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //10
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //20
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //30
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //40
  0,   0,   0,  62,   0,   0,   0,  63,  52,  53, //50
 54,  55,  56,  57,  58,  59,  60,  61,   0,   0, //60
  0,   0,   0,   0,   0,   0,   1,   2,   3,   4, //70
  5,   6,   7,   8,   9,  10,  11,  12,  13,  14, //80
 15,  16,  17,  18,  19,  20,  21,  22,  23,  24, //90
 25,   0,   0,   0,   0,   0,   0,  26,  27,  28, //100
 29,  30,  31,  32,  33,  34,  35,  36,  37,  38, //110
 39,  40,  41,  42,  43,  44,  45,  46,  47,  48, //120
 49,  50,  51,   0,   0,   0,   0,   0,   0,   0, //130
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //140
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //150
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //160
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //170
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //180
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //190
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //200
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //210
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //220
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //230
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //240
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0, //250
  0,   0,   0,   0,   0,   0,
}; // This array has 256 elements

// Converts binary data of length=len to base64 characters.
// Length of the resultant string is stored in flen
// (you must pass pointer flen).
char* base64( const void* binaryData, int len, int *flen )
{
  const unsigned char* bin = (const unsigned char*) binaryData ;
  char* res ;

  int rc = 0 ; // result counter
  int byteNo ; // I need this after the loop

  int modulusLen = len % 3 ;
  int pad = ((modulusLen&1)<<1) + ((modulusLen&2)>>1) ; // 2 gives 1 and 1 gives 2, but 0 gives 0.

  *flen = 4*(len + pad)/3 ;
  res = (char*) malloc( *flen + 1 ) ; // and one for the null
  if( !res )
  {
    puts( "ERROR: base64 could not allocate enough memory." ) ;
    puts( "I must stop because I could not get enough" ) ;
    return 0;
  }

  for( byteNo = 0 ; byteNo <= len-3 ; byteNo+=3 )
  {
    unsigned char BYTE0=bin[byteNo];
    unsigned char BYTE1=bin[byteNo+1];
    unsigned char BYTE2=bin[byteNo+2];
    res[rc++]  = b64[ BYTE0 >> 2 ] ;
    res[rc++]  = b64[ ((0x3&BYTE0)<<4) + (BYTE1 >> 4) ] ;
    res[rc++]  = b64[ ((0x0f&BYTE1)<<2) + (BYTE2>>6) ] ;
    res[rc++]  = b64[ 0x3f&BYTE2 ] ;
  }

  if( pad==2 )
  {
    res[rc++] = b64[ bin[byteNo] >> 2 ] ;
    res[rc++] = b64[ (0x3&bin[byteNo])<<4 ] ;
    res[rc++] = '=';
    res[rc++] = '=';
  }
  else if( pad==1 )
  {
    res[rc++]  = b64[ bin[byteNo] >> 2 ] ;
    res[rc++]  = b64[ ((0x3&bin[byteNo])<<4)   +   (bin[byteNo+1] >> 4) ] ;
    res[rc++]  = b64[ (0x0f&bin[byteNo+1])<<2 ] ;
    res[rc++] = '=';
  }

  res[rc]=0; // NULL TERMINATOR! ;)
  return res ;
}

unsigned char* unbase64( const char* ascii, int len, int *flen )
{
  const unsigned char *safeAsciiPtr = (const unsigned char*)ascii ;
  unsigned char *bin ;
  int cb=0;
  int charNo;
  int pad = 0 ;

  if( len < 2 ) { // 2 accesses below would be OOB.
    // catch empty string, return NULL as result.
    puts( "ERROR: You passed an invalid base64 string (too short). You get NULL back." ) ;
    *flen=0;
    return 0 ;
  }
  if( safeAsciiPtr[ len-1 ]=='=' )  ++pad ;
  if( safeAsciiPtr[ len-2 ]=='=' )  ++pad ;

  *flen = 3*len/4 - pad ;
  bin = (unsigned char*)malloc( *flen ) ;
  if( !bin )
  {
    puts( "ERROR: unbase64 could not allocate enough memory." ) ;
    puts( "I must stop because I could not get enough" ) ;
    return 0;
  }

  for( charNo=0; charNo <= len - 4 - pad ; charNo+=4 )
  {
    int A=unb64[safeAsciiPtr[charNo]];
    int B=unb64[safeAsciiPtr[charNo+1]];
    int C=unb64[safeAsciiPtr[charNo+2]];
    int D=unb64[safeAsciiPtr[charNo+3]];

    bin[cb++] = (A<<2) | (B>>4) ;
    bin[cb++] = (B<<4) | (C>>2) ;
    bin[cb++] = (C<<6) | (D) ;
  }

  if( pad==1 )
  {
    int A=unb64[safeAsciiPtr[charNo]];
    int B=unb64[safeAsciiPtr[charNo+1]];
    int C=unb64[safeAsciiPtr[charNo+2]];

    bin[cb++] = (A<<2) | (B>>4) ;
    bin[cb++] = (B<<4) | (C>>2) ;
  }
  else if( pad==2 )
  {
    int A=unb64[safeAsciiPtr[charNo]];
    int B=unb64[safeAsciiPtr[charNo+1]];

    bin[cb++] = (A<<2) | (B>>4) ;
  }

  return bin ;
}

#endif
