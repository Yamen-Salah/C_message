#include <PinChangeInterrupt.h>
#include "message_comp.h"
#include "ui.h"
#include "transmit.h"
#include <stdio.h>


LiquidCrystal_I2C lcd(0x27, 16, 2);
//CE, CSN for rf
RF24 radio(9, 10);

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
enum Mode {RX, TX }; 
int current_mode = TX;
bool rxListen = true;
int rxMsgIndex = 1;
int rxCursorPos = 0;

//time since last keypapd press
unsigned long lastLeftPress = 0;
unsigned long lastRightPress = 0;
unsigned long lastUp = 0;
unsigned long lastDown = 0;

//Pinmode interrupt so that the encoder does skip and acts better
void encoder_interrupt() {
  readEncoder(encoder_A, encoder_B);
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void setup() {
  Serial.begin(9600);
  radio.begin();
  lcd.init();
  lcd.backlight();
  init_radio_tx(1, 1);

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
    Serial.println(freeRam());

  //wow system interrupts are so cool, i never need to poll. but -30bytes ram :(
  if (current_mode == RX) {

    if (rxListen) {
      int status = 0;
      if (radio.available()) {
        if (radio.testRPD()) {
          status = receive_packet();
          rx_count(status, lcd);
        } else {
          radio.flush_rx();
        }
      }
      if (status == 1) {
        char* message = rebuild_message();
        if (message != NULL) {
          insert_msg_head(message);
          rxMsgIndex = 0;
        } else {
          lcd.setCursor(0, 0);
          lcd.print(F("Rebuild Failed  "));
          delay(1000);
        }
      }
    }

    int selectAction = handleSelect(selectBtn, lcd);

    if (selectAction == LONG_SELECT) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("   TX Mode      "));
      
      Serial.print(F("   TX Mode      "));
      Serial.println(freeRam());
      
      currentSet = 0;
      encoderPos = constrain(encoderPos, 0, charSets[currentSet].size - 1);
      insertPos = 0;
      updateCursor = 0;
      editMode = false;
      current_mode = TX;
      Serial.print(F("delete all data"));
      Serial.println(freeRam());
      delete_all_data();
      init_radio_tx(2, 1);
      delay(500);
      return;
    }

    int msgCount = saved_msgs();

    //Short select deletes current message when browsing
    if (selectAction == SHORT_SELECT && !rxListen && msgCount > 0) {
      MsgNode *currentMsg = msg_head;
      for (int i = 0; i < rxMsgIndex && currentMsg != NULL; i++)
        currentMsg = currentMsg->next;

      if (currentMsg != NULL) {
        delete_msg(currentMsg);
        msgCount = saved_msgs();
        
        if (msgCount == 0) {
          rxListen = true;
          rxMsgIndex = 0;
          ui_listen(lcd);
        } else if(rxMsgIndex >= msgCount) {
          rxMsgIndex = msgCount - 1;
        }
      }
    }

    //Right button switch to browse mode
    if (buttonPressed(rightArrow, lastRightPress)) {
      rxListen = false;
      rxCursorPos = 0;
    }
    //Left button switch to listen mode
    if (buttonPressed(leftArrow, lastLeftPress)) {
      rxListen = true;
      ui_listen(lcd);
    }

    // Up/Down cycle through messages when browsing
    if (!rxListen && msgCount > 0) {
      if (buttonPressed(upArrow, lastUp)) {
        if (rxMsgIndex > 0) {
          rxMsgIndex--;
          rxCursorPos = 0;
        }
      }
      if (buttonPressed(downArrow, lastDown)) {
        if (rxMsgIndex < msgCount - 1) {
          rxMsgIndex++;
          rxCursorPos = 0;
        }
      }

      MsgNode *currentMsg = msg_head;
      for (int i = 0; i < rxMsgIndex && currentMsg != NULL; i++)
        currentMsg = currentMsg->next;

      if (currentMsg != NULL) {
        rx_display_message(currentMsg->data, rxCursorPos, rxMsgIndex, lcd);
      }
    }
  }

  else if(current_mode == TX){
    int selectAction = handleSelect(selectBtn, lcd);

    //Long press sends message and switch to RX
    if (selectAction == LONG_SELECT) {
      transmit_message(lcd);

      Serial.println(freeRam());
      Serial.print(F("LONG SELECT"));

      int messageLen = saved_msgs();

      if (messageLen > 0) {
        delete_all_data();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("   MSG Sent     "));
      }
      else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("   RX Mode      "));
        init_radio_rx(1,1);
      }
      insertPos = 0;
      updateCursor = 0;
      editMode = false;
      current_mode = RX;
      rxListen = true;

      delay(500);
      return;
    }

    //L/R cycle character layer in compose mode
    if (!editMode) {
      if (buttonPressed(rightArrow, lastRightPress)) {
        currentSet = (currentSet + 1) % 3;
        encoderPos = 0;  //reset to center
      }
      if (buttonPressed(leftArrow, lastLeftPress)) {
        currentSet = (currentSet - 1 + 3) % 3;
        encoderPos = 0;
      }
    }

    //Down arrow enters edit mode
    if (buttonPressed(downArrow, lastDown)) {
      if (!editMode && get_message_length() > 0) {
        editMode = true;
        updateCursor = constrain(insertPos - 1, 0, get_message_length() - 1);
        encoderPos = updateCursor;  //set encoder to cursor pos
      }
    }

    //Up arrow exits edit mode
    if (buttonPressed(upArrow, lastUp)) {
      if (editMode) {
        editMode = false;
        insertPos = constrain(updateCursor + 1, 0, get_message_length());
        encoderPos = 0;  //reset for compose mode
      } 
    }

    //Displays LCD
    if (editMode) {
      if (get_message_length() > 0) {
        encoderPos = constrain(encoderPos, 0, get_message_length() - 1);
        updateCursor = encoderPos;
      }
      tx_display_message(updateCursor, true, lcd);
    }
    else {
      encoderPos = constrain(encoderPos, 0, charSets[currentSet].size - 1);
      letter_scroll(encoderPos, lcd);
      int messageLength = get_message_length();
      insertPos = constrain(insertPos, 0, messageLength);
      tx_display_message(insertPos, false, lcd);
    }
  }
}