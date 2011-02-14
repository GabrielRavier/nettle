/* nettle-internal.h
 *
 * Things that are used only by the testsuite and benchmark, and
 * subject to change.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2002 Niels M�ller
 *  
 * The nettle library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The nettle library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the nettle library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#ifndef NETTLE_INTERNAL_H_INCLUDED
#define NETTLE_INTERNAL_H_INCLUDED

#include "nettle-meta.h"

/* Temporary allocation, for systems that don't support alloca. Note
 * that the allocation requests should always be reasonably small, so
 * that they can fit on the stack. For non-alloca systems, we use a
 * fix maximum size, and abort if we ever need anything larger. */

#if HAVE_ALLOCA
# define TMP_DECL(name, type, max) type *name
# define TMP_ALLOC(name, size) (name = alloca(sizeof (*name) * size))
#else /* !HAVE_ALLOCA */
# define TMP_DECL(name, type, max) type name[max]
# define TMP_ALLOC(name, size) \
do { if (size > (sizeof(name) / sizeof(name[0]))) abort(); } while (0)
#endif 

/* Arbitrary limits which apply to systems that don't have alloca */
#define NETTLE_MAX_BIGNUM_BITS 10000
#define NETTLE_MAX_HASH_BLOCK_SIZE 128
#define NETTLE_MAX_HASH_DIGEST_SIZE 64
#define NETTLE_MAX_SEXP_ASSOC 17
#define NETTLE_MAX_CIPHER_BLOCK_SIZE 32

/* Doesn't quite fit with the other algorithms, because of the weak
 * keys. Weak keys are not reported, the functions will simply crash
 * if you try to use a weak key. */

extern const struct nettle_cipher nettle_des;
extern const struct nettle_cipher nettle_des3;

extern const struct nettle_cipher nettle_blowfish128;

/* Glue to openssl, for comparative benchmarking. The corresponding
 * code is not included in the nettle library, as that would make the
 * shared library depend on openssl. Instead, look at
 * examples/nettle-openssl.c. */
extern const struct nettle_cipher nettle_openssl_aes128;
extern const struct nettle_cipher nettle_openssl_aes192;
extern const struct nettle_cipher nettle_openssl_aes256;
extern const struct nettle_cipher nettle_openssl_arcfour128;
extern const struct nettle_cipher nettle_openssl_blowfish128;
extern const struct nettle_cipher nettle_openssl_des;
extern const struct nettle_cipher nettle_openssl_cast128;

extern const struct nettle_hash nettle_openssl_md5;
extern const struct nettle_hash nettle_openssl_sha1;

/* Tentative interface for "authenticated encryption with associated
   data" algorithms. Should be moved to nettle-meta.h when stable. */
struct nettle_aead
{
  const char *name;
  
  unsigned context_size;
  /* Block size of the input, and the size of the output digest */
  unsigned block_size;

  /* Suggested key size; other sizes are sometimes possible. */
  unsigned key_size;

  nettle_set_key_func *set_key;
  nettle_set_key_func *set_iv;
  nettle_hash_update_func *update;
  nettle_crypt_func *encrypt;
  nettle_crypt_func *decrypt;
  nettle_hash_digest_func *digest;
};

#define _NETTLE_AEAD(type, TYPE, name, key_size) {	\
  #type "-" #name #key_size,				\
  sizeof(struct type##_##name##_ctx),			\
  TYPE##_BLOCK_SIZE,					\
  key_size / 8,						\
  (nettle_set_key_func *) type##_##name##_set_key,	\
  (nettle_set_key_func *) type##_##name##_set_iv,	\
  (nettle_hash_update_func *) type##_##name##_update,	\
  (nettle_crypt_func *) type##_##name##_encrypt,	\
  (nettle_crypt_func *) type##_##name##_decrypt,	\
  (nettle_hash_digest_func *) type##_##name##_digest,	\
}

const struct nettle_aead nettle_gcm_aes128;
const struct nettle_aead nettle_gcm_aes192;
const struct nettle_aead nettle_gcm_aes256;

const struct nettle_aead nettle_gcm_camellia128;
const struct nettle_aead nettle_gcm_camellia192;
const struct nettle_aead nettle_gcm_camellia256;

const struct nettle_aead nettle_gcm_serpent128;
const struct nettle_aead nettle_gcm_serpent192;
const struct nettle_aead nettle_gcm_serpent256;

const struct nettle_aead nettle_gcm_twofish128;
const struct nettle_aead nettle_gcm_twofish192;
const struct nettle_aead nettle_gcm_twofish256;

#endif /* NETTLE_INTERNAL_H_INCLUDED */
