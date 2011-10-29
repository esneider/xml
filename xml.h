/**
 * @file xml.h
 *
 */

#ifndef _XML_H_
#define _XML_H_


/*
 * if letter <= 0 then list is an array of matching elements;
 * else, it is a compressed trie array
 */
struct trie_node {

	int letter;
	void* list;
	int len;
};


struct xml_element {

	char* name;
	char* value;

	char status;

	struct xml_element* father;
	struct xml_element* next;
	struct xml_element* prev;
	struct xml_element* son; // first son

	struct xml_attribute* attr; // first attribute

	struct trie_node* sons_trie;
	struct trie_node* attr_trie;
};


struct xml_attribute {

	char* name;
	char* value;

	char status;

	struct xml_element* father;
	struct xml_attribute* next;
	struct xml_attribute* prev;
};


struct xml_element* load_xml( const char* name );
void free_xml( struct xml_element* elem );

void** xml_get( struct xml_element* element, const char* query );
void free_xml_list( void** list );


#endif /* _XML_H_ */

