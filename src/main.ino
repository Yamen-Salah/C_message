#include <PinChangeInterrupt.h>
#include "message_comp.h"
#include "ui.h"
#include "transmit.h"


LiquidCrystal_I2C lcd(0x27, 16, 2);
//CE, CSN for rf
RF24 radio(A2, A1);

//potentially changeable constants
const int sck = 13;
const int miso = 12;
const int mosi = 11;

const int encoder_B = 8;
const int encoder_A = 7;

const int selectBtn = 6;
const int leftArrow = 5;
const int upArrow = 4;
const int rightArrow = 3;
const int downArrow = 2;

//globals and or variables used in the loop
int currentSet = 0;
enum Mode {RX, TX };
Mode current_mode = TX;
bool rxListen = true;
int rxMsgIndex = 0;
int rxCursorPos = 0;

//time since last keypapd press
unsigned long lastLeft = 0;
unsigned long lastRight = 0;
unsigned long lastUp = 0;
unsigned long lastDown = 0;

//Pinmode interrupt so that the encoder does skip and acts better
void encoder_interrupt() {
  readEncoder(encoder_A, encoder_B);
}

void setup() {
  radio.begin();
  lcd.init();
  lcd.backlight();
  init_radio_tx(1, 1);

  // Test messages for teting inbox
  // insert_msg_head(strdup("Hello!"));

  pinMode(encoder_A, INPUT_PULLUP);
  pinMode(encoder_B, INPUT_PULLUP);
  pinMode(selectBtn, INPUT_PULLUP);
  pinMode(upArrow, INPUT_PULLUP);
  pinMode(downArrow, INPUT_PULLUP);
  pinMode(leftArrow, INPUT_PULLUP);
  pinMode(rightArrow, INPUT_PULLUP);
  attachPCINT(digitalPinToPCINT(encoder_A), encoder_interrupt, CHANGE);
}

void loop() {
  //wow system interrupts are so cool, i never need to poll. but -30bytes ram :(
  if (current_mode == RX) {

    if (rxListen) { //if not browsing messages
      if (radio.isChipConnected() && radio.available()) { //if a packet has arrived

        int pktIndex = receive_packet();
        rx_count(pktIndex, lcd);

        if (rx_stack->data.finalPacket == EOM) { //was the final packet
          char* message = rebuild_message(); //returns a char ptr to the reassembled message
          if (message != NULL) {
            insert_msg_head(message); //put the new msg at top of inbox
            rxMsgIndex = 0;
          }
          else {
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
      init_radio_tx(1, 1);
      return;
    }

    int msgCount = saved_msgs();

    if (selectAction == SHORT_SELECT && !rxListen && msgCount > 0) {
      MsgNode *current = msg_head;
      for (int i = 0; i < rxMsgIndex && current; i++){ //go to the target Node
        current = current->next;
      }
        delete_msg(current);
        msgCount = saved_msgs();
        rxMsgIndex = msgCount ? min(rxMsgIndex, msgCount - 1) : 0;
    }

    //Right: enter browse mode
    if (buttonPressed(rightArrow, lastRight) && rxListen) {
      rxListen = false;
      rxCursorPos = 0;
      encoderPos = 0;
    }
    //Left: exit browse mode back to listen
    if (buttonPressed(leftArrow, lastLeft) && !rxListen) {
      rxListen = true;
      rxCursorPos = 0;
      ui_listen(lcd);
    }

    // Up/Down cycle through messages; encoder scrolls within a message
    if (!rxListen) {
      if (msgCount > 0) {
        if (buttonPressed(upArrow, lastUp) && rxMsgIndex > 0) {
          rxMsgIndex--;
          rxCursorPos = 0;
          encoderPos = 0;
        }
        if (buttonPressed(downArrow, lastDown) && rxMsgIndex < msgCount - 1) {
          rxMsgIndex++;
          rxCursorPos = 0;
          encoderPos = 0;
        }

        MsgNode *currentMsg = msg_head;
        for (int i = 0; i < rxMsgIndex && currentMsg != NULL; i++){
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
  }

  else {
    int selectAction = handleSelect(selectBtn, lcd);

    //Long press sends message and switch to RX
    if (selectAction == LONG_SELECT) {
      transmit_message(lcd);

      delete_all_data();
      init_radio_rx(1, 1);
      editMode = false;
      current_mode = RX;
      rxListen = true;
      ui_listen(lcd);
      return;
    }

    //L/R cycle character layer in compose mode
    if (!editMode) {
      if (buttonPressed(rightArrow, lastRight)) {
        currentSet = (currentSet + 1) % 3;
        encoderPos = 0;  //reset
      }
      if (buttonPressed(leftArrow, lastLeft)) {
        currentSet = (currentSet - 1 + 3) % 3;
        encoderPos = 0;
      }
    }

    //Down arrow enters edit mode
    if (buttonPressed(downArrow, lastDown) && !editMode) {
      int len = get_message_length();
      editMode = true;
      updateCursor = constrain(insertPos - 1, 0, len - 1);
      encoderPos = updateCursor;  //set encoder to cursor pos
    }

    //Up arrow exits edit mode
    if (buttonPressed(upArrow, lastUp) && editMode) {
      editMode = false;
      insertPos = constrain(updateCursor + 1, 0, get_message_length());
      encoderPos = 0;  //reset for compose mode
    }

    //Displays LCD
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
