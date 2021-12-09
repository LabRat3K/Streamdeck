
/* ---------------------------------------------------------------
 * streamdeck.ino 
 * 
 * Based on the work by Tekks to create a TFT Touchscreen Control Deck
 * 
 * For use on the ATMEGA32U4 as it will provide Keyboard HID class
 *
 * Updated by LabRat in 2021
 *
 * Note: Attempts to use graphics read from the SD  on the LCD have 
 * been abandoned, as this takes up too much FLASH space, and won't
 * fit on the Leonardo. Perhaps revisit this in the future if we can 
 * shrink the GFX library.
 *
 *
 * To Do:
 *    EEPROM for BUTTON array:
 *       - move button array to be stored/read in/from EEPROM
 *       - provide ability to receive a new array/config and flash to EEPROM
 *       - (alternatively could read from SD card)
 *    CTRL/ALT Keypress combinations?
 *       - Could use a bit mask to denote if any KEY modifiers should be used
 *
 * EEPROM map
 *   0x00-0x01: 16 bit CRC of the BUTTONS array
 *   0x02.. (2+sizeof(BUTTONS)): alternate button configuration
 *   
 */

// Update these to reflect your display/touchscreen
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

#include <TouchScreen.h>
#include <EEPROM.h>

// Uncomment to enable serial console debugging
// Note: Leonardo will pause until a serial connection is established
//
#define DEBUG

// Uncomment to force use of ROM based values
//#define CORRUPT_EEPROM

// Uncomment in order to print x,y,z for all "touch" events detected
//#define DEBUG_TOUCH

// Uncomment if you aren't using a button mask to delineate the virtual buttons
//#define OUTLINE_BUTTONS

#if defined(ARDUINO_AVR_LEONARDO)
  #define HID_OUTPUT
  #include <Keyboard.h>
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~
// TOUCH SCREEN CALIBRATION
// You should run the calibration routine and update the MIN/MAX for X/Y
//
//Param For my 3.5" TFT display 
#define TS_MINY 176
#define TS_MAXY 906

#define TS_MINX 185
#define TS_MAXX 955

// TOUCH SCREEN PIN MAPPING
// These are the pins for the shield!
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A1  // must be an analog pin, use "An" notation!
#define YM 6   // can be a digital pin
#define XP 7   // can be a digital pin

#define MINPRESSURE 50
#define MAXPRESSURE 1000

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 338);

// ~~~~~~~~~~~~~~~~~
// TFT CONFIGURATION
// These pins should correspond with your TFT display wiring
// The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
//
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0

// Mapping Color index to 5:6:5 Color codes (RGB)
#define BLACK   (0)
#define BLUE    (1)
#define RED     (2)
#define GREEN   (3)
#define CYAN    (4)
#define YELLOW  (5)
#define WHITE   (6)
#define ORANGE  (7)
const uint16_t PROGMEM COLOR_MAP[8] = { 0x0000, 0x001F, 0xF800, 0x07E0, 0x07FF, 0xFFE0, 0xFFFF, 0xFD00 };
// Design decision to limit to 8 so that we can store foreground and background in a single byte

MCUFRIEND_kbv tft;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// VIRTUAL BUTTON CONFIGURATION
// Button State Information
// Boolean flags to denote the ON/OFF state for modal switches
// Some simple #define macros to set, clear, query, and toggle the bits
// NOTE: This assumes that the number of buttons is 16 or less. Change
// to uint32_t if you need more or create an array of maps based 
// on "PAGE" displayed
uint16_t press_state = 0x00;

#define set_state(x)    (press_state |=  (1<<x))
#define clr_state(x)    (press_state &= ~(1<<x))
#define qry_state(x)    ((press_state & (1<<x))>0)
#define toggle_state(x) (press_state ^=  (1<<x))

// Define the Button Layout & Size and store in FLASH (save the RAM)
#define BWIDTH 90
#define BHEIGHT 90
#define XGAP 6
#define YGAP 10


