#include "ui.h"
#include "message_comp.h"
#include <LiquidCrystal_I2C.h>

//Debounce times
const int DEBOUNCE_MS   = 100;
const int LONG_PRESS_MS = 600;

unsigned long lastSelect = 0;
bool selectHeld = false;
bool longPressDone = false;

volatile int encoderPos = 0;
volatile int lastTick = HIGH;

bool editMode = false;
int  insertPos = 0;
int  updateCursor = 0;

//Hardware interfacing functions
bool buttonPressed(int pin, unsigned long &lastPress) {
  if (digitalRead(pin) || millis() - lastPress <= DEBOUNCE_MS) return false;
  lastPress = millis();
  return true;
}

void readEncoder(int pinA, int pinB) {
  int aState = digitalRead(pinA);
  if(aState != lastTick && aState == LOW){
    digitalRead(pinB) == HIGH ? encoderPos++ : encoderPos--;
  }
  lastTick = aState;
}

int handleSelect(int pin, LiquidCrystal_I2C &lcd) {
  bool buttonPressed = (digitalRead(pin) == LOW);
  int state = SELECT_NONE;

  //Press starts recording the time
  if (buttonPressed && !selectHeld) {
    selectHeld = true;
    longPressDone = false;
    lastSelect = millis();
  }

  //Held, check if long press threshold reached
  else if (buttonPressed && selectHeld && !longPressDone) {
    if (millis() - lastSelect > LONG_PRESS_MS) {
      longPressDone = true;
      state = LONG_SELECT;
    }
  }

  //Released, if it wasn't a long press, its a short press
  else if (!buttonPressed && selectHeld) {
    if (!longPressDone && (millis() - lastSelect > DEBOUNCE_MS)) {
      if (!editMode) {
        int setSize = charSets[currentSet].size;
        if (setSize == 0) {
          selectHeld = false;
          longPressDone = false;
          return SELECT_NONE;
        }
        int charArrPos = encoderPos;
        charArrPos = constrain(charArrPos, 0, setSize - 1);
        char c = pgm_read_byte(&charSets[currentSet].chars[charArrPos]);
        int messageLength = get_message_length();

        int result = 0;
        if (insertPos <= 0 || messageLength == 0)
          result = insert_data_at_head(c);
        else {
          Node *target = get_node_at(insertPos - 1);
          if (target != NULL)
            result = insert_data_at_middle(target, c);
          else
            result = insert_data_at_head(c);  // Fallback if target is NULL
        }

        // Only increment insertPos if insertion succeeded
        if (result == 0) {
          insertPos++;
          // Ensure insertPos doesn't exceed the new message length
          insertPos = constrain(insertPos, 0, get_message_length());
        }
      }
      else {
        int messageLength = get_message_length();
        if (messageLength > 0) {
          Node *target = get_node_at(updateCursor);
          if (target != NULL) {
            find_and_delete_data(target);
            messageLength--;
            if (messageLength == 0) {
              updateCursor = 0;
              insertPos = 0;
            } else if (updateCursor >= messageLength) {
              updateCursor = messageLength - 1;
            }
          }
        }
      }
      state = SHORT_SELECT;
    }
    selectHeld = false;
    longPressDone = false;
  }

  return state;
}

void letter_scroll(int letter_index, LiquidCrystal_I2C &lcd) {
  //clear row
  lcd.setCursor(0, 0);
  lcd.print(F("                "));

  int setSize = charSets[currentSet].size;
  if (setSize == 0) return;
  
  // Constrain letter_index to valid range
  letter_index = constrain(letter_index, 0, setSize - 1);

  //main letter choices
  if (letter_index > 0) {
    lcd.setCursor(4, 0);
    lcd.print("< ");
    lcd.print((char)pgm_read_byte(&charSets[currentSet].chars[letter_index - 1]));
  }
  lcd.setCursor(8, 0);
  lcd.print((char)pgm_read_byte(&charSets[currentSet].chars[letter_index]));
  if (letter_index < setSize - 1) {
    lcd.setCursor(10, 0);
    lcd.print((char)pgm_read_byte(&charSets[currentSet].chars[letter_index + 1]));
    lcd.print(" >");
  }
}

