/*
 *  kexecboot - A kexec based bootloader
 *  Hash library with exhaustive and binary search
 *
 *  Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _HAVE_HASH_H_
#define _HAVE_HASH_H_

#include <stdlib.h>
#include <string.h>

/* Hash key type */
typedef unsigned int hkey_t;

/* Hash object structure */
struct hdata_t {
	hkey_t key;
	unsigned int size;
	void *data;
};

/* Hash storage structure */
struct htable_t {
	unsigned int size;		/* Current size of table */
	unsigned int step;		/* Increment table with specified step on overflow */
	unsigned int count;		/* Current items count */
	struct hdata_t *afterlast;	/* Next item after last - speedup */
	struct hdata_t *hdata;	/* Items array */
};

/*
 * Functions: hkey_crc32(), hkey_char()
 * Create hash key from data with crc32 algorythm.
 * Args:
 * - data from which hash should be created
 * - size of data for hashing
 * Return value: hash key value.
 */
hkey_t hkey_crc32(void *data, unsigned int size);

/*
 * Function: htable_create()
 * Create and initialize hash table with specified size and step
 * Args:
 * - starting size of hash table
 * - step for increasing table when overflowed
 * Return value:
 * - pointer to new hash table structure on success
 * - NULL on error
 * Should be freed with htable_destroy()
 */
struct htable_t *htable_create(unsigned int start_size, unsigned int step);

/*
 * Function: htable_destroy()
 * Destroy hash table
 * Args:
 * - hash table structure
 * Return value: None
 */
void htable_destroy(struct htable_t *htable);


/*
 * 'plain' functions - exhaustive search
 */
#ifndef NO_HASH_PLAIN_FUNC

/*
 * Function: htable_plain_search()
 * Search key in hash table and return data
 * Args:
 * - hash table structure
 * - key value
 * Return value:
 * - hash object structure on success
 * - NULL when no such key found
 */
struct hdata_t *htable_plain_search(struct htable_t *htable, hkey_t key);

/*
 * Function: htable_plain_insert()
 * Add data to hash table
 * Args:
 * - hash table structure
 * - key value
 * - data to store
 * - size of data to store
 * Return value:
 * -  0 when key is already exists
 * -  key value (>0) on success
 * - -1 on error (e.g. no memory)
 * NOTE: when key == 0 then key will be created from data by make_hash()
 */
int htable_plain_insert(struct htable_t *htable, hkey_t key, void *data, unsigned int size);

/*
 * Function: htable_plain_remove()
 * Remove datа from hash table
 * Args:
 * - hash table structure
 * - key value to remove
 * Return value:
 * -  0 on success
 * - -1 when no such key found
 */
int htable_plain_remove(struct htable_t *htable, hkey_t key);
#endif	// NO_HASH_PLAIN_FUNC

/*
 * 'bin' functions - binary search
 */
#ifndef NO_HASH_BIN_FUNC

/*
 * Function: htable_bin_search()
 * Search key in hash table and return data
 * Args:
 * - hash table structure
 * - key value
 * Return value:
 * - hash object structure on success
 * - NULL when no such key found
 */
struct hdata_t *htable_bin_search(struct htable_t *htable, hkey_t key);

/*
 * Function: htable_bin_insert()
 * Add data to hash table
 * Args:
 * - hash table structure
 * - key value
 * - data to store
 * - size of data to store
 * Return value:
 * -  0 when key is already exists
 * -  key value (>0) on success
 * - -1 on error (e.g. no memory)
 * NOTE: when key == 0 then key will be created from data by make_hash()
 */
int htable_bin_insert(struct htable_t *htable, hkey_t key, void *data, unsigned int size);

/*
 * Function: htable_bin_remove()
 * Remove datа from hash table
 * Args:
 * - hash table structure
 * - key value to remove
 * Return value:
 * -  0 on success
 * - -1 when no such key found
 */
int htable_bin_remove(struct htable_t *htable, hkey_t key);
#endif	// NO_HASH_BIN_FUNC

#endif	// _HAVE_HASH_H_
