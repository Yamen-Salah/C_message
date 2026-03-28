#include "transmit.h"
#include "message_comp.h"
#include "ui.h"
#include <Arduino.h>

// The Radios have 6 adresses used to define wherer they store data kind of
// "channels"
const byte address[][6] = {"Ch001", "Ch002", "Ch003",
                           "Ch004", "Ch005", "Ch006"};

// Special constants that define the power levels for the RF lib
const int pwrLevels[] = {RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX};

PacketNode *tx_queue_front = NULL;
PacketNode *tx_queue_back = NULL;

// Linked List (for message array ptrs)
MsgNode *msg_head = NULL;
MsgNode *msg_tail = NULL;

StackNode *rx_stack = NULL;

//hash table initialization, normal morse code + Space is ----
const char* hashTable[27] ={
  ".-",   //A
  "-...", //B
  "-.-.", //C
  "-..",  //D
  ".",    //E
  "..-.", //F
  "--.",  //G
  "....", //H
  "..",   //I
  ".---", //J
  "-.-",  //K
  ".-..",  //L
  "--",   //M
  "-.",   //N
  "---",  //O
  ".--.", //P
  "--.-", //Q
  ".-.",  //R
  "...",  //S
  "-",    //T
  "..-",  //U
  "...-", //V
  ".--",  //W
  "-..-", //X
  "-.--", //Y
  "--..", //Z
  "----"  //Space
};

// This function will have to change alot for the morse encoding
int construct_packets(
    Node *head_ptr) { // creates packets consisting of 30 bytes of char data
  queue_clear(&tx_queue_front, &tx_queue_back);
  Node *temp = head_ptr;
  int index = 0;

  while (temp != NULL) {
    packet chunk;
    memset(chunk.data, 0, sizeof(chunk.data));
    chunk.packet_index = index;

    for (int i = 0; i < PACKET_SIZE && temp != NULL; i++) {
      chunk.data[i] = temp->data;
      temp = temp->next;
    }
    chunk.finalPacket = (temp == NULL) ? EOM : CONTINUE;
    enqueue(&tx_queue_front, &tx_queue_back, chunk);
    index++;
  }
  return index;
}

// TX
void transmit_packet() { // transmits packets of chars
  packet chunk = dequeue(&tx_queue_front, &tx_queue_back);
  bool ok = false;
  for (int attempt = 0; attempt < 3 && !ok; attempt++) {
    ok = radio.write(&chunk, sizeof(chunk));
  }
  Serial.print(F("[TX] pkt "));
  Serial.print(chunk.packet_index);
  Serial.println(ok ? F(": ACK") : F(": no ACK"));
}

void transmit_message(Node *head, LiquidCrystal_I2C &lcd) {
  int totalPackets = construct_packets(head);
  Serial.print(F("[TX] "));
  Serial.print(totalPackets);
  Serial.println(F(" packet(s)"));
  for(int i = 0; i < totalPackets; i++) {
    transmit_packet();
    progress_bar(i, totalPackets, lcd);
  }
  delay(500);
}

// RX
int receive_packet() {
  packet chunk;
  radio.read(&chunk, sizeof(chunk));

  // Count how many packets are already on the stack — the next valid packet_index must equal this
  int depth = 0;
  for (StackNode *p = rx_stack; p != NULL; p = p->next) depth++;

  if (chunk.data[0] == '\0' || chunk.packet_index != depth) {
    Serial.print(F("[RX] bad pkt idx="));
    Serial.print(chunk.packet_index);
    Serial.print(F(" depth="));
    Serial.print(depth);
    Serial.print(F(" data[0]="));
    Serial.println((int)chunk.data[0]);
    stack_clear(&rx_stack);
    return -1;
  }
  push(&rx_stack, chunk);
  return chunk.packet_index;
}

