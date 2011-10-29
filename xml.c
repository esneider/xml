/**
 * @file xml.c
 *
 * TODO:
 * + Do not ignote <!> & <?> tags --> read_special_tag
 * + Handle namespaces
 * + Handle [] in XPath
 * + Handle this:
 * There are 5 predefined entity references in XML:
 *
 * &lt;		<	less than
 * &gt;		>	greater than
 * &amp;	&	ampersand
 * &apos;	'	apostrophe
 * &quot;	"	quotation mark
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "xml.h"


enum STATE {
	OK,
	OPEN_TAG, CLOSE_TAG, ISOLATED_TAG, OTHER_TAG,
	PARSE_ERROR, MEMORY_ERROR
};


#define IS_ELEMENT_STATUS       1
#define IS_ATTRIBUTE_STATUS     2
#define IS_META_ROOT_STATUS     4
#define IS_TOUCHED_STATUS       8
#define IS_NAMESPACE_STATUS    16
#define IS_COMMENT_STATUS      32
#define IS_INSTRUCTION_STATUS  64
#define IF_TEXT_STATUS        128


#define TRIE_HEAD_LETTER   (~0)


static const struct string {

	int len;
	int max_len;
	char* str;

} init_str = { 0, 0, NULL };


static enum STATE str_push_back ( int c, struct string* str ) {

	if ( str->len < 2 || str->len - 2 < str->max_len ) {

		str->max_len = 2*str->len + 2;

		char* aux = realloc( str->str, str->max_len );
		if ( !aux ) return MEMORY_ERROR;

		str->str = aux;
	}

	str->str[ str->len++ ] = (char)c;
	str->str[ str->len ] = 0;
	return OK;
}


static const struct ptr_list {

	int len;
	int max_len;
	void** list;

} init_ptr_list = { 0, 0, NULL };


static enum STATE ptr_list_push_back ( void* p, struct ptr_list* ptrl ) {

	if ( ptrl->len < 2 || ptrl->len - 2 < ptrl->max_len ) {

		ptrl->max_len = 2*ptrl->len + 2;

		void* aux = realloc( ptrl->list, ptrl->max_len * sizeof( void* ) );
		if ( !aux ) return MEMORY_ERROR;

		ptrl->list = aux;
	}

	ptrl->list[ ptrl->len++ ] = p;
	ptrl->list[ ptrl->len ] = NULL;
	return OK;
}


static void skip_space ( FILE* file ) {

	int c;
	do {
		c = fgetc( file );
		if ( c == EOF ) return;
	} while ( isspace(c) );

	ungetc( c, file );
}


static void str_remove_trail_space ( struct string* str ) {

	for ( ; str->len && isspace( str->str[ str->len - 1 ] ); str->len-- ) ;

	char* aux = realloc( str->str, str->len + 1 );
	if ( aux ) str->str = aux;

	str->str[ str->len ] = 0;
}


static enum STATE read_attr_name ( FILE* file, struct xml_attribute* attr ) {

	struct string name = init_str;
	char c;

	while ( ( c = fgetc( file ) ) != '=' && !isspace(c) ) {

		if ( c == EOF || str_push_back( c, &name ) != OK ) {
			free( name.str );
			return ( c == EOF ) ? PARSE_ERROR : MEMORY_ERROR;
		}
	}

	str_remove_trail_space( &name );
	attr->name = name.str;

	ungetc( c, file );
	skip_space( file );

	return OK;
}


static enum STATE read_attr_value ( FILE* file, struct xml_attribute* attr ) {

	char d = fgetc( file );
	if ( d != '\'' && d != '"' ) return PARSE_ERROR;

	struct string value = init_str;
	char c;

	while ( ( c = fgetc( file ) ) != d ) {

		if ( c == EOF || str_push_back( c, &value ) != OK ) {
			free( value.str );
			return ( c == EOF ) ? PARSE_ERROR : MEMORY_ERROR;
		}
	}

	str_remove_trail_space( &value );
	attr->value = value.str;

	return OK;
}


static enum STATE read_attr ( FILE* file, struct xml_element* elem ) {

	enum STATE state;

	skip_space( file );

	int c = fgetc( file );

	if ( c == EOF || c == '=' ) return PARSE_ERROR;

	if ( c == '/' ) {
		skip_space( file );
		c = fgetc( file );
		return ( c == '>' ) ? ISOLATED_TAG : PARSE_ERROR;
	}

	if ( c == '>' ) return OPEN_TAG;

	ungetc( c, file );

	struct xml_attribute* attr = calloc( 1, sizeof( struct xml_attribute ) );
	if ( !attr ) return MEMORY_ERROR;

	attr->father = elem;
	attr->status = IS_ATTRIBUTE_STATUS;

	state = read_attr_name( file, attr );
	if ( state != OK ) {
		free( attr );
		return state;
	}

	c = fgetc( file );
	if ( c != '=' ) {
		free( attr->name );
		free( attr );
		return PARSE_ERROR;
	}
	skip_space( file );

	state = read_attr_value( file, attr );
	if ( state != OK ) {
		free( attr->name );
		free( attr );
		return state;
	}
	state = read_attr( file, elem );

	attr->next = elem->attr;
	elem->attr = attr;
	if ( attr->next ) attr->next->prev = attr;

	return state;
}


static enum STATE read_special_tag ( FILE* file, struct xml_element* elem ) {

	(void)elem;

	for ( int open = 1; open; ) {
		char c = fgetc( file );
		switch ( c ) {
			case '<': open++; break;
			case '>': open--; break;
			case EOF: return PARSE_ERROR;
		}
	}

	return OTHER_TAG;
}


static enum STATE read_tag ( FILE* file, struct xml_element* elem ) {

	int c = fgetc( file );
	if ( c != '<' ) return PARSE_ERROR;

	c = fgetc( file );
	if ( c == EOF ) return PARSE_ERROR;

	if( c == '?' || c == '!' )
		return read_special_tag( file, elem );

	bool close_tag = false;
	if ( c == '/' )
		close_tag = true;
	else
		ungetc( c, file );

	skip_space( file );

	struct string str = init_str;
	c = fgetc( file );

	if ( c == '>' )
		return PARSE_ERROR;

	while ( c != '>' && c != '/' && !isspace(c) ) {

		if ( c == EOF || str_push_back( c, &str ) != OK ) {
			free( str.str );
			return ( c == EOF ) ? PARSE_ERROR : MEMORY_ERROR;
		}
		c = fgetc( file );
	}

	if ( close_tag ) {

		if ( c == '/' || strcmp( str.str, elem->father->name ) != 0 ) {
			free( str.str );
			return PARSE_ERROR;
		}

		if ( c != '>') {
			skip_space( file );
			if ( fgetc( file ) != '>' ) {
				free( str.str );
				return PARSE_ERROR;
			}
		}
		free( str.str );
		return CLOSE_TAG;
	}

	str_remove_trail_space( &str );
	elem->name = str.str;
	ungetc( c, file );

	return read_attr( file, elem );
}


static enum STATE read_value ( FILE* file, struct xml_element* elem ) {

	struct string str = init_str;
	int c;

	while ( ( c = fgetc( file ) ) != '<' ) {

		if ( c == EOF || str_push_back( c, &str ) != OK ) {
			free( str.str );
			return ( c == EOF ) ? PARSE_ERROR : MEMORY_ERROR;
		}
	}

	str_remove_trail_space( &str );
	elem->value = str.str;
	ungetc( c, file );

	return OK;
}


static enum STATE build_trie_node ( struct trie_node* node, void* list_ptr,
                                    int len, int level ) {

	char*** list = list_ptr;

	// if leaf
	if ( level && strcmp( **list, **( list + len - 1 ) ) == 0 ) {

		node->letter = -node->letter;
		node->len = len;
		node->list = malloc( len * sizeof(void*) );
		if ( !node->list )
			return MEMORY_ERROR;

		memcpy( node->list, list, len * sizeof(void*) );
		return OK;
	}

	// malloc sons
	node->len = 1;
	for ( char*** i = list + 1; i < list + len; i++ )
		if ( (**i)[level] != (**(i-1))[level] )
			node->len++;

	node->list = calloc( node->len, sizeof( struct trie_node ) );
	if ( !node->list ) return MEMORY_ERROR;

	// build trie
	int letter = (*list[0])[level];
	int start = 0;
	int pos = -1;
	for ( int end = 1; end <= len; end++ ) {

		if ( end == len || (*list[end])[level] != letter ) {

			((struct trie_node*)node->list)[++pos].letter = letter;

			enum STATE state = build_trie_node(
			                             (struct trie_node*)node->list + pos,
			                             list + start, end - start, level + 1 );
			if ( state != OK ) return state;

			if ( end != len ) letter = (*list[end])[level];

			start = end;
		}
	}
	return OK;
}


static int cmp_str_p ( const void *a, const void *b ) {

	return strcmp( **(char**const*)a, **(char**const*)b );
}


static enum STATE build_trie ( void* source_list, void** trie ) {

	enum STATE state;

	if ( !source_list ) return OK;

	struct ptr_list list = init_ptr_list;

	for ( struct xml_element* i = source_list; i; i = i->next ) {
		if ( ptr_list_push_back( i, &list ) != OK ) {
			free( list.list );
			return MEMORY_ERROR;
		}
	}

	qsort( list.list, list.len, sizeof(void*), cmp_str_p );

	*trie = calloc( 1, sizeof( struct trie_node ) );
	if ( !*trie ) {
		free( list.list );
		return MEMORY_ERROR;
	}

	((struct trie_node*)(*trie))->letter = TRIE_HEAD_LETTER;

	state = build_trie_node( *trie, list.list, list.len, 0 );

	free( list.list );
	if ( state != OK ) free( *trie );

	return state;
}


static void reverse_son_list ( struct xml_element* elem ) {

	if ( !elem || !elem->son ) return;

	for ( struct xml_element* i = elem->son; i->next; i = i->prev ) {
		elem->son = i->next;
		i->next = i->prev;
		i->prev = elem->son;
	}
	elem->son->next = elem->son->prev;
	elem->son->prev = NULL;
}


static enum STATE xml_post_processing ( struct xml_element* elem ) {

	if ( !elem ) return OK;

	enum STATE state;

	reverse_son_list( elem );

	 state = build_trie( elem->son, (void*)&elem->sons_trie );
	if ( state != OK ) return state;

	state = build_trie( elem->attr, (void*)&elem->attr_trie );
	if ( state != OK ) return state;

	state = xml_post_processing ( elem->next );
	if ( state != OK ) return state;

	state = xml_post_processing ( elem->son );

	return state;
}


static enum STATE read_xml ( FILE* file, struct xml_element* elem, bool root ) {

	enum STATE state;

	while ( true ) {

		skip_space( file );

		char c = fgetc( file );
		if ( c == EOF ) return root ? OK : PARSE_ERROR;
		ungetc( c, file );

		if ( c == '<' ) {

			struct xml_element* son = calloc( 1, sizeof( struct xml_element ) );
			if ( !son ) return MEMORY_ERROR;

			son->father = elem;
			son->next = elem->son;
			son->status = IS_ELEMENT_STATUS;

			state = read_tag( file, son );
			switch ( state ) {

				case OPEN_TAG:

					if ( son->next ) son->next->prev = son;
					elem->son = son;

					if ( ( state = read_xml ( file, son, false ) ) != OK )
						return state;
					break;

				case CLOSE_TAG:

					free( son );
					return OK;

				case ISOLATED_TAG:

					if ( son->next ) son->next->prev = son;
					elem->son = son;
					break;

				case OTHER_TAG:

					free( son );
					break;

				default:
					if ( son->next ) son->next->prev = son;
					elem->son = son;
					return state;
			}
		} else {
			state = read_value( file, elem );
			if ( state != OK ) return state;
		}
	}
}


struct xml_element* load_xml ( const char* name ) {

	FILE* file = fopen( name, "r" );
	if ( !file ) return NULL;

	struct xml_element* root = calloc( 1, sizeof( struct xml_element ) );

	if ( !root ) {
		fclose( file );
		return root;
	}

	root->status = IS_META_ROOT_STATUS;

	enum STATE state;

	state = read_xml( file, root, true );

	if ( state == OK )
		state = xml_post_processing( root );

	if ( state != OK ) {
		free_xml( root );
		root = NULL;
	}

	fclose( file );
	return root;
}


static void free_xml_trie ( struct trie_node* node, bool root ) {

	if ( !node ) return;

	if( root || node->letter > 0 )
		for ( int i = 0; i < node->len; i++ )
			free_xml_trie( (struct trie_node*)node->list + i, false );

	free( node->list );
}


static void free_xml_attr ( struct xml_attribute* attr ) {

	if ( !attr ) return;

	free_xml_attr( attr->next );

	free( attr->name );
	free( attr->value );
	free( attr );
}


void free_xml ( struct xml_element* elem ) {

	if ( !elem ) return;

	free_xml( elem->next );
	free_xml( elem->son );
	free_xml_attr( elem->attr );

	free( elem->name );
	free( elem->value );

	free_xml_trie( elem->sons_trie, true );
	free_xml_trie( elem->attr_trie, true );
	free( elem->sons_trie );
	free( elem->attr_trie );

	free( elem );
}


static bool xml_element_check ( struct xml_element* elem, const char* name,
                                int name_len ) {

	if ( !elem || !elem->name ) return false;

	if ( name_len == 1 && name[0] == '*' ) return true;

	if ( strncmp( elem->name, name, name_len ) != 0 ) return false;

	return !elem->name[ name_len ];
}


static int cmp_trie ( const void *a, const void *b ) {

	return abs( *(const int*)a ) - abs( *(const int*)b );
}


static struct trie_node* xml_trie_check ( int level, struct trie_node* node,
										  const char* name, int name_len ) {

	if ( !node ) return NULL;

	if ( node->letter == 0 ) return node;

	if ( level && node->letter < 0 ) {

		if ( strncmp( **(char***)node->list + level, name, name_len ) != 0 )
			return NULL;

		if ( (**(char***)node->list)[level + name_len] != 0 ) return NULL;

		return node;
	}

	int key = *name;
	return xml_trie_check( level + 1,
	                       bsearch( &key, node->list, node->len,
	                                sizeof( struct trie_node ), cmp_trie ),
	                       name + 1, name_len - 1 );
}


static bool _xml_get_ancestor ( struct ptr_list* plist, struct xml_element* elem,
                                const char* name, int name_len ) {

	if ( elem->status & IS_META_ROOT_STATUS ) return true;
	if ( elem->status & IS_TOUCHED_STATUS   ) return true;

	elem->status |= IS_TOUCHED_STATUS;

	bool ret = _xml_get_ancestor( plist, elem->father, name, name_len );

	if ( xml_element_check( elem, name, name_len ) )
		if ( ptr_list_push_back( elem, plist ) != OK )
			return false;

	return ret;
}


static void _xml_clear_ancestor ( struct xml_element* elem ) {

	if ( elem->status & IS_META_ROOT_STATUS ) return;
	if ( !( elem->status & IS_TOUCHED_STATUS ) ) return;

	elem->status ^= IS_TOUCHED_STATUS;

	_xml_clear_ancestor( elem->father );
}


static void xml_get_ancestor ( struct ptr_list* plist, struct xml_element** list,
                               const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !_xml_get_ancestor( plist, (*l)->father, name, name_len ) ) {

			free( plist->list );
			*plist = init_ptr_list;

			for ( l = list; *l; l++ )
				_xml_clear_ancestor( (*l)->father );
			return;
		}
	}

	for ( struct xml_element** l = list; *l; l++ )
		_xml_clear_ancestor( (*l)->father );
}


static void xml_get_ancestor_or_self ( struct ptr_list* plist,
                                       struct xml_element** list,
                                       const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !_xml_get_ancestor( plist, *l, name, name_len ) ) {

			free( plist->list );
			*plist = init_ptr_list;

			for ( l = list; *l; l++ )
				_xml_clear_ancestor( *l );
			return;
		}
	}

	for ( struct xml_element** l = list; *l; l++ )
		_xml_clear_ancestor( *l );
}


static void xml_get_attribute ( struct ptr_list* plist, struct xml_element** list,
                                const char* name, int name_len ) {

	if ( name_len == 1 && name[0] == '*' ) {

		for ( struct xml_element** l = list; *l; l++ ) {

			struct xml_attribute* attr = (*l)->attr;

			for ( ; attr; attr = attr->next ) {

				if ( ptr_list_push_back( attr, plist ) != OK ) {

					free( plist->list );
					*plist = init_ptr_list;
					return;
				}
			}
		}
	} else {
		for ( struct xml_element** l = list; *l; l++ ) {

			struct trie_node* node = xml_trie_check( 0, (*l)->attr_trie, name,
			                                         name_len );
			if ( node ) {
				struct xml_attribute** attrs = (void*)node->list;

				for ( int i = 0; i < node->len; i++ ) {

					if ( ptr_list_push_back( attrs[i], plist ) != OK ) {

						free( plist->list );
						*plist = init_ptr_list;
						return;
					}
				}
			}
		}
	}
}


static void xml_get_child ( struct ptr_list* plist, struct xml_element** list,
                            const char* name, int name_len ) {

	if ( name_len == 1 && name[0] == '*' ) {

		for ( struct xml_element** l = list; *l; l++ ) {

			struct xml_element* elem = (*l)->son;

			for ( ; elem; elem = elem->next ) {

				if ( ptr_list_push_back( elem, plist ) != OK ) {

					free( plist->list );
					*plist = init_ptr_list;
					return;
				}
			}
		}
	} else {
		for ( struct xml_element** l = list; *l; l++ ) {

			struct trie_node* node = xml_trie_check( 0, (*l)->sons_trie, name,
			                                         name_len );
			if ( node ) {
				struct xml_element** elems = (void*)node->list;

				for ( int i = 0; i < node->len; i++ ) {

					if ( ptr_list_push_back( elems[i], plist ) != OK ) {

						free( plist->list );
						*plist = init_ptr_list;
						return;
					}
				}
			}
		}
	}
}


static bool _xml_get_descendant ( struct ptr_list* plist, struct xml_element* elem,
                                  const char* name, int name_len ) {

	if ( !elem || elem->status & IS_TOUCHED_STATUS ) return true;

	elem->status |= IS_TOUCHED_STATUS;

	if ( xml_element_check( elem, name, name_len ) )
		if ( ptr_list_push_back( elem, plist ) != OK )
			return false;

	for ( struct xml_element* son = elem->son; son; son = son->next )
		if ( !_xml_get_descendant( plist, son, name, name_len ) )
			return false;

	return true;
}


static void _xml_clear_descendant ( struct xml_element* elem ) {

	if ( !elem ) return;
	if ( !( elem->status & IS_TOUCHED_STATUS ) ) return;

	elem->status ^= IS_TOUCHED_STATUS;

	for ( struct xml_element* son = elem->son; son; son = son->next )
		_xml_clear_descendant( son );
}


static void xml_get_descendant ( struct ptr_list* plist, struct xml_element** list,
                                 const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !( (*l)->status & IS_TOUCHED_STATUS ) ) {

			struct xml_element* elem = (*l)->son;

			for ( ; elem; elem = elem->next ) {

				if ( !_xml_get_descendant( plist, elem, name, name_len ) ) {

					free( plist->list );
					*plist = init_ptr_list;

					for ( l = list; *l; l++ ) {
						(*l)->status |= IS_TOUCHED_STATUS;
						_xml_clear_descendant( *l );
					}
					return;
				}
			}
		}
	}

	for ( struct xml_element** l = list; *l; l++ ) {
		(*l)->status |= IS_TOUCHED_STATUS;
		_xml_clear_descendant( *l );
	}
}


static void xml_get_descendant_or_self ( struct ptr_list* plist,
                                         struct xml_element** list,
                                         const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !( (*l)->status & IS_TOUCHED_STATUS ) ) {

			if ( !_xml_get_descendant( plist, *l, name, name_len ) ) {

				free( plist->list );
				*plist = init_ptr_list;

				for ( l = list; *l; l++ )
					_xml_clear_descendant( *l );
				return;
			}
		}
	}

	for ( struct xml_element** l = list; *l; l++ )
		_xml_clear_descendant( *l );
}


static void xml_get_following ( struct ptr_list* plist, struct xml_element** list,
                                const char* name, int name_len ) {

	struct ptr_list siblist = init_ptr_list;

	for ( struct xml_element** l = list; *l; l++ ) {

		for ( struct xml_element* elem = (*l)->next; elem; elem = elem->next ) {

			if ( ptr_list_push_back( elem, &siblist ) != OK ) {

				free( siblist.list );
				free( plist->list );
				*plist = init_ptr_list;
				return;
			}
		}
	}
	xml_get_descendant_or_self( plist, (void*)siblist.list, name, name_len );

	free( siblist.list );
}


static void xml_get_following_sibling ( struct ptr_list* plist,
                                        struct xml_element** list,
                                        const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !( (*l)->status & IS_ELEMENT_STATUS ) ) continue;

		for ( struct xml_element* elem = (*l)->next; elem; elem = elem->next ) {

			if ( elem->status & IS_TOUCHED_STATUS ) break;

			if ( xml_element_check( elem, name, name_len ) ) {

				elem->status |= IS_TOUCHED_STATUS;

				if ( ptr_list_push_back( elem, plist ) != OK ) {

					free( plist->list );
					*plist = init_ptr_list;

					for ( l = list; *l; l++ )
						for ( elem = (*l)->next; elem; elem = elem->next )
							if ( !( elem->status & IS_TOUCHED_STATUS ) )
								break;
							else
								elem->status ^= IS_TOUCHED_STATUS;
					return;
				}
			}
		}
	}

	for ( struct xml_element** l = list; *l; l++ )
		for ( struct xml_element* elem = (*l)->next; elem; elem = elem->next )
			if ( !( elem->status & IS_TOUCHED_STATUS ) )
				break;
			else
				elem->status ^= IS_TOUCHED_STATUS;
}


static void xml_get_namespace ( struct ptr_list* plist, struct xml_element** list,
                                const char* name, int name_len ) {

	free( plist->list );
	*plist = init_ptr_list;
	(void)list;
	(void)name;
	(void)name_len;
}


static void xml_get_parent ( struct ptr_list* plist, struct xml_element** list,
                             const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !( (*l)->father->status & IS_TOUCHED_STATUS ) ) {

			(*l)->father->status |= IS_TOUCHED_STATUS;

			if ( xml_element_check( (*l)->father, name, name_len ) ) {

				if ( ptr_list_push_back( (*l)->father, plist ) != OK ) {

					free( plist->list );
					*plist = init_ptr_list;

					for ( l = list; *l; l++ )
						(*l)->father->status ^= IS_TOUCHED_STATUS;
					return;
				}
			}
		}
	}
	for ( struct xml_element** l = list; *l; l++ )
		(*l)->father->status ^= IS_TOUCHED_STATUS;
}


static void xml_get_preceding ( struct ptr_list* plist, struct xml_element** list,
                                const char* name, int name_len ) {

	struct ptr_list siblist = init_ptr_list;

	for ( struct xml_element** l = list; *l; l++ ) {

		struct xml_element* elem;

		for ( elem = *l; elem->prev; elem = elem->prev ) ;
		for ( ; elem != *l; elem = elem->next ) {

			if ( ptr_list_push_back( elem, &siblist ) != OK ) {

				free( siblist.list );
				free( plist->list );
				*plist = init_ptr_list;
				return;
			}
		}
	}
	xml_get_descendant_or_self( plist, (void*)siblist.list, name, name_len );

	free( siblist.list );
}


static void xml_get_preceding_sibling ( struct ptr_list* plist,
                                        struct xml_element** list,
                                        const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( !( (*l)->status & IS_ELEMENT_STATUS ) ) continue;

		struct xml_element* elem;

		for ( elem = *l; elem->prev; elem = elem->prev )

			if ( elem->prev->status & IS_TOUCHED_STATUS ) break;

		for ( ; elem != *l; elem = elem->next ) {

			if ( xml_element_check( elem, name, name_len ) ) {

				elem->status |= IS_TOUCHED_STATUS;

				if ( ptr_list_push_back( elem, plist ) != OK ) {

					free( plist->list );
					*plist = init_ptr_list;

					for ( l = list; *l; l++ )
						for ( elem = (*l)->prev; elem; elem = elem->prev )
							if ( !( elem->status & IS_TOUCHED_STATUS ) )
								break;
							else
								elem->status ^= IS_TOUCHED_STATUS;
					return;
				}
			}
		}
	}

	for ( struct xml_element** l = list; *l; l++ )
		for ( struct xml_element* elem = (*l)->prev; elem; elem = elem->prev )
			if ( !( elem->status & IS_TOUCHED_STATUS ) )
				break;
			else
				elem->status ^= IS_TOUCHED_STATUS;
}


static void xml_get_self ( struct ptr_list* plist, struct xml_element** list,
                           const char* name, int name_len ) {

	for ( struct xml_element** l = list; *l; l++ ) {

		if ( xml_element_check( *l, name, name_len ) ) {

			if ( ptr_list_push_back( *l, plist ) != OK ) {

				free( plist->list );
				*plist = init_ptr_list;
				return;
			}
		}
	}
}


#define NUM_AXES       13

#define ATTRIBUTE_AXE   2
#define CHILD_AXE       3
#define PARENT_AXE      9
#define SELF_AXE       12
#define DESCENDANT_OR_SELF_AXE  5

static const char* axes[ NUM_AXES ] = {

	"ancestor",   "ancestor-or-self",   "attribute", "child",
	"descendant", "descendant-or-self", "following", "following-sibling",
	"namespace",  "parent",             "preceding", "preceding-sibling",
	"self"
};

typedef void (*axe_handler) ( struct ptr_list*, struct xml_element**,
                              const char*, int );

static const axe_handler axe_handlers[ NUM_AXES ] = {

	xml_get_ancestor, xml_get_ancestor_or_self, xml_get_attribute,
	xml_get_child,    xml_get_descendant,       xml_get_descendant_or_self,
	xml_get_following,xml_get_following_sibling,xml_get_namespace,
	xml_get_parent,   xml_get_preceding,        xml_get_preceding_sibling,
	xml_get_self
};


void** xml_get ( struct xml_element* element, const char* query ) {

	struct ptr_list list = init_ptr_list;

	if ( query[0] != '/' ) {

		struct ptr_list aux = init_ptr_list;

		if ( ptr_list_push_back( element, &aux ) != OK ) {
			free( list.list );
			free( aux.list );
			return NULL;
		}
		xml_get_descendant( &list, (void*)aux.list, "*", 1 );

		free( aux.list );
	} else
	if ( query++[1] != '/' ) {

		if ( ptr_list_push_back( element, &list ) != OK ) {
			free( list.list );
			return NULL;
		}
	} else {
		if ( ptr_list_push_back( element->son, &list ) != OK ) {
			free( list.list );
			return NULL;
		}
	}

	while ( *query ) {

		struct ptr_list aux = init_ptr_list;

		int axe = CHILD_AXE;

		switch ( *query ) {

			case '/':
				query++;
				axe = DESCENDANT_OR_SELF_AXE;
				break;

			case '@':
				query++;
				axe = ATTRIBUTE_AXE;
				break;

			case '.':
				if ( *++query == '.' ) {
					query++;
					axe = PARENT_AXE;
				} else
					axe = SELF_AXE;
				break;
		}

		const char* start = query;

		while( true ) {

			if ( *query == ':' ) {

					int m = -1, M = NUM_AXES, med, ret;

					while ( M - m > 1 ) {

						med = ( m + M )/2;
						ret = strncmp( axes[ med ], start, query - start );

						if ( ret == 0 && axes[ med ][ query - start ] == 0 ) {
							axe = med;
							break;
						}
						if ( ret < 0 ) m = med;
						else M = med;
					}

					start = query += 2;
					continue;
			}

			if ( *query == 0 ) {

				axe_handlers[ axe ]( &aux, (void*)list.list, start,
									 query - start );
				break;
			}

			if ( *query == '/' ) {

				axe_handlers[ axe ]( &aux, (void*)list.list, start,
									 query - start );
				query++;
				break;
			}

			if ( *query == '[' ) {

				axe_handlers[ axe ]( &aux, (void*)list.list, start,
									 query - start );
				int i = 1;
				for( ++query; i; ++query )
					i += ( *query == '[' ) - ( *query == ']' );
				++query;
				break;
			}

			++query;
		}

		free( list.list );
		list = aux;
	}

	if ( ptr_list_push_back( NULL, &list ) != OK ) {
		free( list.list );
		return NULL;
	}

	return list.list;
}


void free_xml_list ( void** list ) {

	free( list );
}

