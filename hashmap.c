#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hashmap.h"

static unsigned gethash(const char *szKey)
{
	const unsigned char *us;
	unsigned h = 0;
	for( us=(unsigned const char *)szKey ; *us ; us++ )
		h = 37 * h + *us;
	return h;
}

void dict_init(dict *d)
{
	if( !d )
		return;
	
	d->table = NULL;
	d->size = d->count = 0;
}

void dict_free(dict *d, const bool free_kv_data)
{
	if( !d )
		return;
	
	/*
	 * If the dictionary pointer is not "const", then
	 * you have to make two traversing kvnode_t pointers
	 * or else you'll get a nice little segfault ;)
	 * not sure why but whatever makes the code work I guess.
	*/
	kvnode_t	*kv = NULL,
			*next = NULL;
	
	for( unsigned i=0 ; i<d->size ; ++i ) {
		for( kv = d->table[i] ; kv ; kv = next ) {
			next = kv->pNext;
			if( free_kv_data && kv->pData ) {
				free(kv->pData);
				kv->pData = NULL;
			}
			free(kv);
			kv = NULL;
		}
	}
	if( d->table )
		free(d->table);
	dict_init(d);
}

bool dict_insert(dict *restrict d, const char *restrict szKey, void *restrict pData)
{
	if( !d )
		return false;
	
	if( d->size == 0 ) {
		d->size = 8;
		d->table = calloc(d->size, sizeof(kvnode_t));
		
		if( !d->table ) {
			printf("**** Memory Allocation Error **** dict_insert::d->table is NULL\n");
			d->size = 0;
			return false;
		}
	}
	else if( d->count >= d->size ) {
		dict_rehash(d);
		//printf("**** Rehashed Dictionary ****\n");
		//printf("**** Dictionary Size is now %llu ****\n", d->size);
	}
	else if( dict_has_key(d, szKey) ) {
		printf("dict_insert::d already has entry!\n");
		return false;
	}
	
	kvnode_t *node = malloc( sizeof(kvnode_t) );
	if( !node ) {
		printf("**** Memory Allocation Error **** dict_insert::node is NULL\n");
		return false;
	}
	node->strKey = szKey;
	node->pData = pData;
	
	unsigned hash = gethash(szKey) % d->size;
	node->pNext = d->table[hash];
	d->table[hash] = node;
	++d->count;
	return true;
}

void *dict_find(const dict *restrict d, const char *restrict szKey)
{
	if( !d )
		return NULL;
	else if( !d->table )
		return NULL;
	/*
	 * if dictionary pointer is const, you only
	 * need to use one traversing kvnode_t
	 * pointer without worrying of a segfault
	*/
	kvnode_t *kv;
	unsigned hash = gethash(szKey) % d->size;
	for( kv = d->table[hash] ; kv ; kv = kv->pNext )
		if( !strcmp(kv->strKey, szKey) )
			return kv->pData;
	return NULL;
}

void dict_delete(dict *restrict d, const char *restrict szKey)
{
	if( !d )
		return;
	
	if( !dict_has_key(d, szKey) )
		return;
	
	unsigned hash = gethash(szKey) % d->size;
	kvnode_t	*kv = NULL,
			*next = NULL;
	
	for( kv = d->table[hash] ; kv ; kv = next ) {
		next = kv->pNext;
		if( !strcmp(kv->strKey, szKey) ) {
			d->table[hash] = kv->pNext;
			free(kv);
			kv = NULL;
			d->count--;
		}
	}
}

bool dict_has_key(const dict *restrict d, const char *restrict szKey)
{
	if( !d )
		return false;
	
	kvnode_t *prev;
	unsigned hash = gethash(szKey) % d->size;
	for( prev = d->table[hash] ; prev ; prev = prev->pNext )
		if( !strcmp(prev->strKey, szKey) )
			return true;
	
	return false;
}

unsigned dict_len(const dict *d)
{
	if( !d )
		return 0L;
	return d->count;
}

// Rehashing increases dictionary size by a factor of 2
void dict_rehash(dict *d)
{
	if( !d )
		return;
	
	unsigned old_size = d->size;
	d->size <<= 1;
	d->count = 0;
	
	kvnode_t **curr, **temp;
	temp = calloc(d->size, sizeof(kvnode_t));
	if( !temp ) {
		printf("**** Memory Allocation Error **** dict_insert::temp is NULL\n");
		d->size = 0;
		return;
	}
	
	curr = d->table;
	d->table = temp;
	
	kvnode_t	*kv = NULL,
			*next = NULL;
	
	for( unsigned i=0 ; i<old_size ; ++i ) {
		if( !curr[i] )
			continue;
		for( kv = curr[i] ; kv ; kv = next ) {
			next = kv->pNext;
			dict_insert(d, kv->strKey, kv->pData);
			// free the inner nodes since they'll be re-hashed
			free(kv);
			kv = NULL;
			printf("**** Rehashed Entry ****\n");
		}
	}
	if( curr )
		free(curr);
	curr = NULL;
}

/*
unsigned long gethash(const char *szKey)
{
	char *us;
	unsigned long h = 0;
	for (us = (char *)szKey; *us; us++)
		h = h * 147 + *us;

	return h;
}
*/