// Note: the use of an extra offset to adjust the buttons to match the enclosure
#define COLUMN_1 (XGAP/2)-5
#define COLUMN_2 (COLUMN_1+BWIDTH+XGAP)
#define COLUMN_3 (COLUMN_2+BWIDTH+XGAP)
#define COLUMN_4 (COLUMN_3+BWIDTH+XGAP)
#define COLUMN_5 (COLUMN_4+BWIDTH+XGAP)

//Arbitrary offset/boundary - 15 top/bottom
#define ROW_1 (15)
#define ROW_2 (ROW_1+BHEIGHT+YGAP)
#define ROW_3 (ROW_2+BHEIGHT+YGAP)

const uint16_t PROGMEM COLUMN_MAP[5] = { COLUMN_1, COLUMN_2, COLUMN_3, COLUMN_4, COLUMN_5 };
const uint16_t PROGMEM ROW_MAP[3]    = { ROW_1, ROW_2, ROW_3 };

#define COLUMN(x) (x%(sizeof(COLUMN_MAP)/sizeof(uint16_t)))
#define ROW(x)    (x/(sizeof(COLUMN_MAP)/sizeof(uint16_t)))

// Data Structure to contain all BUTTON information (location, strings, colors, keycodes)
#define BT_NONE      (0x00)
#define BT_MOMENTARY (0x01)
#define BT_LATCHING  (0x02)
#define BT_LINKSET   (0x03)
#define BT_LINKCLR   (0x04)

struct tButton {
   uint8_t btype;          // Button Type - Momentary, Latching, Linked
   char    bStrings[4][8]; // 3 lines plus alternate third line when ENABLED
   uint8_t color;
   uint8_t crtlKeys;
   uint8_t keyCode;
   uint8_t link;           // If LINKED.. identiy of the LINKED button
};

// Macros to decode the foreground and background colors
#define fgColor(x) (x&0x0F)
#define bgColor(x) (x>>4)

#define CRTL_NONE (0x00)

#define LEFT_CTRL   (0x01)
#define LEFT_SHIFT  (0x02)
#define LEFT_ALT    (0x04)
#define LEFT_GUI    (0x08)
#define RIGHT_CTRL  (0x10)
#define RIGHT_SHIFT (0x20)
#define RIGHT_ALT   (0x40)
#define RIGHT_GUI   (0x80)

tButton BUTTONS[15] =  {
   { BT_LINKCLR,   { "DISC", "Mic",   "ON",     "OFF" },   (BLACK<<4 | GREEN), CRTL_NONE, KEY_F13, 0x05 },
   { BT_LATCHING,  { "TS",   "Mic",   "ON",     "OFF" },   (BLACK<<4 | GREEN), CRTL_NONE, KEY_F15, 0xFF },
   { BT_MOMENTARY, { "OBS",  "Scene", "Idle",   "Idle"},   (BLACK<<4 | ORANGE),CRTL_NONE, KEY_F17, 0xFF },
   { BT_MOMENTARY, { "OBS",  "Timer", "ON/OFF", "ON/OFF"}, (BLACK<<4 | ORANGE),CRTL_NONE, KEY_F18, 0xFF },
   { BT_MOMENTARY, { "OBS",  "Scene", "Active", "Active"}, (BLACK<<4 | ORANGE),CRTL_NONE, KEY_F19, 0xFF },

   { BT_LINKSET,   { "DISC", "Speaker", "ON",   "OFF"},    (BLACK<<4 | GREEN), CRTL_NONE, KEY_F14, 0x00 },
   { BT_LATCHING,  { "TS",   "Speaker", "ON",   "OFF"},    (BLACK<<4 | GREEN), CRTL_NONE, KEY_F16, 0xFF },
   { BT_LINKCLR,   { "TEST", "LINK",    "",     "ACTIVE"}, (BLACK<<4 | YELLOW),CRTL_NONE, KEY_F10, 0x09 },
   { BT_MOMENTARY, { "",     "",        "BLACK","YELLOW"}, (WHITE<<4 | BLUE),  CRTL_NONE, KEY_F11, 0xFF },
   { BT_LINKSET,   { "SOME", "TXT",     "",     "ENABLE"}, (BLACK<<4 | YELLOW),CRTL_NONE, KEY_F12, 0x07 },

   {BT_LATCHING,   { "OBS",  "Mic",     "ON",   "OFF"},    (BLACK<<4 | CYAN),  CRTL_NONE, KEY_F23, 0xFF },
   {BT_LATCHING,   { "OBS",  "Speaker", "ON",   "OFF"},    (BLACK<<4 | CYAN),  CRTL_NONE, KEY_F24, 0xFF },
   {BT_MOMENTARY,  { "OBS",  "Scene",   "OW",   "OW"},     (BLACK<<4 | ORANGE),CRTL_NONE, KEY_F20, 0xFF },
   {BT_MOMENTARY,  { "OBS",  "Scene",   "PUBG", "PUBG"},   (BLACK<<4 | ORANGE),CRTL_NONE, KEY_F21, 0xFF },
   {BT_MOMENTARY,  { "OBS",  "Scene",   "SoW",  "SoW"},    (BLACK<<4 | ORANGE),KEY_F22, 0xFF }
};


