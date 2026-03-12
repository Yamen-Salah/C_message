#ifndef MESSAGE_COMP_H
#define MESSAGE_COMP_H

#include <LiquidCrystal_I2C.h>
#include <Arduino.h>

struct CharSet{
  const char* chars;
  int size;
};
extern const CharSet charSets[];
extern int currentSet;

struct Node {
  char data;
  struct Node *prev;
  struct Node *next;
};
typedef struct Node Node;

extern Node *p_head;
extern Node *p_tail;

//Linked List Functions
Node* create_node (char val);
int insert_data_at_head (char val);
int insert_data_at_middle (Node* target, char val);
int insert_data_at_tail (char val);
int find_and_delete_data (Node* target);
void delete_all_data ();


Node* get_node_at(int index);
int get_message_length();

#endif