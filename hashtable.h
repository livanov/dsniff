struct entry {
	char 			*key;
	void 			*value;
	struct entry 	*next;
};

struct hashtable {
	int 			size;
	int				count;
	struct entry 	**table;	
	char 			**keys;
};

struct hashtable *ht_create( int size ) ;

void ht_set( struct hashtable *hashtable, char *key, void *value );

void *ht_get( struct hashtable *hashtable, char *key );

char ** ht_get_keys( struct hashtable *hashtable );

int ht_get_count( struct hashtable *hashtable ) ;