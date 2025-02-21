/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2007-2013 by Daniel Stenberg
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"
#include "ares_nowarn.h"
#include <stdlib.h>

#if !defined(HAVE_ARC4RANDOM_BUF) && !defined(HAVE_GETRANDOM) && !defined(_WIN32)
#  define ARES_NEEDS_RC4 1
#endif

typedef enum  {
  ARES_RAND_OS   = 1,  /* OS-provided such as RtlGenRandom or arc4random */
  ARES_RAND_FILE = 2,  /* OS file-backed random number generator */
#ifdef ARES_NEEDS_RC4
  ARES_RAND_RC4  = 3   /* Internal RC4 based PRNG */
#endif
} ares_rand_backend;


/* Don't build RC4 code if it goes unused as it will generate dead code
 * warnings */
#ifdef ARES_NEEDS_RC4
#  define ARES_RC4_KEY_LEN 32 /* 256 bits */

typedef struct ares_rand_rc4
{
  unsigned char S[256];
  size_t        i;
  size_t        j;
} ares_rand_rc4;


#ifdef _MSC_VER
typedef unsigned __int64 cares_u64;
#else
typedef unsigned long long cares_u64;
#endif


static unsigned int ares_u32_from_ptr(void *addr)
{
    if (sizeof(void *) == 8) {
        return (unsigned int)((((cares_u64)addr >> 32) & 0xFFFFFFFF) | ((cares_u64)addr & 0xFFFFFFFF));
    }
    return (unsigned int)((size_t)addr & 0xFFFFFFFF);
}


/* initialize an rc4 key as the last possible fallback. */
static void ares_rc4_generate_key(ares_rand_rc4 *rc4_state, unsigned char *key, size_t key_len)
{
  size_t         i;
  size_t         len = 0;
  unsigned int   data;
  struct timeval tv;

  if (key_len != ARES_RC4_KEY_LEN)
    return;

  /* Randomness is hard to come by.  Maybe the system randomizes heap and stack addresses.
   * Maybe the current timestamp give us some randomness.
   * Use  rc4_state (heap), &i (stack), and ares__tvnow()
   */
  data = ares_u32_from_ptr(rc4_state);
  memcpy(key + len, &data, sizeof(data));
  len += sizeof(data);

  data = ares_u32_from_ptr(&i);
  memcpy(key + len, &data, sizeof(data));
  len += sizeof(data);

  tv = ares__tvnow();
  data = (unsigned int)((tv.tv_sec | tv.tv_usec) & 0xFFFFFFFF);
  memcpy(key + len, &data, sizeof(data));
  len += sizeof(data);

  srand(ares_u32_from_ptr(rc4_state) | ares_u32_from_ptr(&i) | (unsigned int)((tv.tv_sec | tv.tv_usec) & 0xFFFFFFFF));

  for (i=len; i<key_len; i++) {
    key[i]=(unsigned char)(rand() % 256);  /* LCOV_EXCL_LINE */
  }
}


static void ares_rc4_init(ares_rand_rc4 *rc4_state)
{
  unsigned char key[ARES_RC4_KEY_LEN];
  size_t        i;
  size_t        j;

  ares_rc4_generate_key(rc4_state, key, sizeof(key));

  for (i = 0; i < sizeof(rc4_state->S); i++) {
    rc4_state->S[i] = i & 0xFF;
  }

  for(i = 0, j = 0; i < 256; i++) {
    j = (j + rc4_state->S[i] + key[i % sizeof(key)]) % 256;
    ARES_SWAP_BYTE(&rc4_state->S[i], &rc4_state->S[j]);
  }

  rc4_state->i = 0;
  rc4_state->j = 0;
}


/* Just outputs the key schedule, no need to XOR with any data since we have none */
static void ares_rc4_prng(ares_rand_rc4 *rc4_state, unsigned char *buf, size_t len)
{
  unsigned char *S = rc4_state->S;
  size_t         i = rc4_state->i;
  size_t         j = rc4_state->j;
  size_t         cnt;

  for (cnt=0; cnt<len; cnt++) {
    i = (i + 1) % 256;
    j = (j + S[i]) % 256;

    ARES_SWAP_BYTE(&S[i], &S[j]);
    buf[cnt] = S[(S[i] + S[j]) % 256];
  }

  rc4_state->i = i;
  rc4_state->j = j;
}

#endif /* ARES_NEEDS_RC4 */


struct ares_rand_state
{
  ares_rand_backend type;
  union {
    FILE *rand_file;
#ifdef ARES_NEEDS_RC4
    ares_rand_rc4 rc4;
#endif
  } state;

  /* Since except for RC4, random data will likely result in a syscall, lets
   * pre-pull 256 bytes at a time.  Every query will pull 2 bytes off this so
   * that means we should only need a syscall every 128 queries. 256bytes
   * appears to be a sweet spot that may be able to be served without
   * interruption */
  unsigned char     cache[256];
  size_t            cache_remaining;
};