// Forward Declaration
void writeButtons(void);

// ~~~~~~~~~~~~~~~~~~~
// MAIN CODE BLOCK 
//
void setup(void) {
  unsigned long time=millis();

  Serial.begin(115200);
  while ((!Serial) && (millis()<time+5000)) { ; } // Wait for Serial setup to complete
  Serial.println(F("Stream Deck"));

  // Setup the LCD (Hard-Coded value vs probing the hardware)
  tft.reset();
  tft.begin(0x9486); 
  tft.fillScreen(BLACK);

  tft.setRotation(3);
  tft.setTextSize(2); // Same font size everywhere, so only do this once

 // Debugging
 // writeButtons();

#ifdef CORRUPT_EEPROM
  writeCRC(0xDEADC0DE);
  Serial.println("Data Corrupted");
#endif

  // If EEPROM CRC checks out.. read the updated profile into RAM
  unsigned long tempCRC = readCRC();

  if (tempCRC != eeprom_crc()) {
     // CRC failed - log the error but then use the integrated DEFAULTS
  	Serial.print(F("CRC Error:0x"));
  	Serial.print(tempCRC, HEX);
        Serial.print("  vs 0x");
        Serial.println(eeprom_crc(),HEX);
  } else {
     // CRC is good : copy the block into the RAM array
     int i;
     uint8_t *temp = (uint8_t*) &(BUTTONS[0]);
     for (i=sizeof(unsigned long); i<sizeof(unsigned long)+sizeof(BUTTONS);i++){
        *temp = EEPROM[i];
        temp++; 
     }
  }
  // Render default buttons
  draw_screen(); 
  
  #if defined(HID_OUTPUT)
  	Keyboard.begin();
  #endif
}

// -----------------------------------------------------------
// Serial Input - parse string
// Q,bid
// W,bid,btype,str1,str2,str3,str4,color,crtlKeys,keycode,link
// -----------------------------------------------------------
tButton temp;
uint8_t bid = 0xFF;

uint8_t hex2dec(char * hex) {
   uint8_t retval = 0x00;
   uint8_t i;

    for(i = 0; i < 2; i++) {
        if(hex[i] >= '0' && hex[i] <= '9')
        {
            retval = retval << 4;
            retval += hex[i] - '0';
        }
        else if(hex[i] >= 'A' && hex[i] <= 'F')
        {
            retval = retval << 4;
            retval += 10+hex[i] - 'A';
        }
        else if(hex[i] >= 'a' && hex[i] <= 'f')
        {
            retval = retval << 4;
            retval += 10+hex[i] - 'a';
        }
    }
    return retval;
}

void dump_data(uint8_t bid, tButton * button) {
    Serial.print(bid,HEX);
    Serial.print(",");
    Serial.print(button->btype,HEX);
    Serial.print(",");
    Serial.print(button->bStrings[0]);
    Serial.print(",");
    Serial.print(button->bStrings[1]);
    Serial.print(",");
    Serial.print(button->bStrings[2]);
    Serial.print(",");
    Serial.print(button->bStrings[3]);
    Serial.print(",");
    Serial.print(button->color,HEX);
    Serial.print(",");
    Serial.print(button->crtlKeys,HEX);
    Serial.print(",");
    Serial.print(button->keyCode,HEX);
    Serial.print(",");
    Serial.println(button->link,HEX);
}

