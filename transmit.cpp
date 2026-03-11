#include "transmit.h"
#include "message_comp.h"
#include "ui.h"

//The Radios have 6 adresses used to define wherer they store data kind of "channels"
const byte address[][6] PROGMEM = {"Ch001", "Ch002", "Ch003", "Ch004", "Ch005", "Ch006"};

//Special constants that define the power levels for the RF lib
const int pwrLevels[] = {RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX};

PacketNode *tx_queue_front = NULL;
PacketNode *tx_queue_back = NULL;

//Linked List (for message array ptrs)
MsgNode *msg_head = NULL;
MsgNode *msg_tail = NULL;

StackNode *rx_stack = NULL;

int construct_packets(Node *head_ptr){
  queue_clear(&tx_queue_front, &tx_queue_back);
  Node* temp = head_ptr;
  int index = 0;

  //First loop repeats until there are no more nodes in the char linked list.
  while(temp != NULL){
    packet chunk;
    memset(chunk.data, 0, sizeof(chunk.data));
    chunk.packet_index = index;

    //Second loop repeatedly places chars from the linked list into a packet until the packet is full. then ties up the packet with either EOM or continue
    for(int i = 0; i < PACKET_SIZE && temp != NULL; i++){
      chunk.data[i] = temp->data;
      temp = temp->next;
    }
    chunk.finalPacket = (temp == NULL) ? EOM : CONTINUE;
    
    //enqeue the packet into the queue ready to be sent.
    enqueue(&tx_queue_front, &tx_queue_back, chunk);
    index++;
  }
  return index;
}

//TX
void transmit_packet(){
  packet chunk = dequeue(&tx_queue_front, &tx_queue_back);
  radio.write(&chunk, sizeof(chunk));
}

void transmit_message(LiquidCrystal_I2C &lcd) {
  int totalPackets = construct_packets(p_head);

  for(int i = 0; i < totalPackets; i++) {
    transmit_packet();
    progress_bar(i, totalPackets, lcd);
  }
}

//RX
int receive_packet(){
  packet chunk;
  radio.read(&chunk, sizeof(chunk));
  push(&rx_stack, chunk);
  return (chunk.finalPacket == EOM) ? 1 : -1;
}

char* rebuild_message(){
  if(stack_empty(rx_stack)) {
    return NULL;  // No packets to rebuild
  }
  
  packet chunk = pop(&rx_stack);
  int FinalChunkSize = 0;
  int packetNum = chunk.packet_index;

  for(int k = 0; chunk.data[k] != '\0'; k++, FinalChunkSize++);

  int totalChars = (packetNum * PACKET_SIZE) + FinalChunkSize + 1;
  char *message = (char*) malloc(totalChars);
  if(message == NULL) {
    // Memory allocation failed - clear remaining stack to prevent leak
    stack_clear(&rx_stack);
    return NULL;
  }
  message[totalChars - 1] = '\0';

  int ArrPos = chunk.packet_index * PACKET_SIZE;
  for(int j = 0; j < FinalChunkSize; j++){
    message[ArrPos + j] = chunk.data[j];
  }
  for(int i = 0; i < packetNum; i++){
    if(stack_empty(rx_stack)) {
      // Stack empty prematurely - free message and clear to prevent leak
      free(message);
      return NULL;
    }
    chunk = pop(&rx_stack);
    ArrPos = chunk.packet_index * PACKET_SIZE;
    for(int j = 0; j < PACKET_SIZE && chunk.data[j] != '\0'; j++){
      message[ArrPos + j] = chunk.data[j];
    }
  }
  
  // Clear any remaining packets in stack (should be empty, but safety check)
  stack_clear(&rx_stack);
  
  return message;
}

//Radio Init bruv
void init_radio_tx(int byte_address, int pwr_lvl){
  byte addr[6];
  memcpy_P(addr, address[byte_address - 1], 6);
  radio.openWritingPipe(addr);
  radio.setPALevel(pwr_lvl);
  radio.stopListening();
}

void init_radio_rx(int byte_address, int pwr_lvl){
  byte addr[6];
  memcpy_P(addr, address[byte_address - 1], 6);
  radio.openReadingPipe(1, addr);
  radio.setPALevel(pwr_lvl);
  radio.startListening();
  radio.flush_rx();
}