//2 DISPLAY FUNCTIONS... ONE FOR THE MESSAGE COMPOSE WRITING ONE FOR SCROLLING AND EDITING THE MESSAGE
//possible to combine ltr
void tx_display_message(int cursorPos, bool editMode, LiquidCrystal_I2C &lcd) {
  int messageLength = get_message_length();

  if (editMode) {
    lcd.setCursor(0, 0);
    lcd.print(F("Edit Msg:Sel=Del"));
  }

  lcd.setCursor(0, 1);
  lcd.print(F("                "));

  if (messageLength == 0) {
    if (!editMode && (millis() / 500) % 2 == 0) {
      lcd.setCursor(0, 1);
      lcd.print("_");
    }
    return;
  }
  cursorPos = constrain(cursorPos, 0, messageLength);

  int windowStart = 0;
  int maxVisible = editMode ? 14 : 15;
  if (cursorPos > maxVisible) {
    windowStart = cursorPos - maxVisible;
  }
  // Ensure windowStart doesn't exceed message length
  windowStart = constrain(windowStart, 0, messageLength - 1);

  Node *p = get_node_at(windowStart);
  lcd.setCursor(0, 1);
  for (int i = windowStart, col = 0; i < messageLength && col < 16; i++, col++) {
    if (p == NULL) {
      // Fill remaining positions with spaces if we hit NULL unexpectedly
      while (col < 16) {
        lcd.print(' ');
        col++;
      }
      break;
    }
    //Ensure printing of non null chars
    char c = p->data;
    if (c == '\0') {
      lcd.print(' ');
    } else {
      if (editMode && i == cursorPos) {
        if ((millis() / 300) % 2 == 0)
          lcd.print(c);
        else
          lcd.write(0xFF);
      } else {
        lcd.print(c);
      }
    }
    p = p->next;
  }

  if (!editMode) {
    int cursorCol = cursorPos - windowStart;
    if (cursorCol >= 0 && cursorCol < 16 && (millis() / 500) % 2 == 0) {
      lcd.setCursor(cursorCol, 1);
      lcd.print("_");
    }
  }
}

void tx_show_translation(char *msg, LiquidCrystal_I2C &lcd){

}


void rx_display_message(char *msg, int cursorPos, int msgIndex, LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print("Msg ");
  lcd.print(msgIndex + 1);

  lcd.setCursor(0, 1);
  lcd.print("                ");
  
  int len = strlen(msg);

  lcd.setCursor(0, 1);
  for (int i = cursorPos, col = 0; i < len && col < 16; i++, col++) {
  lcd.print(msg[i]);
  }
}

void progress_bar(int packet_index, int totalPackets, LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("Sending "));
  lcd.print(packet_index + 1);
  lcd.print("/");
  lcd.print(totalPackets);

  int percent = ((packet_index + 1) * 100)/totalPackets;
  int barLength = map(percent, 0, 100, 0, 13);

  lcd.setCursor(0, 1);
  for (int i = 0; i < 13; i++) {
    if(i <= barLength) lcd.write(0xFF);
    else lcd.print(' ');
  }
    lcd.setCursor(13, 1);
    if (percent < 10) lcd.print("  ");
    else if (percent < 100) lcd.print(" ");
    lcd.print(percent);
}

void rx_count(int packet_index, LiquidCrystal_I2C &lcd){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F(" Receiving Msg! "));
  lcd.setCursor(0, 1);
  lcd.print(" Pkt: ");
  lcd.print(packet_index + 1);
  lcd.print("/");
  lcd.print("?");
}

void memory_full(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("  No Memory :(  "));
}

void ui_listen(LiquidCrystal_I2C &lcd){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  Listening...  "));
}

