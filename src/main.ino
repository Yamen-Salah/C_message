#include <PinChangeInterrupt.h>
#include "message_comp.h"
#include "ui.h"
#include "transmit.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
RF24 radio(A2, A1);  // CE=A2, CSN=A1; SCK/MISO/MOSI are hardware SPI (13/12/11)

const int encoder_B = 8;
const int encoder_A = 7;
const int selectBtn = 6;
const int leftArrow = 5;
const int upArrow   = 4;
const int rightArrow = 3;
const int downArrow  = 2;

morseTreeNode* root = NULL;
int currentSet = 0;
enum Mode { RX, TX };
Mode current_mode = TX;
bool rxListen = true;
int rxMsgIndex = 0;

unsigned long lastLeft = 0;
unsigned long lastRight = 0;
unsigned long lastUp = 0;
unsigned long lastDown = 0;

void encoder_interrupt() {
  readEncoder(encoder_A, encoder_B);
}

void setup() {
  Serial.begin(9600);
  bool radioOk = false;
  for (int attempt = 0; attempt < 5; attempt++) {
    delay(200);
    if (radio.begin()) { radioOk = true; break; }
  }
  Serial.println(radioOk ? F("[SETUP] radio OK") : F("[SETUP] radio FAILED"));

  lcd.init();
  lcd.backlight();
  init_radio_tx(1, 3);

  root = initNode((char*)ROOTKEY, ROOTLETTER);
  root = buildTree(root);

  pinMode(encoder_A, INPUT_PULLUP);
  pinMode(encoder_B, INPUT_PULLUP);
  pinMode(selectBtn, INPUT_PULLUP);
  pinMode(upArrow,   INPUT_PULLUP);
  pinMode(downArrow, INPUT_PULLUP);
  pinMode(leftArrow, INPUT_PULLUP);
  pinMode(rightArrow, INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(encoder_A), encoder_interrupt, CHANGE);
}

void loop() {
  if (current_mode == RX) {

    if (rxListen) {
      if (radio.available()) {
        int pktIndex = receive_packet();
        if (pktIndex == -1) { return; }
        if (pktIndex > 0) rx_count(pktIndex, lcd);

        if (rx_stack != NULL && rx_stack->data.finalPacket == EOM) {
          char* message = rebuild_message();
          if (message != NULL) {
            message = decodeMessage(message, 0, root);
            if (message == NULL) { decode_failure(lcd); return; }
            insert_msg_head(message);
            rxMsgIndex = 0;
            Serial.print(F("[RX] received: \""));
            Serial.print(message);
            Serial.println(F("\""));
            ui_msg_received(saved_msgs(), lcd);
          } else {
            lcd.setCursor(0, 0);
            lcd.print(F("Rebuild Failed  "));
            delay(500);
          }
        }
      }
    }

    int selectAction = handleSelect(selectBtn, lcd);

    if (selectAction == LONG_SELECT) {
      currentSet = 0;
      insertPos = 0;
      editMode = false;
      current_mode = TX;
      delete_all_data();
      init_radio_tx(1, 3);
      Serial.println(F("[MODE] TX"));
      return;
    }

    int msgCount = saved_msgs();

    if (selectAction == SHORT_SELECT && !rxListen && msgCount > 0) {
      MsgNode *current = msg_head;
      for (int i = 0; i < rxMsgIndex && current; i++) {
        current = current->next;
      }
      delete_msg(current);
      msgCount = saved_msgs();
      rxMsgIndex = msgCount ? min(rxMsgIndex, msgCount - 1) : 0;
    }

    if (buttonPressed(rightArrow, lastRight) && rxListen) {
      rxListen = false;
      encoderPos = 0;
    }
    if (buttonPressed(leftArrow, lastLeft) && !rxListen) {
      rxListen = true;
      ui_listen(lcd);
    }

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

        MsgNode *currentMsg = msg_head;
        for (int i = 0; i < rxMsgIndex && currentMsg != NULL; i++) {
          currentMsg = currentMsg->next;
        }

        if (currentMsg != NULL) {
          int maxScroll = max(0, (int)strlen(currentMsg->data) - 16);
          encoderPos = constrain(encoderPos, 0, maxScroll);
          rx_display_message(currentMsg->data, encoderPos, rxMsgIndex, lcd);
        }
      } else {
        ui_no_messages(lcd);
      }
    }

  } else {

    int selectAction = handleSelect(selectBtn, lcd);

    if (selectAction == LONG_SELECT) {
      Serial.print(F("[TX] sending: \""));
      for (Node* p = p_head; p != NULL; p = p->next) Serial.print(p->data);
      Serial.println(F("\""));
      Node* morse_head = translate_message(p_head);
      transmit_message(morse_head, lcd);
      Node* temp = morse_head;
      while (temp != NULL) { Node* next = temp->next; free(temp); temp = next; }

      delete_all_data();
      init_radio_rx(1, 3);
      editMode = false;
      current_mode = RX;
      rxListen = true;
      ui_listen(lcd);
      Serial.println(F("[MODE] RX"));
      return;
    }

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

    if (buttonPressed(downArrow, lastDown) && !editMode) {
      int len = get_message_length();
      editMode = true;
      updateCursor = constrain(insertPos - 1, 0, len - 1);
      encoderPos = updateCursor;
    }

    if (buttonPressed(upArrow, lastUp) && editMode) {
      editMode = false;
      insertPos = constrain(updateCursor + 1, 0, get_message_length());
      encoderPos = 0;
    }

    if (editMode) {
      int len = get_message_length();
      if (len > 0) {
        encoderPos = constrain(encoderPos, 0, len - 1);
        updateCursor = encoderPos;
      }
      tx_display_message(updateCursor, true, lcd);
    } else {
      encoderPos = constrain(encoderPos, 0, charSets[currentSet].size - 1);
      letter_scroll(encoderPos, lcd);
      tx_display_message(insertPos, false, lcd);
    }
  }
}
