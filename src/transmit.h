#ifndef TRANSMIT_H
#define TRANSMIT_H

#include "message_comp.h"
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>

#define PACKET_SIZE 30
#define CONTINUE 0x00
#define EOM 0xFF

#define HC12_RX_PIN  12
#define HC12_TX_PIN  11
#define HC12_SET_PIN 13

//tx packet struct
struct packet {
  uint8_t packet_index;
  char data[PACKET_SIZE];
  uint8_t finalPacket;
};

//tx queue for outgoing packets
struct PacketNode {
  packet data;
  PacketNode *next;
};

//rx stack for the incoming packets
struct StackNode {
  packet data;
  StackNode *next;
};

//linked list struct
struct MsgNode {
  char *data;
  MsgNode *prev;
  MsgNode *next;
};

extern SoftwareSerial hc12;
extern LiquidCrystal_I2C lcd;

extern PacketNode *tx_queue_front;
extern PacketNode *tx_queue_back;

extern MsgNode *msg_head;
extern MsgNode *msg_tail;

extern StackNode *rx_stack;

// BST definitions
#define MAXSIZE 6
#define DOT '.'
#define DASH '-'
#define ROOTKEY "===="
#define ROOTLETTER '\0'
#define BLANKKEY "????"
#define BLANKLETTER '?'

struct morseTreeNode {
  char key[MAXSIZE];
  char letter;
  struct morseTreeNode *left;
  struct morseTreeNode *right;
};
typedef struct morseTreeNode morseTreeNode;

// Tx
int construct_packets(Node *head_ptr);
void transmit_packet();
void transmit_message(Node *head, LiquidCrystal_I2C &lcd);

// Rx
int receive_packet();
char* rebuild_message();
void init_radio();

// inbox functions
int saved_msgs();
MsgNode* get_msg_at(int index);

// Linked List functions
MsgNode* create_msg_node(char *val);
int insert_msg_head(char *val);
int insert_msg_middle(MsgNode *target, char *val);
int insert_msg_tail(char *val);
int delete_msg(MsgNode *target);

// Queue functions
int enqueue(PacketNode **front, PacketNode **back, packet p);
packet dequeue(PacketNode **front, PacketNode **back);
bool queue_empty(PacketNode *front);
void queue_clear(PacketNode **front, PacketNode **back);

// Stack functions
void push(StackNode **top, packet p);
packet pop(StackNode **top);
bool stack_empty(StackNode *top);
void stack_clear(StackNode **top);

// Translate
Node *translate_message(Node *head_ptr);

// BST functions
morseTreeNode *initNode(const char *key, char letter);
morseTreeNode *insertNode(morseTreeNode *root, const char *key, char letter, int depth);
morseTreeNode *buildTree(morseTreeNode *root);
char findLetter(char *searchKey, morseTreeNode *root, int searchDepth);
char *decodeMessage(char *message, int letterCount, morseTreeNode *root);

#endif
