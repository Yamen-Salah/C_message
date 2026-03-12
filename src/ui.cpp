#include "ui.h"
#include "message_comp.h"
#include <LiquidCrystal_I2C.h>

//Debounce times
const int DEBOUNCE_MS   = 200;
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
  bool pressed = (digitalRead(pin) == LOW);
  int state = SELECT_NONE;

  //Press starts recording the time
  if (pressed && !selectHeld) {
    selectHeld = true;
    longPressDone = false;
    lastSelect = millis();
  }

  //Held
  else if (pressed && !longPressDone && millis() - lastSelect > LONG_PRESS_MS) {
    longPressDone = true;
    state = LONG_SELECT;
  }

  //Released
  else if (!pressed && selectHeld) {
    if (!longPressDone && millis() - lastSelect > DEBOUNCE_MS) {
      if (!editMode) {
        const auto &cs = charSets[currentSet];
        if (cs.size > 0) {
          char c = cs.chars[constrain(encoderPos, 0, cs.size - 1)];
          Node *target = insertPos > 0 ? get_node_at(insertPos - 1) : nullptr;
          if ((target ? insert_data_at_middle(target, c) : insert_data_at_head(c)) == 0)
            insertPos++;
        }
      } else {
        Node *node = get_node_at(updateCursor);
        if (node) {
          find_and_delete_data(node);
          int len = get_message_length();
          if (len == 0) { updateCursor = 0; insertPos = 0; }
          else updateCursor = min(updateCursor, len - 1);
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
  int setSize = charSets[currentSet].size;
  if (setSize == 0) return;

  letter_index = constrain(letter_index, 0, setSize - 1);

  lcd.setCursor(0, 0);
  lcd.print(F("    "));                                   // cols 0-3: left padding

  if (letter_index > 0) {                                 // cols 4-6: prev char with arrow, or blank if at start
    lcd.print(F("< "));
    lcd.print(charSets[currentSet].chars[letter_index - 1]);
  } else {
    lcd.print(F("   "));
  }

  lcd.print(F(" "));                                      // col 7: gap
  lcd.print(charSets[currentSet].chars[letter_index]);    // col 8: selected char

  if (letter_index < setSize - 1) {                      // cols 9-15: next char with arrow, or blank if at end
    lcd.print(F(" "));
    lcd.print(charSets[currentSet].chars[letter_index + 1]);
    lcd.print(F(" >   "));
  } else {
    lcd.print(F("       "));
  }
}

//2 DISPLAY FUNCTIONS... ONE FOR THE MESSAGE COMPOSE WRITING ONE FOR SCROLLING AND EDITING THE MESSAGE
//possible to combine ltr
void tx_display_message(int cursorPos, bool editMode, LiquidCrystal_I2C &lcd) {
  int messageLength = get_message_length();

  if (editMode) {  // delete mode tool tip
    lcd.setCursor(0, 0);
    lcd.print(F("Edit Msg:Sel=Del"));
  }

  cursorPos = constrain(cursorPos, 0, messageLength);
  int windowStart = max(0, cursorPos - (editMode ? 14 : 15));  // scroll window; edit mode reserves col 15 for context

  Node *p = get_node_at(windowStart);    // walk list from window start, one char per column
  lcd.setCursor(0, 1);
  for (int col = 0; col < 16; col++) {
    int i = windowStart + col;
    char c = (i < messageLength) ? p->data : ' ';
    if (i < messageLength) p = p->next;
    lcd.print(i == cursorPos && (millis() / 500) % 2 == 0 ? '_' : c);  // blink underscore at cursor
  }
}

void rx_display_message(char *msg, int cursorPos, int msgIndex, LiquidCrystal_I2C &lcd){
  // Row 0: Message Nunmber
  lcd.setCursor(0, 0);
  lcd.print(F("Msg "));
  lcd.print(msgIndex + 1);
  lcd.print(F("           "));

  // Row 1: message 
  int len = strlen(msg);
  lcd.setCursor(0, 1);
  int col = 0;
  for (int i = cursorPos; i < len && col < 16; i++, col++)
    lcd.print(msg[i]);
  while (col < 16) { lcd.print(' '); col++; }
}

void progress_bar(int packet_index, int totalPackets, LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("Sending "));
  lcd.print(packet_index + 1);
  lcd.print("/");
  lcd.print(totalPackets);

  int percent = ((packet_index + 1) * 100) / totalPackets;
  int barLength = (packet_index + 1) * 13 / totalPackets;

  lcd.setCursor(0, 1);
  for (int i = 0; i < 13; i++) {
    if(i < barLength) lcd.write(0xFF);
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
  lcd.print(F(" Pkt: "));
  lcd.print(packet_index + 1);
  lcd.print("/");
  lcd.print(F("?"));
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

void ui_no_messages(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("   No Messages  "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
}

