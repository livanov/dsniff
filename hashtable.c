#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "hashtable.h"
//#include "data_models.h"

/* Create a new hashtable. */
struct hashtable *ht_create( int size ) {

	struct hashtable *hashtable = NULL;
	int i;

	if( size < 1 ) return NULL;

	/* Allocate the table itself. */
	if( ( hashtable = malloc( sizeof( struct hashtable ) ) ) == NULL ) {
		return NULL;
	}

	/* Allocate pointers to the head nodes. */
	if( ( hashtable->table = malloc( sizeof( struct entry * ) * size ) ) == NULL ) {
		return NULL;
	}
	for( i = 0; i < size; i++ ) {
		hashtable->table[i] = NULL;
	}
	
	if( ( hashtable->keys = malloc( sizeof( char * ) * size ) ) == NULL ) {
		return NULL;
	}

	hashtable->size = size;
	hashtable->count = 0;

	return hashtable;	
}

void ht_free( struct hashtable *hashtable)
{
	free( hashtable-> keys );
	free( hashtable->table );
	free( hashtable );
}

/* Hash a string for a particular hash table. */
int ht_hash( struct hashtable *hashtable, char *key ) {

	unsigned long int hashval;
	int i = 0;

	/* Convert our string to an integer */
	while( hashval < ULONG_MAX && i < strlen( key ) ) {
		hashval = hashval << 8;
		hashval += key[ i ];
		i++;
	}

	return hashval % hashtable->size;
}

/* Create a key-value pair. */
struct entry *ht_newpair( char *key, void *value ) {
	struct entry *newpair;

	if( ( newpair = malloc( sizeof( struct entry ) ) ) == NULL ) {
		return NULL;
	}

	if( ( newpair->key = strdup( key ) ) == NULL ) {
		return NULL;
	}

	newpair->value = value;
	
	//if( ( newpair->value = strdup( value ) ) == NULL ) {
	//		return NULL;
	//}

	newpair->next = NULL;

	return newpair;
}

/* Insert a key-value pair into a hash table. */
void ht_set( struct hashtable *hashtable, char *key, void *value ) {
	int bin = 0;
	struct entry *newpair = NULL;
	struct entry *next = NULL;
	struct entry *last = NULL;

	bin = ht_hash( hashtable, key );

	next = hashtable->table[ bin ];

	while( next != NULL && next->key != NULL && strcmp( key, next->key ) > 0 ) {
		last = next;
		next = next->next;
	}

	/* There's already a pair.  Let's replace that string. */
	if( next != NULL && next->key != NULL && strcmp( key, next->key ) == 0 ) {

		//free( next->value );
		//next->value = strdup( value );
		next->value = value;

	/* Nope, could't find it.  Time to grow a pair. */
	} else {
		newpair = ht_newpair( key, value );

		/* We're at the start of the linked list in this bin. */
		if( next == hashtable->table[ bin ] ) {
			newpair->next = next;
			hashtable->table[ bin ] = newpair;
	
		/* We're at the end of the linked list in this bin. */
		} else if ( next == NULL ) {
			last->next = newpair;
	
		/* We're in the middle of the list. */
		} else  {
			newpair->next = next;
			last->next = newpair;
		}
		
		hashtable->keys[hashtable->count] = malloc ( sizeof ( key ) );
		strcpy(hashtable->keys[hashtable->count], key);
		hashtable->count++;
	}
	
}

/* Retrieve a key-value pair from a hash table. */
void *ht_get( struct hashtable *hashtable, char *key ) {
	int bin = 0;
	struct entry *pair;

	bin = ht_hash( hashtable, key );

	/* Step through the bin, looking for our value. */
	pair = hashtable->table[ bin ];
	while( pair != NULL && pair->key != NULL && strcmp( key, pair->key ) > 0 ) {
		pair = pair->next;
	}

	/* Did we actually find anything? */
	if( pair == NULL || pair->key == NULL || strcmp( key, pair->key ) != 0 ) {
		return NULL;

	} else {
		return pair->value;
	}
}

char ** ht_get_keys( struct hashtable *hashtable ) {
	return hashtable->keys;
}

int ht_get_count( struct hashtable *hashtable ) {
	return hashtable->count;
}

/*
int main()
{
	struct hashtable *hashtable = ht_create( 65536 );
	
	ht_set( hashtable, "key1", "inky" );
	ht_set( hashtable, "key2", "pinky" );
	ht_set( hashtable, "key3", "blinky" );
	//ht_set( hashtable, "key4", "floyd" );

	printf( "%d\n", hashtable->count);
	short i;
	for ( i = 0 ; i < hashtable->count ; i++)
		printf("%s\n", hashtable->keys[i]);
	//printf( "%s\n", ht_get( hashtable, "key1" ) );
	//printf( "%s\n", ht_get( hashtable, "key2" ) );
	//printf( "%s\n", ht_get( hashtable, "key3" ) );
	//printf( "%s\n", ht_get( hashtable, "key4" ) );

	//int bin = ht_hash(hashtable, "key1");
	//struct entry *ent = hashtable->table[bin];
	//printf("%s\n", ent);
	
	return 0;
}
*/