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
#ifndef __ARES__HTABLE_STVP_H
#define __ARES__HTABLE_STVP_H

/*! \addtogroup ares__htable_stvp HashTable with size_t Key and void pointer Value
 *
 * This data structure wraps the base ares__htable data structure in order to
 * split the key and value data types as size_t and void pointer, respectively.
 *
 * Average time complexity:
 *  - Insert: O(1)
 *  - Search: O(1)
 *  - Delete: O(1)
 *
 * @{
 */

struct ares__htable_stvp;

/*! Opaque data type for size_t key, void pointer hash table implementation */
typedef struct ares__htable_stvp ares__htable_stvp_t;

/*! Callback to free value stored in hashtable
 * 
 *  \param[in] val  user-supplied value
 */
typedef void (*ares__htable_stvp_val_free_t)(void *val);

/*! Destroy hashtable
 * 
 *  \param[in] htable  Initialized hashtable
 */
void ares__htable_stvp_destroy(ares__htable_stvp_t *htable);

/*! Create size_t key, void pointer value hash table
 * 
 *  \param[in] val_free  Optional. Call back to free user-supplied value.  If
 *                       NULL it is expected the caller will clean up any user
 *                       supplied values.
 */
ares__htable_stvp_t *ares__htable_stvp_create(
    ares__htable_stvp_val_free_t val_free);

/*! Insert key/value into hash table
 * 
 *  \param[in] htable Initialized hash table
 *  \param[in] key    key to associate with value
 *  \param[in] val    value to store (takes ownership). May be NULL.
 *  \return 1 on success, 0 on out of memory or misuse
 */
unsigned int ares__htable_stvp_insert(ares__htable_stvp_t *htable, size_t key,
                                      void *val);

/*! Retrieve value from hashtable based on key
 * 
 *  \param[in]  htable  Initialized hash table
 *  \param[in]  key     key to use to search
 *  \param[out] val     Optional.  Pointer to store value.
 *  \return 1 on success, 0 on failure
 */
unsigned int ares__htable_stvp_get(ares__htable_stvp_t *htable, size_t key,
                                   void **val);

/*! Retrieve value from hashtable directly as return value.  Caveat to this
 *  function over ares__htable_stvp_get() is that if a NULL value is stored
 *  you cannot determine if the key is not found or the value is NULL.
 * 
 *  \param[in] htable  Initialized hash table
 *  \param[in] key     key to use to search
 *  \return value associated with key in hashtable or NULL
 */
void *ares__htable_stvp_get_direct(ares__htable_stvp_t *htable, size_t key);

/*! Remove a value from the hashtable by key
 * 
 *  \param[in] htable  Initialized hash table
 *  \param[in] key     key to use to search
 *  \return 1 if found, 0 if not
 */
unsigned int ares__htable_stvp_remove(ares__htable_stvp_t *htable, size_t key);

/*! Retrieve the number of keys stored in the hash table
 * 
 *  \param[in] htable  Initialized hash table
 *  \return count
 */
size_t ares__htable_stvp_num_keys(ares__htable_stvp_t *htable);

/*! @} */

#endif /* __ARES__HTABLE_STVP_H */
