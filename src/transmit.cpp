#include "transmit.h"
#include "message_comp.h"
#include "ui.h"

//The Radios have 6 adresses used to define wherer they store data kind of "channels"
const byte address[][6] = {"Ch001", "Ch002", "Ch003", "Ch004", "Ch005", "Ch006"};

//Special constants that define the power levels for the RF lib
const int pwrLevels[] = {RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX};

PacketNode *tx_queue_front = NULL;
PacketNode *tx_queue_back = NULL;

//Linked List (for message array ptrs)
MsgNode *msg_head = NULL;
MsgNode *msg_tail = NULL;

StackNode *rx_stack = NULL;

//This function will have to change alot for the morse encoding
int construct_packets(Node *head_ptr){ //creates packets consisting of 30 bytes of char data
  queue_clear(&tx_queue_front, &tx_queue_back);
  Node* temp = head_ptr;
  int index = 0;

  while(temp != NULL){
    packet chunk;
    memset(chunk.data, 0, sizeof(chunk.data));
    chunk.packet_index = index;

    for(int i = 0; i < PACKET_SIZE && temp != NULL; i++){
      chunk.data[i] = temp->data;
      temp = temp->next;
    }
    chunk.finalPacket = (temp == NULL) ? EOM : CONTINUE;
    enqueue(&tx_queue_front, &tx_queue_back, chunk);
    index++;
  }
  return index;
}

//TX
void transmit_packet(){ //transmits packets of chars
  packet chunk = dequeue(&tx_queue_front, &tx_queue_back);
  radio.write(&chunk, sizeof(chunk));
}

void transmit_message(LiquidCrystal_I2C &lcd) {
  // aden put your code here something like this -> Node p_head_translated = translate_message();
  int totalPackets = construct_packets(p_head);

  for(int i = 0; i < totalPackets; i++) {
    transmit_packet();
    progress_bar(i, totalPackets, lcd);
  }
  delay(500);
}

//RX
int receive_packet(){ 
  packet chunk;
  radio.read(&chunk, sizeof(chunk));
  push(&rx_stack, chunk);
  return chunk.packet_index;
}

// Rebuilds the message back to front from the stack
//uses a stack because its much easier to see the last packet
char* rebuild_message(){
  if(stack_empty(rx_stack))
    return NULL;
  
  packet chunk = pop(&rx_stack);
  int packetNum = chunk.packet_index;
  int FinalChunkSize = strnlen(chunk.data, PACKET_SIZE);

  int totalChars = (packetNum * PACKET_SIZE) + FinalChunkSize + 1;
  char *message = (char*) malloc(totalChars);
  if(message == NULL) {
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
      free(message);
      return NULL;
    }
    chunk = pop(&rx_stack);
    ArrPos = chunk.packet_index * PACKET_SIZE;
    for(int j = 0; j < PACKET_SIZE && chunk.data[j] != '\0'; j++){
      message[ArrPos + j] = chunk.data[j];
    }
  }
  return message;
}

//Radio Init bruv
void init_radio_tx(int byte_address, int pwr_lvl){
  radio.openWritingPipe(address[byte_address - 1]);
  radio.setPALevel(pwrLevels[pwr_lvl]);
  radio.stopListening();
}

void init_radio_rx(int byte_address, int pwr_lvl){
  radio.openReadingPipe(1, address[byte_address - 1]);
  radio.setPALevel(pwrLevels[pwr_lvl]);
  radio.startListening();
  radio.flush_rx();
}

//Stack implementation functions
void push(StackNode **top, packet p){
  StackNode *p_new = (StackNode*) malloc(sizeof(StackNode));
  if(p_new == NULL) {
    memory_full(lcd);
    return;
  }
  p_new->data = p;
  p_new->next = *top;
  *top = p_new;
}

packet pop(StackNode **top){
  packet p = {};
  if(*top == NULL) return p;
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
  while(*top) pop(top);
}

//Queue implementation functions
PacketNode* create_packet(packet p) {
  PacketNode *p_new = (PacketNode*) malloc(sizeof(PacketNode));
  if(p_new == NULL) return NULL;
  p_new->data = p;
  p_new->next = NULL;
  return p_new;
}

int enqueue(PacketNode **front, PacketNode **back, packet p){
  PacketNode *p_new = create_packet(p);
  if(p_new == NULL) return -1;
  if(*back != NULL){
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
  packet p = {};
  if(*front == NULL) return p;

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
  while(*front) dequeue(front, back);
}

//Linked list functions
MsgNode* create_msg_node(char *val){
  MsgNode *p_new = (MsgNode*) malloc(sizeof(MsgNode));
  if(p_new == NULL) return NULL;
  p_new->data = val;
  p_new->prev = NULL;
  p_new->next = NULL;
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