// Rebuilds the message back to front from the stack
// uses a stack because its much easier to see the last packet
char *rebuild_message() {
  if (stack_empty(rx_stack))
    return NULL;

  packet chunk = pop(&rx_stack);
  int packetNum = chunk.packet_index;
  int finalChunkSize = strnlen(chunk.data, PACKET_SIZE);

  int totalChars = (packetNum * PACKET_SIZE) + finalChunkSize + 1;
  char *message = (char *)malloc(totalChars);
  if (message == NULL) {
    stack_clear(&rx_stack);
    return NULL;
  }
  message[totalChars - 1] = '\0';

  int arrPos = chunk.packet_index * PACKET_SIZE;
  for (int j = 0; j < finalChunkSize; j++) {
    message[arrPos + j] = chunk.data[j];
  }
  for (int i = 0; i < packetNum; i++) {
    if (stack_empty(rx_stack)) {
      free(message);
      return NULL;
    }
    chunk = pop(&rx_stack);
    arrPos = chunk.packet_index * PACKET_SIZE;
    for (int j = 0; j < PACKET_SIZE && chunk.data[j] != '\0'; j++) {
      message[arrPos + j] = chunk.data[j];
    }
  }
  return message;
}

// Radio Init bruv
void init_radio_tx(int byte_address, int pwr_lvl) {
  stack_clear(&rx_stack);
  radio.openWritingPipe(address[byte_address - 1]);
  radio.setPALevel(pwrLevels[pwr_lvl]);
  radio.stopListening();
}

void init_radio_rx(int byte_address, int pwr_lvl) {
  stack_clear(&rx_stack);                          // discard any stale packets from prior session
  radio.openReadingPipe(1, address[byte_address - 1]);
  radio.setPALevel(pwrLevels[pwr_lvl]);
  radio.flush_rx();                                // flush FIFO before starting to listen
  radio.startListening();
}

// Stack implementation functions
void push(StackNode **top, packet p) {
  StackNode *p_new = (StackNode *)malloc(sizeof(StackNode));
  if (p_new == NULL) {
    memory_full(lcd);
    return;
  }
  p_new->data = p;
  p_new->next = *top;
  *top = p_new;
}

packet pop(StackNode **top) {
  packet p = {};
  if (*top == NULL)
    return p;
  StackNode *temp = *top;
  p = temp->data;
  *top = temp->next;
  free(temp);
  return p;
}

bool stack_empty(StackNode *top) { return top == NULL; }

void stack_clear(StackNode **top) {
  while (*top)
    pop(top);
}

// Queue implementation functions
PacketNode *create_packet(packet p) {
  PacketNode *p_new = (PacketNode *)malloc(sizeof(PacketNode));
  if (p_new == NULL)
    return NULL;
  p_new->data = p;
  p_new->next = NULL;
  return p_new;
}

int enqueue(PacketNode **front, PacketNode **back, packet p) {
  PacketNode *p_new = create_packet(p);
  if (p_new == NULL)
    return -1;
  if (*back != NULL) {
    (*back)->next = p_new;
    *back = p_new;
  } else {
    *front = p_new;
    *back = p_new;
  }
  return 0;
}

packet dequeue(PacketNode **front, PacketNode **back) {
  packet p = {};
  if (*front == NULL)
    return p;

  PacketNode *temp = *front;
  p = temp->data;
  *front = (*front)->next;

  if (*front == NULL) {
    *back = NULL;
  }

  free(temp);
  return p;
}

bool queue_empty(PacketNode *front) { return front == NULL; }

void queue_clear(PacketNode **front, PacketNode **back) {
  while (*front)
    dequeue(front, back);
}

// Linked list functions
MsgNode *create_msg_node(char *val) {
  MsgNode *p_new = (MsgNode *)malloc(sizeof(MsgNode));
  if (p_new == NULL)
    return NULL;
  p_new->data = val;
  p_new->prev = NULL;
  p_new->next = NULL;
  return p_new;
}

int insert_msg_head(char *val) {
  MsgNode *p_new = create_msg_node(val);
  if (p_new == NULL)
    return -1;
  if (msg_head != NULL) {
    p_new->next = msg_head;
    msg_head->prev = p_new;
    msg_head = p_new;
  } else {
    msg_head = p_new;
    msg_tail = p_new;
  }
  return 0;
}

int insert_msg_middle(MsgNode *target, char *val) {
  if (target == NULL)
    return insert_msg_head(val);
  MsgNode *p_new = create_msg_node(val);
  if (p_new == NULL)
    return -1;

  p_new->prev = target;
  p_new->next = target->next;

  if (target->next != NULL) {
    target->next->prev = p_new;
  } else {
    msg_tail = p_new;
  }
  target->next = p_new;
  return 0;
}