int write_str(char * input) {
           char *token;
           int j=0;

     // Parse the ButtonId
     token = strtok(input,",");
     if (token == NULL) return -1;
     bid = hex2dec(token);
     if (bid>14) return -1;

     token = strtok(NULL,",");
     if (token == NULL) return -2;
     temp.btype = hex2dec(token);
     if (temp.btype > 4) return -2;

     for (j=0;j<4;j++) {
        token = strtok(NULL,",");
        if (token == NULL) return -(3+j);
        strncpy(temp.bStrings[j],token,8);
     }

     token = strtok(NULL,",");
     if (token == NULL) return -7;
     temp.color = hex2dec(token);

     token = strtok(NULL,",");
     if (token == NULL) return -8;
     temp.crtlKeys = hex2dec(token);

     token = strtok(NULL,",");
     if (token == NULL) return -9;
     temp.keyCode = hex2dec(token);

     token = strtok(NULL,",");
     if (token == NULL) return -10;
     temp.link = hex2dec(token);
     if (temp.link > 14) return -109;
     if (temp.link == bid) return -109; // Can't link to yourself

     // Data is good - write it to the EEPROM
     memcpy(&(BUTTONS[bid]),&temp,sizeof(tButton));
     return 0;
}

void command_parse() {
   char parseBuffer[45];
   int  retCode = 0x00;

   Serial.readString().toCharArray(parseBuffer, 42);

   if (parseBuffer[0] == 'W') {
      retCode =  write_str(&(parseBuffer[2]));

      if (retCode < 0) {
         Serial.print("Error parsing string: ");
         Serial.println(retCode);
         return;
      }
   }

   if (parseBuffer[0] == 'Q') {
     char *token;

     // Parse the ButtonId
     token = strtok(&(parseBuffer[2]),",");
     if (token == NULL) {
         Serial.println("Invalid Button id");
         return;
     }
     bid = hex2dec(token);
     if (bid<15) {
        // Query - read and print
        dump_data(bid, &(BUTTONS[bid]));
	return;
     }
   }

  if (parseBuffer[0] == 'F') {
     parseBuffer[7] = 0;
     if (!strcmp(&parseBuffer[1],"LabRat")) {
        writeButtons();
	return;
     }
  }
  if (parseBuffer[0] == 'C') {
     parseBuffer[7] = 0;
     if (!strcmp(&parseBuffer[1],"taRbaL")) {
        writeCRC(0xDEADC0DE);
	return;
     }
  }
  Serial.println(F("Q,<bid> - print config for button."));
  Serial.println(F("W,<bid>,<btype>,<str1>.<str2>,<str3>,<str4>,<color>,<crtlKeys>,<keycode>,<link>"));
  Serial.println(F("  <bid>   - button id: 0 .. E"));
  Serial.println(F("  <btype> - button type - 0:none, 1:momentary, 2:latching, 3:link_set, 4:link_clear"));
  Serial.println(F("  <str1>..<str4> - strings 1-3: Text on Buttons (7 char max) String 4: 3rd line alternate"));
  Serial.println(F("  <color> - <bg>|<fg> (upper/lower nibbles)"));
  Serial.println(F("     0: Black, 1: Blue, 2: Red, 3: Green, 4: Cyan, 5: Yellow, 6: White, 7: Orange"));
  Serial.println(F("  <crtlKeys> - <right>|<left> (upper/lowernibbles) key modifiers "));
  Serial.println(F("     1:CTRL, 2: SHIFT, 4: ALT, 8: GUI"));
  Serial.println(F("  <keycode> - hex keypress"));
  Serial.println(F("  <link> - Id of the linked button"));
  Serial.println(F("FLabRat  - Write config into the flash (eeprom)"));
  Serial.println(F("CtaRbaL  - Invalidate CSUM in the flash"));
}