/* Define RtlGenRandom = SystemFunction036.  This is in advapi32.dll.  There is
 * no need to dynamically load this, other software used widely does not.
 * http://blogs.msdn.com/michael_howard/archive/2005/01/14/353379.aspx
 * https://docs.microsoft.com/en-us/windows/win32/api/ntsecapi/nf-ntsecapi-rtlgenrandom
 */
#ifdef _WIN32
BOOLEAN WINAPI SystemFunction036(PVOID RandomBuffer, ULONG RandomBufferLength);
#  ifndef RtlGenRandom
#    define RtlGenRandom(a,b) SystemFunction036(a,b)
#  endif
#endif


static int ares__init_rand_engine(ares_rand_state *state)
{
  memset(state, 0, sizeof(*state));

#if defined(HAVE_ARC4RANDOM_BUF) || defined(HAVE_GETRANDOM) || defined(_WIN32)
  state->type = ARES_RAND_OS;
  return 1;
#elif defined(CARES_RANDOM_FILE)
  state->type            = ARES_RAND_FILE;
  state->state.rand_file = fopen(CARES_RANDOM_FILE, "rb");
  if (state->state.rand_file) {
    setvbuf(state->state.rand_file, NULL, _IONBF, 0);
    return 1;
  }
  /* Fall-Thru on failure to RC4 */
#endif

#ifdef ARES_NEEDS_RC4
  state->type = ARES_RAND_RC4;
  ares_rc4_init(&state->state.rc4);

  /* Currently cannot fail */
  return 1;
#endif
}


ares_rand_state *ares__init_rand_state(void)
{
  ares_rand_state *state = NULL;

  state = ares_malloc(sizeof(*state));
  if (!state)
    return NULL;

  if (!ares__init_rand_engine(state)) {
    ares_free(state);
    return NULL;
  }

  return state;
}


static void ares__clear_rand_state(ares_rand_state *state)
{
  if (!state)
    return;

  switch (state->type) {
    case ARES_RAND_OS:
      break;
    case ARES_RAND_FILE:
      fclose(state->state.rand_file);
      break;
#ifdef ARES_NEEDS_RC4
    case ARES_RAND_RC4:
      break;
#endif
  }
}


static void ares__reinit_rand(ares_rand_state *state)
{
  ares__clear_rand_state(state);
  ares__init_rand_engine(state);
}


void ares__destroy_rand_state(ares_rand_state *state)
{
  if (!state)
    return;

  ares__clear_rand_state(state);
  ares_free(state);
}


static void ares__rand_bytes_fetch(ares_rand_state *state, unsigned char *buf,
                                   size_t len)
{

  while (1) {
    size_t bytes_read = 0;

    switch (state->type) {
      case ARES_RAND_OS:
#ifdef _WIN32
        RtlGenRandom(buf, len);
        return;
#elif defined(HAVE_ARC4RANDOM_BUF)
        arc4random_buf(buf, len);
        return;
#elif defined(HAVE_GETRANDOM)
        while (1) {
          size_t n = len - bytes_read;
          /* getrandom() on Linux always succeeds and is never
           * interrupted by a signal when requesting <= 256 bytes.
           */
          ssize_t rv = getrandom(buf + bytes_read, n > 256 ? 256 : n, 0);
          if (rv <= 0)
            continue; /* Just retry. */

          bytes_read += rv;
          if (bytes_read == len)
            return;
        }
        break;
#else
        /* Shouldn't be possible to be here */
        break;
#endif

      case ARES_RAND_FILE:
        while (1) {
          size_t rv = fread(buf + bytes_read, 1, len - bytes_read, state->state.rand_file);
          if (rv == 0)
            break; /* critical error, will reinit rand state */

          bytes_read += rv;
          if (bytes_read == len)
            return;
        }
        break;

#ifdef ARES_NEEDS_RC4
      case ARES_RAND_RC4:
        ares_rc4_prng(&state->state.rc4, buf, len);
        return;
#endif
    }

    /* If we didn't return before we got here, that means we had a critical rand
     * failure and need to reinitialized */
    ares__reinit_rand(state);
  }
}


void ares__rand_bytes(ares_rand_state *state, unsigned char *buf, size_t len)
{
  /* See if we need to refill the cache to serve the request, but if len is
   * excessive, we're not going to update our cache or serve from cache */
  if (len > state->cache_remaining && len < sizeof(state->cache)) {
    size_t fetch_size = sizeof(state->cache) - state->cache_remaining;
    ares__rand_bytes_fetch(state, state->cache, fetch_size);
    state->cache_remaining = sizeof(state->cache);
  }

  /* Serve from cache */
  if (len <= state->cache_remaining) {
    size_t offset = sizeof(state->cache) - state->cache_remaining;
    memcpy(buf, state->cache + offset, len);
    state->cache_remaining -= len;
    return;
  }

  /* Serve direct due to excess size of request */
  ares__rand_bytes_fetch(state, buf, len);
}


unsigned short ares__generate_new_id(ares_rand_state *state)
{
  unsigned short r=0;

  ares__rand_bytes(state, (unsigned char *)&r, sizeof(r));
  return r;
}

