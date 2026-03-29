#include "ui.h"
#include "message_comp.h"
#include <LiquidCrystal_I2C.h>

const int DEBOUNCE_MS = 150;
const int LONG_PRESS_MS = 600;

unsigned long lastSelect = 0;
bool selectHeld = false;
bool longPressFired = false;

volatile int encoderPos = 0;
volatile int lastTick = HIGH;

bool editMode = false;
int  insertPos = 0;
int  updateCursor = 0;

// Input functions start here

// returns true if pin is LOW and debounce has elapsed
bool buttonPressed(int pin, unsigned long &lastPress) {
  if (digitalRead(pin) || millis() - lastPress <= DEBOUNCE_MS) return false;
  lastPress = millis();
  return true;
}

// updates encoderPos on each encoder tick
void readEncoder(int pinA, int pinB) {
  int aState = digitalRead(pinA);
  if(aState != lastTick && aState == LOW){
    digitalRead(pinB) == HIGH ? encoderPos++ : encoderPos--;
  }
  lastTick = aState;
}

// returns press type for the select button
SelectAction handleSelect(int pin, LiquidCrystal_I2C &lcd) {
  bool pressed = (digitalRead(pin) == LOW);
  SelectAction state = SELECT_NONE;

  // Press starts recording the time
  if (pressed && !selectHeld) {
    selectHeld = true;
    longPressFired = false;
    lastSelect = millis();
  }

  // Held so its a long press
  else if (pressed && !longPressFired && millis() - lastSelect > LONG_PRESS_MS) {
    longPressFired = true;
    state = LONG_SELECT;
  }

  // Released so its a short press
  else if (!pressed && selectHeld) {
    if (!longPressFired && millis() - lastSelect > DEBOUNCE_MS) {
      state = SHORT_SELECT;
    }
    selectHeld = false;
    longPressFired = false;
  }
  return state;
}

// Updating display functions start here

// shows character picker
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

// renders message and cursor in compose mode
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

// shows messages in inbox
void rx_display_message(char *msg, int cursorPos, int msgIndex, LiquidCrystal_I2C &lcd){

  lcd.setCursor(0, 0);
  lcd.print(F("Msg "));
  lcd.print(msgIndex + 1);
  lcd.print(F("           "));

  int len = strlen(msg);
  lcd.setCursor(0, 1);
  int col = 0;
  for (int i = cursorPos; i < len && col < 16; i++, col++)
    lcd.print(msg[i]);
  while (col < 16) { lcd.print(' '); col++; }
}

// shows packet send progress with a bar
void progress_bar(int packet_index, int totalPackets, LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("Sending "));
  lcd.print(packet_index + 1);
  lcd.print("/");
  lcd.print(totalPackets);

  int percent   = (packet_index + 1) * 100 / totalPackets;
  int barLength = (packet_index + 1) * 13  / totalPackets;

  lcd.setCursor(0, 1);
  for (int i = 0; i < 13; i++)
    lcd.write(i < barLength ? 0xFF : ' ');

  lcd.setCursor(13, 1);
  if (percent < 100) lcd.print(' ');
  if (percent < 10)  lcd.print(' ');
  lcd.print(percent);
}

// shows incoming packet count during receive
void rx_pkt_count(int packet_index, LiquidCrystal_I2C &lcd){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F(" Receiving Msg! "));
  lcd.setCursor(0, 1);
  lcd.print(F(" Pkt: "));
  lcd.print(packet_index + 1);
  lcd.print("/");
  lcd.print(F("?"));
}

// Status screen functions start here

// shows idle listening screen
void rx_listen(LiquidCrystal_I2C &lcd){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  Listening...  "));
}

// shows listening screen with unread message count
void pending_msgs(int msgCount, LiquidCrystal_I2C &lcd){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  Listening...  "));
  lcd.setCursor(0, 1);
  lcd.print(msgCount);
  lcd.print(F(" msg(s) waiting "));
}

// shows empty inbox screen
void no_messages(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("   No Messages  "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
}

// shows out of memory warning
void memory_full(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("  No Memory :(  "));
}

// Error screen fucntions start here

//The mythical yamen function
void decode_failure(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("Decoding Faliure"));
}

// shows receive error
void rx_failure(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("   Rx Faliure   "));
}

// shows message rebuild error
void rebuild_failure(LiquidCrystal_I2C &lcd){
  lcd.setCursor(0, 0);
  lcd.print(F("Rebuild Failed  "));
  delay(500);
}