void loop() {
  TSPoint p = ts.getPoint();
  if (Serial.available()>0) {
      command_parse();
  }

  // Sharing pins: fix the directions of the touchscreen pins
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
   
    p.x = map(p.x, TS_MAXX, TS_MINX, 0, 480);
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

   #ifdef DEBUG_TOUCH
   	Serial.print(p.x);
   	Serial.print(",");
   	Serial.print(p.y);
   	Serial.print(",");
   	Serial.println(p.z);
   #endif

    //################## Code for actions here ##################

    // Determine if touch point is in a row
    // Note: Optimized to a cascade - no need to test after positive 
    uint8_t button_id = 0x00;

    if (p.y >  30 && p.y < 120) {
        button_id = 0x00;
    } else {
       if (p.y > 135 && p.y < 210) {
           button_id = 0x05; // Row index * Column count
       } else {
          if (p.y > 230 && p.y < 310) {
              button_id = 0x0A; // Row index * Column count
          } else {
            // Not in a row - return (not found)
            // Is a delay needed here?
            return;
          }
       }
    }

    // Determine if touch point is in a column
    if (p.x >  3 && p.x <  96) { 
       button_id+=0; // It's a NOP to fill this branch
    } else {
       if (p.x > 99 && p.x < 189) {
          button_id += 1; 
       } else {
          if (p.x > 195 && p.x < 285) {
              button_id+= 2;
          } else { 
             if (p.x > 291 && p.x < 381) {
                 button_id+= 3;
             } else {
                if (p.x > 387 && p.x < 477) {
                    button_id+= 4;
                } else {
                  // Not in a column - return (not found)
                  // Is a delay needed here?
                  return;
                }
             }
          }
       }
    }

    // button_id now contains a value in range of 0x00 - 0x0E
    #ifdef DEBUG_TOUCH
    	Serial.print(F("ButtonId:"));
    	Serial.println(button_id,HEX);
    #endif


    // For anything other than button type NONE.. we send the keyCode
    #if defined(HID_OUTPUT)
    if (BUTTONS[button_id].btype != BT_NONE) {
	uint8_t tempKey = BUTTONS[button_id].crtlKeys;
	if (tempKey) {
           if (tempKey & LEFT_CTRL)   Keyboard.press(KEY_LEFT_CTRL);
           if (tempKey & LEFT_SHIFT)  Keyboard.press(KEY_LEFT_SHIFT);
           if (tempKey & LEFT_ALT)    Keyboard.press(KEY_LEFT_ALT);
           if (tempKey & LEFT_GUI)    Keyboard.press(KEY_LEFT_GUI);
           if (tempKey & RIGHT_CTRL)  Keyboard.press(KEY_LEFT_CTRL);
           if (tempKey & RIGHT_SHIFT) Keyboard.press(KEY_LEFT_SHIFT);
           if (tempKey & RIGHT_ALT)   Keyboard.press(KEY_LEFT_ALT);
           if (tempKey & RIGHT_GUI)   Keyboard.press(KEY_LEFT_GUI);
        }
	Keyboard.write(BUTTONS[button_id].keyCode);
	Keyboard.releaseAll();
    }
    #endif

    // Update the state and re-draw the buttons
    switch (BUTTONS[button_id].btype) {
       case BT_MOMENTARY:
                // Draw with inverted coloring
		draw_re(button_id, bgColor(BUTTONS[button_id].color),fgColor(BUTTONS[button_id].color));
		delay(250);
                // Restore to original Format
		draw_re(button_id);
                break;
      case BT_LATCHING:
		toggle_state(button_id);
		if (qry_state(button_id)) {
			draw_re(button_id, WHITE, RED);
		} else {
			draw_re(button_id);
		}
		break;
       case BT_LINKCLR:
		toggle_state(button_id);
		if (qry_state(button_id)) {  // MUTE request
			draw_re(button_id, WHITE, RED);
		} else {
			draw_re(button_id);

                        if (qry_state(BUTTONS[button_id].link)) {
			   clr_state(BUTTONS[button_id].link); // Enable DISCHORD speaker
			   draw_re(BUTTONS[button_id].link);
                        }
		}
		break;
       case BT_LINKSET:
		toggle_state(button_id);
		if (qry_state(button_id)) { // Mute Speaker request
			draw_re(button_id, WHITE, RED);

                        if (!(qry_state(BUTTONS[button_id].link))) {
			    // Update the MIC as well - change to MUTED
			    draw_re(BUTTONS[button_id].link, WHITE, RED);
			    set_state(BUTTONS[button_id].link);
                        }
		} else {
			draw_re(button_id);
		}
		break;
       case BT_NONE:
       default:  {
               /* Do Nothing */
       }
    }
    delay(500);
    Serial.print(".");
  }
}

