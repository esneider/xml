/**
 * @file test.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "xml.h"


static void print_xml_attr ( int level, struct xml_attribute* attr ) {

	if ( !attr ) return;

	for( int i = 0; i < level*3; i++ ) putchar(' ');
	printf( "(%s, %s)\n", attr->name, attr->value );

	if ( level )
		print_xml_attr( level, attr->next );
}


static void print_xml_node ( int level, struct xml_element* elem ) {

	if ( !elem ) return;

	if ( elem->status & 2 ) {
		print_xml_attr( level, (void*)elem );
		return;
	}

	for( int i = 0; i < level*3; i++ ) putchar(' ');
	printf( "%s: %s\n", elem->name, elem->value );

	print_xml_attr( level + 1, elem->attr );
	print_xml_node( level + 1, elem->son );
	if ( level )
		print_xml_node( level, elem->next );
}


static void print_xml_trie ( int level, struct trie_node* node ) {

	if ( !node ) return;

	if ( node->letter > 0 || !level ) {
		for( int i = 0; i < level*3; i++ ) putchar(' ');
		if ( level ) putchar( node->letter );

		putchar('[');
		for ( int i = 0; i < node->len; i++)
			putchar( abs( ((struct trie_node*)node->list)[i].letter ) );
		printf( "]\n" );

		for ( int i = 0; i < node->len; i++)
			print_xml_trie( level + 1, (struct trie_node*)node->list + i );
	} else {
		for ( int i = 0; i < node->len; i++) {
			for( int j = 0; j < level*3; j++ ) putchar(' ');
			printf( "%s\n", ((struct xml_element**)node->list)[i]->name );
		}
	}
}


int main ( void ) {

	struct xml_element* xml_root = load_xml( "test/test.xml" );
	void** query = xml_get( xml_root,
							"/language//list/@name/..*/ancestor::highlighting/"
							"following-sibling::*/.general//comment/@*/..*" );

	for ( void** i = query; i && *i; ++i ) {
		print_xml_node( 0, *i );
		printf("\n");
	}

	free_xml( xml_root );
	free_xml_list( query );

	return 0;
}

