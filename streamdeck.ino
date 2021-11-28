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
 */

// Update these to reflect your display/touchscreen
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

#include <TouchScreen.h>

// Uncomment to enable serial console debugging
// Note: Leonardo will pause until a serial connection is established
//
//#define DEBUG


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
   uint8_t keyCode;
   uint8_t link;           // If LINKED.. identiy of the LINKED button
};

// Macros to decode the foreground and background colors
#define fgColor(x) (x&0x0F)
#define bgColor(x) (x>>4)

const tButton BUTTONS[15] =  {
   { BT_LINKCLR,   { "DISC", "Mic",   "ON",     "OFF" },   (BLACK<<4 | GREEN), KEY_F13, 0x05 },
   { BT_LATCHING,  { "TS",   "Mic",   "ON",     "OFF" },   (BLACK<<4 | GREEN), KEY_F15, 0xFF },
   { BT_MOMENTARY, { "OBS",  "Scene", "Idle",   "Idle"},   (BLACK<<4 | ORANGE),KEY_F17, 0xFF },
   { BT_MOMENTARY, { "OBS",  "Timer", "ON/OFF", "ON/OFF"}, (BLACK<<4 | ORANGE),KEY_F18, 0xFF },
   { BT_MOMENTARY, { "OBS",  "Scene", "Active", "Active"}, (BLACK<<4 | ORANGE),KEY_F19, 0xFF },

   { BT_LINKSET,   { "DISC", "Speaker", "ON",   "OFF"},    (BLACK<<4 | GREEN), KEY_F14, 0x00 },
   { BT_LATCHING,  { "TS",   "Speaker", "ON",   "OFF"},    (BLACK<<4 | GREEN), KEY_F16, 0xFF },
   { BT_LINKCLR,   { "TEST", "LINK",    "",     "ACTIVE"}, (BLACK<<4 | YELLOW),KEY_F10, 0x09 },
   { BT_MOMENTARY, { "",     "",        "BLACK","YELLOW"}, (BLACK<<4 | YELLOW),KEY_F11, 0xFF },
   { BT_LINKSET,   { "SOME", "TXT",     "",     "ENABLE"}, (BLACK<<4 | YELLOW),KEY_F12, 0x07 },

   {BT_LATCHING,   { "OBS",  "Mic",     "ON",   "OFF"},    (BLACK<<4 | CYAN),  KEY_F23, 0xFF },
   {BT_LATCHING,   { "OBS",  "Speaker", "ON",   "OFF"},    (BLACK<<4 | CYAN),  KEY_F24, 0xFF },
   {BT_MOMENTARY,  { "OBS",  "Scene",   "OW",   "OW"},     (BLACK<<4 | ORANGE),KEY_F20, 0xFF },
   {BT_MOMENTARY,  { "OBS",  "Scene",   "PUBG", "PUBG"},   (BLACK<<4 | ORANGE),KEY_F21, 0xFF },
   {BT_MOMENTARY,  { "OBS",  "Scene",   "SoW",  "SoW"},    (BLACK<<4 | ORANGE),KEY_F22, 0xFF }
};


// ~~~~~~~~~~~~~~~~~~~
// MAIN CODE BLOCK 
//
void setup(void) {
  #ifdef DEBUG
  Serial.begin(115200);
  while (!Serial) { ; } // Wait for Serial setup to complete 
  Serial.println(F("Stream Deck"));
  #endif

  tft.reset();
  tft.begin(0x9486); // Hard-coded versus probing the hardware
  tft.fillScreen(BLACK);

  tft.setRotation(3);
  tft.setTextSize(2); // Same font size everywhere, so only do this once

  draw_screen(); // Render default buttons
  
  #if defined(HID_OUTPUT)
  Keyboard.begin();
  #endif
}

void loop() {
  TSPoint p = ts.getPoint();

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
	Keyboard.write(BUTTONS[button_id].keyCode);
    }
    #endif

    // Update the state and re-draw the buttons
    switch (BUTTONS[button_id].btype) {
       case BT_MOMENTARY:
		draw_re(button_id, bgColor(BUTTONS[button_id].color),fgColor(BUTTONS[button_id].color));
		delay(250);
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
  }
}

void draw_screen() {
  uint8_t i = 0;

  for (i=0;i<15;i++) {  
     draw_re(i);
  }
}


void draw_re(uint8_t bid) {
  // Empty Button(s)
  #ifdef OUTLINE_BUTTONS
  tft.drawRoundRect(pgm_read_word(&(COLUMN_MAP[COLUMN(bid)])),
           pgm_read_word(&(ROW_MAP[ROW(bid)])), 
           BWIDTH, BHEIGHT, min(BWIDTH,BHEIGHT)/4, pgm_read_word(&(COLOR_MAP[WHITE])));
  #endif

  draw_re(bid, fgColor(BUTTONS[bid].color), BUTTONS[bid].bStrings[0], BUTTONS[bid].bStrings[1],BUTTONS[bid].bStrings[2], bgColor(BUTTONS[bid].color));
}

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
  //tft.setCursor(x + txtdst, y + txtdst+10);
  tft.setCursor(x + 47-txt1.length()*6, y + txtdst+10);
  tft.print(txt1);
  tft.setCursor(x + 47-txt2.length()*6, y + txtdst + 30);
  tft.print(txt2);
  tft.setCursor(x + 47-txt3.length()*6, y + txtdst + 50);
  tft.print(txt3);
}