void draw_screen() {
  uint8_t i = 0;

  // Render each button - default state
  for (i=0;i<15;i++) {  
     draw_re(i);
  }
}


void draw_re(uint8_t bid) {
  #ifdef OUTLINE_BUTTONS
        // Empty Button(s)
  	tft.drawRoundRect(pgm_read_word(&(COLUMN_MAP[COLUMN(bid)])),
		pgm_read_word(&(ROW_MAP[ROW(bid)])), 
		BWIDTH, BHEIGHT, min(BWIDTH,BHEIGHT)/4, pgm_read_word(&(COLOR_MAP[WHITE])));
  #endif

  draw_re(bid, fgColor(BUTTONS[bid].color), BUTTONS[bid].bStrings[0], BUTTONS[bid].bStrings[1],BUTTONS[bid].bStrings[2], bgColor(BUTTONS[bid].color));
}

// If only BID + 2 colors .. then render ALTERNATE text as 3rd line
void draw_re(uint8_t bid, uint8_t color, uint8_t bgcolor) {
   // Button(s) - pressed state - Strings[0-1,3]
   draw_re(bid, color, BUTTONS[bid].bStrings[0], BUTTONS[bid].bStrings[1],BUTTONS[bid].bStrings[3],bgcolor);
}

void draw_re(uint8_t bid, uint8_t color, String txt1, String txt2, String txt3, uint8_t bgcolor) {
  int x = pgm_read_word(&(COLUMN_MAP[COLUMN(bid)]));
  int y = pgm_read_word(&(ROW_MAP[ROW(bid)]));
  int txtdst = 5;

  #ifdef OUTLINE_BUTTONS
  	tft.drawRoundRect(x, y, BWIDTH, BHEIGHT,min(BWIDTH,BHEIGHT)/4, pgm_read_word(&(COLOR_MAP[WHITE])));
  #endif
  tft.fillRoundRect(x + 1, y + 1, BWIDTH-2, BHEIGHT-2,min(BWIDTH-2, BHEIGHT-2)/4, pgm_read_word(&(COLOR_MAP[bgcolor])));
  
  tft.setTextColor(pgm_read_word(&(COLOR_MAP[color])));

  // Attempt to center the text on the BUTTON
  tft.setCursor(x + 47-txt1.length()*6, y + txtdst+10);
  tft.print(txt1);
  tft.setCursor(x + 47-txt2.length()*6, y + txtdst + 30);
  tft.print(txt2);
  tft.setCursor(x + 47-txt3.length()*6, y + txtdst + 50);
  tft.print(txt3);
}

void writeCRC(unsigned long crc) {
   int i ;
   for (i=sizeof(unsigned long);i>0; i--) {
      EEPROM[i-1] = crc&0xFF;
      crc = crc>>8; 
   }
}

void writeButtons(void) {
    int i;
    uint8_t *temp = (uint8_t*) &(BUTTONS[0]);

     for (i=sizeof(unsigned long); i<sizeof(unsigned long)+sizeof(BUTTONS);i++){
        EEPROM.write(i,*temp);
        temp++; 
     }

     // Now update the CRC for this block of data
     writeCRC(eeprom_crc());
}

unsigned long readCRC(void) {
   unsigned long temp = 0x00;
   int i ;
   for (i=0;i<sizeof(unsigned long); i++) {
      temp = (temp << 8) + EEPROM[i];
   }
   return (temp);
}

// CRC routines to validate incoming data and/or EEPROM
unsigned long eeprom_crc(void) {
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = sizeof(unsigned long) ; index < sizeof(unsigned long)+sizeof(BUTTONS) ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }

  return crc;
}
