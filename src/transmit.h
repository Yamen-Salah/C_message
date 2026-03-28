#ifndef TRANSMIT_H
#define TRANSMIT_H

#include "message_comp.h"
#include <SPI.h>
#include <RF24.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>

#define PACKET_SIZE 30
#define CONTINUE 0x00
#define EOM 0xFF

extern RF24 radio;
extern LiquidCrystal_I2C lcd;

//tx packet struct
struct packet {
  uint8_t packet_index;
  char data[PACKET_SIZE];
  uint8_t finalPacket;
};
typedef struct packet packet;

//tx queue for outgoing packets
struct PacketNode {
  packet data;
  struct PacketNode *next;
};
typedef struct PacketNode PacketNode;

//rx stack for the incoming packets
struct StackNode {
  packet data;
  struct StackNode *next;
};
typedef struct StackNode StackNode;

//linked list struct
struct MsgNode {
  char *data;
  struct MsgNode *prev;
  struct MsgNode *next;
};
typedef struct MsgNode MsgNode;

extern PacketNode *tx_queue_front;
extern PacketNode *tx_queue_back;

extern MsgNode *msg_head;
extern MsgNode *msg_tail;

extern StackNode *rx_stack;

//Queue functions
PacketNode* create_packet(packet p);
int enqueue(PacketNode **front, PacketNode **back, packet p);
packet dequeue(PacketNode **front, PacketNode **back);
bool queue_empty(PacketNode *front);
void queue_clear(PacketNode **front, PacketNode **back);

//Stack functions
void push(StackNode **top, packet p);
packet pop(StackNode **top);
bool stack_empty(StackNode *top);
void stack_clear(StackNode **top);

//Linked List functions
MsgNode* create_msg_node(char *val);
int insert_msg_head(char *val);
int insert_msg_middle(MsgNode *target, char *val);
int insert_msg_tail(char *val);
int delete_msg(MsgNode *target);
int saved_msgs();

//BST functions


//Radio setup
void init_radio_tx(int byte_address, int pwr_lvl);
void init_radio_rx(int byte_address, int pwr_lvl);

//Tx
int construct_packets(Node *head_ptr);
void transmit_packet();
void transmit_message(Node *head, LiquidCrystal_I2C &lcd);

//Rx
int receive_packet();
char* rebuild_message();
#endif