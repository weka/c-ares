/* Copyright (C) 2023 by Brad House
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
#ifndef __ARES__HTABLE_H
#define __ARES__HTABLE_H


/*! \addtogroup ares__htable Base HashTable Data Structure
 *
 * This is a basic hashtable data structure that is meant to be wrapped
 * by a higher level implementation.  This data structure is designed to
 * be callback-based in order to facilitate wrapping without needing to
 * worry about any underlying complexities of the hashtable implementation.
 *
 * This implementation supports automatic growing by powers of 2 when reaching
 * 75% capacity.  A rehash will be performed on the expanded bucket list.
 *
 * Average time complexity:
 *  - Insert: O(1)
 *  - Search: O(1)
 *  - Delete: O(1)
 *
 * @{
 */

struct ares__htable_t;

/*! Opaque data type for generic hash table implementation */
typedef struct ares__htable ares__htable_t;

/*! Callback for generating a hash of the key.
 * 
 *  \param[in] key   pointer to key to be hashed
 *  \param[in] seed  randomly generated seed used by hash function.
 *                   value is specific to the hashtable instance
 *                   but otherwise will not change between calls.
 *  \return hash
 */
typedef unsigned int (*ares__htable_hashfunc_t)(const void *key,
                                                unsigned int seed);

/*! Callback to free the bucket
 * 
 *  \param[in] bucket  user provided bucket
 */
typedef void (*ares__htable_bucket_free_t)(void *bucket);

/*! Callback to extract the key from the user-provided bucket
 * 
 *  \param[in] bucket  user provided bucket
 *  \return pointer to key held in bucket
 */
typedef const void *(*ares__htable_bucket_key_t)(const void *bucket);

/*! Callback to compare two keys for equality
 * 
 *  \param[in] key1  first key
 *  \param[in] key2  second key
 *  \return 1 if equal, 0 if not
 */
typedef unsigned int (*ares__htable_key_eq_t)(const void *key1,
                                              const void *key2);


/*! Destroy the initialized hashtable 
 * 
 *  \param[in] initialized hashtable
 */
void ares__htable_destroy(ares__htable_t *htable);

/*! Create a new hashtable
 * 
 *  \param[in] hash_func   Required. Callback for Hash function.
 *  \param[in] bucket_key  Required. Callback to extract key from bucket.
 *  \param[in] bucket_free Required. Callback to free bucket.
 *  \param[in] key_eq      Required. Callback to check for key equality.
 *  \return initialized hashtable.  NULL if out of memory or misuse.
 */
ares__htable_t *ares__htable_create(ares__htable_hashfunc_t    hash_func,
                                    ares__htable_bucket_key_t  bucket_key,
                                    ares__htable_bucket_free_t bucket_free,
                                    ares__htable_key_eq_t      key_eq);

/*! Count of keys from initialized hashtable
 *  
 *  \param[in] htable  Initialized hashtable.
 *  \return count of keys
 */
size_t ares__htable_num_keys(ares__htable_t *htable);

/*! Insert bucket into hashtable
 * 
 *  \param[in] htable  Initialized hashtable.
 *  \param[in] bucket  User-provided bucket to insert. Takes "ownership". Not
 *                     allowed to be NULL.
 *  \return 1 on success, 0 if out of memory
 */
unsigned int ares__htable_insert(ares__htable_t *htable, void *bucket);

/*! Retrieve bucket from hashtable based on key.
 * 
 *  \param[in] htable  Initialized hashtable
 *  \param[in] key     Pointer to key to use for comparison.
 *  \return matching bucket, or NULL if not found.
 */
void *ares__htable_get(ares__htable_t *htable, const void *key);

/*! Remove bucket from hashtable by key 
 * 
 *  \param[in] htable  Initialized hashtable
 *  \param[in] key     Pointer to key to use for comparison
 *  \return 1 if found, 0 if not found
 */
unsigned int ares__htable_remove(ares__htable_t *htable, const void *key);

/*! FNV1a hash algorithm.  Can be used as underlying primitive for building
 *  a wrapper hashtable.
 * 
 *  \param[in] key      pointer to key
 *  \param[in] key_len  Length of key
 *  \param[in] seed     Seed for generating hash
 *  \return hash value
 */
unsigned int ares__htable_hash_FNV1a(const void *key, size_t key_len,
                                     unsigned int seed);

/*! FNV1a hash algorithm, but converts all characters to lowercase before 
 *  hashing to make the hash case-insensitive. Can be used as underlying
 *  primitive for building a wrapper hashtable.  Used on string-based keys.
 * 
 *  \param[in] key      pointer to key
 *  \param[in] key_len  Length of key
 *  \param[in] seed     Seed for generating hash
 *  \return hash value
 */
unsigned int ares__htable_hash_FNV1a_casecmp(const void *key, size_t key_len,
                                             unsigned int);

/*! @} */

#endif /* __ARES__HTABLE_H */
