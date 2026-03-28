#ifndef UI_H
#define UI_H

#include <LiquidCrystal_I2C.h>
#include "message_comp.h"

extern volatile int encoderPos;
extern bool editMode;
extern int  insertPos;
extern int  updateCursor;

#define SELECT_NONE  0
#define SHORT_SELECT 1
#define LONG_SELECT  2

//Hardware interfaces
bool buttonPressed(int pin, unsigned long &lastPress);
int handleSelect(int pin, LiquidCrystal_I2C &lcd);
void readEncoder(int pinA, int pinB);

//Display Functions
void tx_display_message(int cursorPos, bool editMode, LiquidCrystal_I2C &lcd);
void rx_display_message(char *msg, int cursorPos, int msgIndex, LiquidCrystal_I2C &lcd);

void progress_bar(int packet_index, int totalPackets, LiquidCrystal_I2C &lcd);
void rx_count(int packet_index, LiquidCrystal_I2C &lcd);

void memory_full(LiquidCrystal_I2C &lcd);
void decode_failure(LiquidCrystal_I2C &lcd);
void ui_listen(LiquidCrystal_I2C &lcd);
void ui_msg_received(int msgCount, LiquidCrystal_I2C &lcd);
void ui_no_messages(LiquidCrystal_I2C &lcd);
void letter_scroll(int letter_index, LiquidCrystal_I2C &lcd);

int free_ram();
#endif