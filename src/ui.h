#ifndef UI_H
#define UI_H

#include <LiquidCrystal_I2C.h>

extern volatile int encoderPos;
extern bool editMode;
extern int  insertPos;
extern int  updateCursor;

enum SelectAction { SELECT_NONE, SHORT_SELECT, LONG_SELECT };

// Input functions
bool buttonPressed(int pin, unsigned long &lastPress);
void readEncoder(int pinA, int pinB);
SelectAction handleSelect(int pin, LiquidCrystal_I2C &lcd);

// Updating display functions
void letter_scroll(int letter_index, LiquidCrystal_I2C &lcd);
void tx_display_message(int cursorPos, bool editMode, LiquidCrystal_I2C &lcd);
void rx_display_message(char *msg, int cursorPos, int msgIndex, LiquidCrystal_I2C &lcd);
void progress_bar(int packet_index, int totalPackets, LiquidCrystal_I2C &lcd);
void rx_pkt_count(int packet_index, LiquidCrystal_I2C &lcd);

// Status screens
void rx_listen(LiquidCrystal_I2C &lcd);
void pending_msgs(int msgCount, LiquidCrystal_I2C &lcd);
void no_messages(LiquidCrystal_I2C &lcd);
void memory_full(LiquidCrystal_I2C &lcd);

// Error screens
void decode_failure(LiquidCrystal_I2C &lcd);
void rx_failure(LiquidCrystal_I2C &lcd);
void rebuild_failure(LiquidCrystal_I2C &lcd);

#endif