//Stack implementation functions
void push(StackNode **top, packet p){
  StackNode *p_new = (StackNode*) malloc(sizeof(StackNode));
  if(p_new == NULL) {
    memory_full(lcd);
    return;  // Prevent NULL pointer dereference
  }
  p_new->data = p;
  p_new->next = *top;
  *top = p_new;
}

packet pop(StackNode **top){
  packet p;
  if(*top == NULL){
    p.packet_index = 0;
    memset(p.data, 0, PACKET_SIZE);
    p.finalPacket = 0;
    return p;
  }
  StackNode *temp = *top;
  p = temp->data;
  *top = temp->next;
  free(temp);
  return p;
}

bool stack_empty(StackNode *top){
  return top == NULL;
}

void stack_clear(StackNode **top){
  while(!stack_empty(*top)){
    pop(top);
  }
}

//Queue implementation functions
PacketNode* create_packet (packet p) {
  // Declaration of local node pointer, with mem allocation
  PacketNode *p_new = (PacketNode*) malloc (sizeof(PacketNode));
  
  if (p_new != NULL) {
    p_new->data = p; 
    p_new->next = NULL;
  }
  return p_new;
}

int enqueue (PacketNode **front, PacketNode **back, packet p){
  PacketNode *p_new = create_packet (p);
  //Make sure node creation worked
  if (p_new == NULL) {
    return -1;
  }
  if (*back != NULL){
    (*back)->next = p_new;
    *back = p_new;
  }
  else{
    *front = p_new;
    *back = p_new;
  }
  return 0;   
}

packet dequeue(PacketNode **front, PacketNode **back) {
  packet p;
  if (*front == NULL) {
    //If queue is empty, 
    p.packet_index = 0;
    memset(p.data, 0, PACKET_SIZE);
    p.finalPacket = 0;
    return p;
  }

  PacketNode *temp = *front;
  p = temp->data;
  *front = (*front)->next;

  if (*front == NULL) {
    *back = NULL;
  }

  free(temp);
  return p;
}

bool queue_empty(PacketNode *front) {
  return front == NULL;
}

void queue_clear(PacketNode **front, PacketNode **back) {
  while (!queue_empty(*front)) {
    dequeue(front, back);
  }
}

//Linked list functions
MsgNode* create_msg_node(char *val){
  MsgNode *p_new = (MsgNode*) malloc(sizeof(MsgNode));
  if(p_new != NULL){
    p_new->data = val;
    p_new->prev = NULL;
    p_new->next = NULL;
  }
  return p_new;
}

int insert_msg_head(char *val){
  MsgNode *p_new = create_msg_node(val);
  if(p_new == NULL) return -1;
  if(msg_head != NULL){
    p_new->next = msg_head;
    msg_head->prev = p_new;
    msg_head = p_new;
  } else {
    msg_head = p_new;
    msg_tail = p_new;
  }
  return 0;
}

int insert_msg_middle(MsgNode *target, char *val){
  if(target == NULL) return insert_msg_head(val);
  MsgNode *p_new = create_msg_node(val);
  if(p_new == NULL) return -1;

  p_new->prev = target;
  p_new->next = target->next;

  if(target->next != NULL){
    target->next->prev = p_new;
  } else {
    msg_tail = p_new;
  }
  target->next = p_new;
  return 0;
}

int insert_msg_tail(char *val){
  MsgNode *p_new = create_msg_node(val);
  if(p_new == NULL) return -1;
  if(msg_tail != NULL){
    msg_tail->next = p_new;
    p_new->prev = msg_tail;
    msg_tail = p_new;
  } else {
    msg_head = p_new;
    msg_tail = p_new;
  }
  return 0;
}

int delete_msg(MsgNode *target){
  if(target == NULL) return -1;
  if(target == msg_head && target == msg_tail){
    msg_head = NULL;
    msg_tail = NULL;
  } else if(target == msg_head){
    msg_head = target->next;
    msg_head->prev = NULL;
  } else if(target == msg_tail){
    msg_tail = target->prev;
    msg_tail->next = NULL;
  } else {
    target->prev->next = target->next;
    target->next->prev = target->prev;
  }
  free(target->data);
  free(target);
  return 0;
}

int saved_msgs(){
  int count = 0;
  MsgNode *p = msg_head;
  while(p != NULL){ count++; p = p->next; }
  return count;
}