int insert_msg_tail(char *val) {
  MsgNode *p_new = create_msg_node(val);
  if (p_new == NULL)
    return -1;
  if (msg_tail != NULL) {
    msg_tail->next = p_new;
    p_new->prev = msg_tail;
    msg_tail = p_new;
  } else {
    msg_head = p_new;
    msg_tail = p_new;
  }
  return 0;
}

int delete_msg(MsgNode *target) {
  if (target == NULL)
    return -1;
  if (target == msg_head && target == msg_tail) {
    msg_head = NULL;
    msg_tail = NULL;
  } else if (target == msg_head) {
    msg_head = target->next;
    msg_head->prev = NULL;
  } else if (target == msg_tail) {
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

int saved_msgs() {
  int count = 0;
  MsgNode *p = msg_head;
  while (p != NULL) {
    count++;
    p = p->next;
  }
  return count;
}

int hash(char letter){
    //since we are only dealing with letters from a-z and a space,
    //this hash function will directly translate the characters ASCII
    //to the hash.

    // 0 - 25 will be letters, 26 is space

    if ('A' <= letter && letter <= 'Z'){
        return letter - 65;
    } else if ('a' <= letter && letter <= 'z'){
        return letter - 97;
    } else if (letter == ' '){
        return 26;
    } else {
        return -1;
    }
}

Node* translate_message(Node* head_ptr){
  Node* new_head = NULL;    //basically making a new linked list
  Node* new_tail = NULL;
  Node* temp = head_ptr;

  while(temp != NULL){      //iterate through every character in message
    int hash_val = hash(temp->data);  //not using % because hash function always return proper numbers

    if(hash_val != -1) {    // Only translate if it's a valid letter or space
      const char* encoded_letter = hashTable[hash_val];
      int i = 0;

      //Append code
      while(encoded_letter[i] != '\0'){ //iterate through dots and dashs of encoded letter to append to linked list
        Node* p_new = (Node*) malloc(sizeof(Node));
        if (p_new != NULL) {    //if malloc worked, add peice of the code to the linked list, and repeat
          p_new->data = encoded_letter[i];
          p_new->next = NULL;
          p_new->prev = new_tail;

          if (new_tail != NULL) new_tail->next = p_new;
          else new_head = p_new;

          new_tail = p_new;
        }
        i++;
      }

      Node* space_node = (Node*) malloc(sizeof(Node)); //Add a space after every letter to separate morse letters
      if (space_node != NULL) { //if malloc works, add a space to end of linked list
        space_node->data = ' ';
        space_node->next = NULL;
        space_node->prev = new_tail;

        if (new_tail != NULL) new_tail->next = space_node;
        else new_head = space_node;

        new_tail = space_node;
      }
    }
    temp = temp->next;
  }

  return new_head; //return the linked list to be transmitted
}

// BST functions
int decodeCursor = 0;

// Creating a tree. root has key of "===="
morseTreeNode *initNode(const char *key, char letter) { // initialize a new node;
  morseTreeNode *newNode = (morseTreeNode *)malloc(sizeof(morseTreeNode)); // create newnode
    if (newNode != NULL) {   // make sure malloc did not return null
      newNode->letter = letter; // the letter produced by each morse code
      newNode->left = NULL;
      newNode->right = NULL;
      strncpy(newNode->key, key, MAXSIZE - 1); // copy morse code to the new allocated node
      newNode->key[MAXSIZE - 1] = '\0';
      return newNode;
    }
    free(newNode);
    return NULL;
}
// upon first function call, depth is 0.
//  works by inserting a letter only once we reach the end of its keys. Thus,
//  nodes are made to ensure that it is compounding
// with the longest keys at the bottom of the tree. this is checked using depth
// after insertions are completed, the tree will act like a BST for searching
morseTreeNode *insertNode(morseTreeNode *root, const char *key, char letter, int depth) {
  if (root == NULL)
    root = initNode(
    BLANKKEY, BLANKLETTER); // need a false node.. we will fill this later
    // with the proper key. makes it so that longest
    // morse codes are at bottom of tree and their
    // depth corresponds to their key length;
  if (depth == (int)strlen(key)) { // want to stop as we reach the depth that corresponds with
    // the end of the key of the letter inserted
    root->letter = letter;
    for (int i = 0; i < (int)strlen(key); i++){
      root->key[i] = key[i];
    }
    root->key[strlen(key)] = '\0';
    return root;
  } 
  else {
  if (key[depth] == DASH) { // next thing is a dash so go right
    depth++;
    root->right = insertNode(root->right, key, letter, depth);
  } 
  else { // otherwise we want to go left
    depth++;
    root->left = insertNode(root->left, key, letter, depth);
  }
  }
  return root;
}

morseTreeNode *buildTree(morseTreeNode *root) {

  root = insertNode(root, ".-", 'A', 0);
  root = insertNode(root, "-...", 'B', 0);
  root = insertNode(root, "-.-.", 'C', 0);
  root = insertNode(root, "-..", 'D', 0);
  root = insertNode(root, ".", 'E', 0);
  root = insertNode(root, "..-.", 'F', 0);
  root = insertNode(root, "--.", 'G', 0);
  root = insertNode(root, "....", 'H', 0);
  root = insertNode(root, "..", 'I', 0);
  root = insertNode(root, ".---", 'J', 0);
  root = insertNode(root, "-.-", 'K', 0);
  root = insertNode(root, ".-..", 'L', 0);
  root = insertNode(root, "--", 'M', 0);
  root = insertNode(root, "-.", 'N', 0);
  root = insertNode(root, "---", 'O', 0);
  root = insertNode(root, ".--.", 'P', 0);
  root = insertNode(root, "--.-", 'Q', 0);
  root = insertNode(root, ".-.", 'R', 0);
  root = insertNode(root, "...", 'S', 0);
  root = insertNode(root, "-", 'T', 0);
  root = insertNode(root, "..-", 'U', 0);
  root = insertNode(root, "...-", 'V', 0);
  root = insertNode(root, ".--", 'W', 0);
  root = insertNode(root, "-..-", 'X', 0);
  root = insertNode(root, "-.--", 'Y', 0);
  root = insertNode(root, "--..", 'Z', 0);
  root = insertNode(root, "----", ' ', 0);

  return root;
}

char *getMorse(char *message) {
  if (message[decodeCursor] == ' ')
    decodeCursor++;
  int scanPos = decodeCursor;
  int morseLength = 0; // account for null
  while (message[scanPos] != '\0' && message[scanPos] != ' ') {
    morseLength++;
    scanPos++;
  }
  char *morseCode = (char *)malloc(morseLength + 1);
  if (morseCode == NULL)
    return (morseCode);
  morseCode[morseLength] = '\0';
  for (int i = 0; i < morseLength; i++) {
    morseCode[i] = message[decodeCursor];
    decodeCursor++;
  }
  return morseCode;
}

// if letter cannot be found, return ? as the letter.
char findLetter(char *searchKey, morseTreeNode *root, int searchDepth) {
  if (root == NULL)
    return '?';
  if (searchDepth == (int)strlen(searchKey)) {
    return ((strcmp(searchKey, root->key) == 0) ? (root->letter) : '?');
  }
  if (searchKey[searchDepth] == DOT) {
    searchDepth++;
    return findLetter(searchKey, root->left, searchDepth);
  } else {
    searchDepth++;
    return findLetter(searchKey, root->right, searchDepth);
  }
}

// pass in message string that is morse code. create a new string of decoded
// letters adn return that pointer. letterCount is intially zero. matches number
// of letters decoded.
char *decodeMessage(char *message, int letterCount, morseTreeNode *root) {
  if (message[decodeCursor] == '\0') {
    decodeCursor = 0;
    message[letterCount] = '\0';
    message = (char *)realloc(message, letterCount + 1);
    if (message != NULL) {
      return message;
    } else {
      free(message);
      return NULL;
    }
  } else {
    char *morseCode = getMorse(message);
    if (morseCode == NULL) {
      decodeCursor = 0;
      return NULL;
    } else if (morseCode[0] == '\0') {
      // trailing separator consumed; decodeCursor is now at '\0', let base case handle cleanup
      free(morseCode);
      message = decodeMessage(message, letterCount, root);
    } else {
      char foundLetter = findLetter(morseCode, root, 0);
      free(morseCode);
      message[letterCount] = foundLetter;
      letterCount++;
      message = decodeMessage(message, letterCount, root);
    }
  }
  return message;
}

