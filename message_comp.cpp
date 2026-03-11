#include "message_comp.h"
#include <LiquidCrystal_I2C.h>

//the progmem flag puts the strings in flash instead of ram - turns out it not needed but it saves memory anyway so why not keep it
const char letters[] PROGMEM = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
const char numbers[] PROGMEM = {'0','1','2','3','4','5','6','7','8','9'};
const char specials[] PROGMEM = {' ','.','?','!',';','"','#','@','$','+','-','*','/','(',')'};

const CharSet charSets[] = {
  {letters, 26},
  {numbers, 10},
  {specials, 15}
};

Node *p_head = NULL;
Node *p_tail = NULL;

int currentSet = 0;

//LINKED LIST FUNCTIONS AFTER HERE
// Function to create a new node
Node* create_node (char val) {
  // Declaration of local node pointer, with mem allocation
  Node *p_new = (Node *) malloc (sizeof (Node));
  
  if (p_new != NULL) {
    p_new->data = val;
    p_new->prev = NULL; 
    p_new->next = NULL;
  }
  return p_new;
}

// Function to insert node at front of the list
int insert_data_at_head (char val){
  Node *p_new = create_node (val);
  // Make sure node creation worked
  if (p_new == NULL) {
    return -1;
  }
  if (p_head != NULL){
    p_new->next = p_head;
    p_head->prev = p_new;
    p_head = p_new;
  }
  else{
    p_head = p_new;
    p_tail = p_new;
  }
  return 0;   
}
 
// Insert new item of data at back of linked list
int insert_data_at_tail (char val) {
  Node *p_new = create_node (val);
  // Make sure node creation worked
  if (p_new == NULL) {
    return -1;
  }
  if (p_tail != NULL){
    p_tail->next = p_new;
    p_new->prev = p_tail;
    p_tail = p_new;
  }
  else {
    p_head = p_new;
    p_tail = p_new;
  }
  return 0;
}

//Insert data based on a given pointer rather than a value again because we want to perform this operation relative to a cursor.
int insert_data_at_middle(Node *target, char val) {
  if (target == NULL) {
    return insert_data_at_head(val);
  }
  Node *p_new = create_node(val);
  if (p_new == NULL) return -1;

  p_new->prev = target;
  p_new->next = target->next;

  if (target->next != NULL) {
    target->next->prev = p_new;
  } else {
    p_tail = p_new;  //target was tail
  }
  target->next = p_new;
  return 0;
}

//Find a given node by pointer rather than value like the lab version of this function.
//This is because we want the cursor to be able to delete values that are right before or after its pos not to search for a given value.
int find_and_delete_data(Node *target) {
  if (target == NULL) return -1;

  if (target == p_head && target == p_tail) {
    //Only one node
    p_head = NULL;
    p_tail = NULL;
  } else if (target == p_head) {
    p_head = target->next;
    p_head->prev = NULL;
  } else if (target == p_tail) {
    p_tail = target->prev;
    p_tail->next = NULL;
  } else {
    target->prev->next = target->next;
    target->next->prev = target->prev;
  }
  free(target);
  return 0;
}

int delete_all_data() {
  Node *p_temp;
  // Loop through all nodes
  while (p_head != NULL) {
    p_temp = p_head;
    p_head = p_head->next;
    free(p_temp);
  }
  p_tail = NULL;
  return 0;
}

//returns the length of the message by following the chain of nodes.
int get_message_length() {
  int messageLen = 0;
  Node *p = p_head;
  while (p != NULL) { messageLen++; p = p->next; }
  return messageLen;
}

//Get node at returns a pointer to the node that is at the index the pot is selecting. This way, you can easily delete stuff or add to a given 
//index within the LL
Node* get_node_at(int index) {
  Node *p = p_head;
  for (int i = 0; i < index && p != NULL; i++) {
    p = p->next;
  }
  return p;
}