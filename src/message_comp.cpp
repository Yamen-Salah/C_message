#include "message_comp.h"
#include <LiquidCrystal_I2C.h>

const char letters[] = {' ','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};
const char numbers[] = {'0','1','2','3','4','5','6','7','8','9'};
const char specials[] = {'.','?','!',';','"','#','@','$','+','-','*','/','(',')'};

const CharSet charSets[] = {
  {letters, sizeof(letters)/sizeof(letters[0])},
  {numbers, sizeof(numbers)/sizeof(numbers[0])},
  {specials, sizeof(specials)/sizeof(specials[0])}
};

Node *p_head = NULL;
Node *p_tail = NULL;

//LINKED LIST FUNCTIONS AFTER HERE

// allocates a new node with the given character
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

// inserts a character at the front of the list
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

// inserts a character at the back of the list
int insert_data_at_tail(char val) {
  Node *p_new = create_node(val);
  if (p_new == NULL)
    return -1;
  if (p_tail != NULL) {
    p_tail->next = p_new;
    p_new->prev = p_tail;
    p_tail = p_new;
  } else {
    p_head = p_new;
    p_tail = p_new;
  }
  return 0;
}

// inserts a character after the target node
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

// removes and frees the target node
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

// frees all nodes starting from head
void free_node_list(Node *head) {
  while (head != NULL) { Node *next = head->next; free(head); head = next; }
}

// frees and clears the entire compose buffer
void delete_all_data() {
  free_node_list(p_head);
  p_head = NULL;
  p_tail = NULL;
}

// returns the number of characters in the compose buffer
int get_message_length() {
  int messageLen = 0;
  Node *p = p_head;
  while (p != NULL) { messageLen++; p = p->next; }
  return messageLen;
}

// returns the node at the given index
Node* get_node_at(int index) {
  Node *p = p_head;
  for (int i = 0; i < index && p != NULL; i++) {
    p = p->next;
  }
  return p;
}
