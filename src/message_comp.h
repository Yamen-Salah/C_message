#ifndef MESSAGE_COMP_H
#define MESSAGE_COMP_H

#include <Arduino.h>

struct CharSet{
  const char* chars;
  int size;
};
extern const CharSet charSets[];
extern int currentSet;

struct Node {
  char data;
  Node *prev;
  Node *next;
};

extern Node *p_head;
extern Node *p_tail;

//Linked List Functions
Node* create_node (char val);
int insert_data_at_head (char val);
int insert_data_at_tail (char val);
int insert_data_at_middle (Node* target, char val);
int find_and_delete_data (Node* target);
void free_node_list(Node *head);
void delete_all_data ();
int get_message_length();
Node* get_node_at(int index);

#endif
