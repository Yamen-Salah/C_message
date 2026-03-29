#include <SoftwareSerial.h>
#include "message_comp.h"
#include "ui.h"
#include "transmit.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
// HC-12 Pinout: RX=12, TX=11, SET=13
SoftwareSerial hc12(HC12_RX_PIN, HC12_TX_PIN);

const int encoder_B = 8;
const int encoder_A = 2;
const int selectBtn = 6;
const int leftArrow = 5;
const int upArrow   = 4;
const int rightArrow = 3;
const int downArrow  = 7;

enum Mode { RX, TX };
morseTreeNode* root = NULL;
Mode current_mode = TX;
bool rxListen = true;
int rxMsgIndex = 0;
int currentSet = 0;

unsigned long lastLeft = 0;
unsigned long lastRight = 0;
unsigned long lastUp = 0;
unsigned long lastDown = 0;

void encoder_interrupt() {
  readEncoder(encoder_A, encoder_B);
}

void setup() {
  pinMode(HC12_SET_PIN, OUTPUT);
  digitalWrite(HC12_SET_PIN, HIGH); // HIGH = normal mode
  hc12.begin(9600);

  lcd.init();
  lcd.backlight();

  root = initNode((char*)ROOTKEY, ROOTLETTER);
  root = buildTree(root);

  pinMode(encoder_A, INPUT_PULLUP);
  pinMode(encoder_B, INPUT_PULLUP);
  pinMode(selectBtn, INPUT_PULLUP);
  pinMode(upArrow,   INPUT_PULLUP);
  pinMode(downArrow, INPUT_PULLUP);
  pinMode(leftArrow, INPUT_PULLUP);
  pinMode(rightArrow, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(encoder_A), encoder_interrupt, CHANGE);
}

void loop() {
  if (current_mode == RX) {
    // only try to receive bytes when actively listening
    if (rxListen) {
      if (hc12.available()) {
        int pktIndex = receive_packet();
        if (pktIndex == -1) {
          rx_failure(lcd);
          return;
        }
        if (pktIndex >= 0) rx_pkt_count(pktIndex, lcd);

        // check top of stack for EOM flag - means all packets have arrived
        if (pktIndex >= 0 && rx_stack != NULL && rx_stack->data.finalPacket == EOM) {
          char* message = rebuild_message();

          if (message != NULL) {
            // decode morse back to plaintext and save to inbox
            message = decodeMessage(message, 0, root);
            if (message == NULL) { decode_failure(lcd); return; }
            insert_msg_head(message);
            rxMsgIndex = 0;
            pending_msgs(saved_msgs(), lcd);
          }
          else {
            rebuild_failure(lcd);
          }
        }
      }
    }

    SelectAction selectAction = handleSelect(selectBtn, lcd);

    // long press switches back to tx and clears all state
    if (selectAction == LONG_SELECT) {
      currentSet = 0;
      insertPos = 0;
      editMode = false;
      current_mode = TX;
      delete_all_data();
      init_radio();
      return;
    }

    int msgCount = saved_msgs();

    // short press while browsing deletes the currently viewed message
    if (selectAction == SHORT_SELECT && !rxListen && msgCount > 0) {
      delete_msg(get_msg_at(rxMsgIndex));
      msgCount = saved_msgs();
      // clamp index so it stays valid after deletion
      rxMsgIndex = msgCount ? min(rxMsgIndex, msgCount - 1) : 0;
    }

    // right arrow exits listen mode to browse saved messages
    if (buttonPressed(rightArrow, lastRight) && rxListen) {
      rxListen = false;
      encoderPos = 0;
    }
    // left arrow goes back to actively listening for new packets
    if (buttonPressed(leftArrow, lastLeft) && !rxListen) {
      rxListen = true;
      rx_listen(lcd);
    }

    // browse and scroll messages when not listening
    if (!rxListen) {
      if (msgCount > 0) {
        if (buttonPressed(upArrow, lastUp) && rxMsgIndex > 0) {
          rxMsgIndex--;
          encoderPos = 0;
        }
        if (buttonPressed(downArrow, lastDown) && rxMsgIndex < msgCount - 1) {
          rxMsgIndex++;
          encoderPos = 0;
        }
        MsgNode *currentMsg = get_msg_at(rxMsgIndex);
        if (currentMsg != NULL) {
          // clamp encoder scroll to the overflow length of the message
          int maxScroll = max(0, (int)strlen(currentMsg->data) - 16);
          encoderPos = constrain(encoderPos, 0, maxScroll);
          rx_display_message(currentMsg->data, encoderPos, rxMsgIndex, lcd);
        }
      }
      else {
        no_messages(lcd);
      }
    }
  }
  else {
    SelectAction selectAction = handleSelect(selectBtn, lcd);

    // long press translates the composed message to morse and transmits it
    if (selectAction == LONG_SELECT) {
      Node* morse_head = translate_message(p_head);
      transmit_message(morse_head, lcd);
      free_node_list(morse_head);

      delete_all_data();
      init_radio();
      editMode = false;
      current_mode = RX;
      rxListen = true;
      rx_listen(lcd);
      return;
    }

    if (selectAction == SHORT_SELECT) {
      if (!editMode) {
        // insert the selected character from the current charset at the cursor position
        const auto &cs = charSets[currentSet];
        if (cs.size > 0) {
          char c = cs.chars[constrain(encoderPos, 0, cs.size - 1)];
          Node *target = insertPos > 0 ? get_node_at(insertPos - 1) : nullptr;
          if ((target ? insert_data_at_middle(target, c) : insert_data_at_head(c)) == 0)
            insertPos++;
        }
      } else {
        // delete the character under the cursor in edit mode
        Node *node = get_node_at(updateCursor);
        if (node) {
          find_and_delete_data(node);
          int len = get_message_length();
          if (len == 0) { updateCursor = 0; insertPos = 0; }
          else updateCursor = min(updateCursor, len - 1);
        }
      }
    }

    // left/right cycle through the three character sets (letters, numbers, symbols)
    // Turns out were only using one charset in the hash, so everything else is unsupported
    // for now at least. It does show how the code handles that case though, so we left it in 
    if (!editMode) {
      if (buttonPressed(rightArrow, lastRight)) {
        currentSet = (currentSet + 1) % 3;
        encoderPos = 0;
      }
      if (buttonPressed(leftArrow, lastLeft)) {
        currentSet = (currentSet - 1 + 3) % 3;
        encoderPos = 0;
      }
    }

    // down arrow enters edit mode with the cursor placed at the last inserted character
    if (buttonPressed(downArrow, lastDown) && !editMode) {
      int messsageLen = get_message_length();
      editMode = true;
      updateCursor = constrain(insertPos - 1, 0, messsageLen - 1);
      encoderPos = updateCursor;
    }

    // up arrow exits edit mode and restores insert position after the cursor
    if (buttonPressed(upArrow, lastUp) && editMode) {
      editMode = false;
      insertPos = constrain(updateCursor + 1, 0, get_message_length());
      encoderPos = 0;
    }

    // display differs based on mode: edit shows cursor on message, normal shows character picker
    if (editMode) {
      int len = get_message_length();
      if (len > 0) {
        encoderPos = constrain(encoderPos, 0, len - 1);
        updateCursor = encoderPos;
      }
      tx_display_message(updateCursor, true, lcd);
    }
    else {
      encoderPos = constrain(encoderPos, 0, charSets[currentSet].size - 1);
      letter_scroll(encoderPos, lcd);
      tx_display_message(insertPos, false, lcd);
    }
  }
}
