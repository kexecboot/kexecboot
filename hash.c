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

#include "hash.h"

/* Create hash key from data */
hkey_t make_hash(void *data, unsigned int size)
{
	static hkey_t hash_key;
	static unsigned char *p;

	hash_key = 0;
	p = (unsigned char *)data;
	
	while (p < (unsigned char *)data + size) {
		hash_key += *p;
		++p;
	}
	return hash_key;
}


/* Create hash table with specified size and step */
struct htable_t *htable_create(unsigned int start_size, unsigned int step)
{
	struct htable_t *htable;
	
	htable = (struct htable_t *)malloc(sizeof(struct htable_t));
	if (NULL == htable) {
		return NULL;
	}
	
	htable->size = start_size;
	htable->step = step;
	htable->count = 0;
	htable->hdata = (struct hdata_t *)malloc(start_size * sizeof(struct hdata_t));
	if (NULL == htable->hdata) {
		free(htable);
		return NULL;
	}
	htable->afterlast = htable->hdata;
	
	return htable;
}


/* Destroy hash table */
void htable_destroy(struct htable_t *htable)
{
	struct hdata_t *p, *e;
	
	p = htable->hdata;
	e = htable->afterlast;
	
	while (p < e) {
		if (NULL != p->data) free(p->data);
		++p;
	}
	
	free(htable->hdata);
	free(htable);
}


/*
 * 'plain' functions - exhaustive search
 */
#ifndef NO_HASH_PLAIN_FUNC

/* Search key in hash table and return data */
struct hdata_t *htable_plain_search(struct htable_t *htable, hkey_t key)
{
	static struct hdata_t *p, *e;
	
	p = htable->hdata;
	e = htable->afterlast;
	
	while (p < e) {
		if (p->key == key) {
			/* Found */
			return p;
		}
		++p;
	}
	
	return NULL;
}


/* Add data to hash table */
int htable_plain_insert(struct htable_t *htable, hkey_t key, void *data, unsigned int size)
{
	hkey_t lkey;
	unsigned int rest;
	struct hdata_t *hdata;
	void *p;
	
	/* Check memory amount */
	if (htable->count >= htable->size) {
		/* We don't have enough free space - reallocating */
		rest = htable->afterlast - htable->hdata;
		hdata = (struct hdata_t *)realloc(htable->hdata, (htable->size + htable->step) * sizeof(*hdata));
		if(NULL != hdata) {
			htable->hdata = hdata;
			htable->afterlast = hdata + rest;	/* Relocating afterlast */
			htable->size += htable->step;
		} else {
			/* Can't reallocate */
			return -1;
		}
	}
	
	/* Check key */
	if (0 == key) {
		/* We should create key from data */
		lkey = make_hash(data, size);
	} else {
		/* Use supplied key */
		lkey = key;
	}
	
	/* Search key in table */
	hdata = htable_plain_search(htable, key);
	if (NULL != hdata) {
		/* Key already exists - give up */
		return 0;
	}
	
	/* Go to end of array */
	hdata = htable->afterlast;
	
	++htable->count;
	++htable->afterlast;
	
	/* Store data */
	hdata->key = lkey;
	hdata->size = size;
	p = malloc(size);
	if (NULL == p) {
		return -1;
	}
	memcpy(p, data, size);
	hdata->data = p;
	
	return lkey;
}


/* Remove datа from hash table */
int htable_plain_remove(struct htable_t *htable, hkey_t key)
{
	struct hdata_t *p;
	unsigned int rest;
	
	/* Search structure to remove */
	p = htable_plain_search(htable, key);
	if (NULL == p) {
		/* We have no such key */
		return -1;
	}
	/* Free stored data */
	free(p->data);
	
	/* Move rest of array left */
	rest = htable->count - (p - htable->hdata) - 1;
	memmove(p, p + 1, rest * sizeof(*p));
	
	--htable->count;
	--htable->afterlast;
	
	return 0;
}
#endif	// NO_HASH_PLAIN_FUNC

/*
 * 'bin' functions - binary search
 */
#ifndef NO_HASH_BIN_FUNC

/* Internal function: Search nearest key in hash table and return data */
struct hdata_t *htable_bin_search_nearest(struct htable_t *htable, hkey_t key)
{
	static int half, rest;
	static struct hdata_t *p;

	if (0 == htable->count) return NULL;
	
	half = htable->count >> 1;
	rest = htable->count - half;

	p = htable->hdata + half;

	while (half > 0) {
		half = rest >> 1;
		if (key < p->key) {
			p -= half;
		} else if (key > p->key) {
			p += half;
		} else {
			break;
		}
		rest -= half;
	}
	
	return p;
}


/* Search key in hash table and return data */
struct hdata_t *htable_bin_search(struct htable_t *htable, hkey_t key)
{
	static struct hdata_t *p;
	
	p = htable_bin_search_nearest(htable, key);
	if (NULL == p)
		return NULL;
	if (p->key == key) {
		return p;
	} else {
		return NULL;
	}
}


/* Add data to hash table */
int htable_bin_insert(struct htable_t *htable, hkey_t key, void *data, unsigned int size)
{
	hkey_t lkey;
	unsigned int rest;
	struct hdata_t *hdata, *tmp;
	
	/* Check memory amount */
	if (htable->count >= htable->size) {
		/* We don't have enough free space - reallocating */
		rest = htable->afterlast - htable->hdata;
		hdata = (struct hdata_t *)realloc(htable->hdata, (htable->size + htable->step) * sizeof(*hdata));
		if(NULL != hdata) {
			htable->hdata = hdata;
			htable->afterlast = hdata + rest;	/* Relocating afterlast */
			htable->size += htable->step;
		} else {
			/* Can't reallocate */
			return -1;
		}
	}
	
	/* Check key */
	if (0 == key) {
		/* We should create key from data */
		lkey = make_hash(data, size);
	} else {
		/* Use supplied key */
		lkey = key;
	}
	
	if (0 != htable->count) {
		/* Search key in data */
		hdata = htable_bin_search_nearest(htable, key);

		if (hdata->key == key) {
			/* Key already exists - give up */
			return 0;
		}
		
		/* Free place for new item */
		if (hdata->key < key) {
			/* We found item before - left it*/
			++hdata;
		}
		
		/* Move rest of array right */
		rest = htable->count - (hdata - htable->hdata);
		memmove(hdata + 1, hdata, rest * sizeof(*hdata));
	} else {
		hdata = htable->hdata;
	}
	
	++htable->count;
	++htable->afterlast;
	
	/* Store data */
	hdata->key = lkey;
	hdata->size = size;
	tmp = malloc(size);
	if (NULL == tmp) {
		return -1;
	}
	memcpy(tmp, data, size);
	hdata->data = tmp;
	
	return lkey;
}


/* Remove datа from hash table */
int htable_bin_remove(struct htable_t *htable, hkey_t key)
{
	struct hdata_t *p;
	unsigned int rest;
	
	/* Search structure to remove */
	p = htable_bin_search(htable, key);
	if (NULL == p) {
		/* We have no such key */
		return -1;
	}
	
	/* Free stored data */
	free(p->data);
	
	/* Move rest of array left */
	rest = htable->count - (p - htable->hdata) - 1;
	memmove(p, p + 1, rest * sizeof(*p));
	
	--htable->count;
	--htable->afterlast;
	
	return 0;
}

#endif // NO_HASH_BIN_FUNC
