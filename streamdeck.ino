/* ---------------------------------------------------------------
 * streamdeck.ino 
 * 
 * Based on the work by Tekks to create a TFT Touchscreen Control Deck
 * 
 * For use on the ATMEGA32U4 as it will provide Keyboard HID class
 *
 * Updated by LabRat in 2021
 *
 * Note: Attempts to use graphics read from the TFT on the LCD have 
 * been abandoned, as this takes up too much FLASH space, and won't
 * fit on the Leonardo. Perhaps revisit this in the future.
 */

// Update these to reflect your display/touchscreen
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

#include <TouchScreen.h>
#include <Keyboard.h>

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

// 5:6:5 Color codes (RGB)
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFD00

MCUFRIEND_kbv tft;

// ~~~~~~~~~~~~~~~~~~~~~~
// KEYBOARD CONFIGURATION
// Key event definitions
//
#define KEY_F13   0xF0 
#define KEY_F14   0xF1 
#define KEY_F15   0xF2 
#define KEY_F16   0xF3 
#define KEY_F17   0xF4 
#define KEY_F18   0xF5 
#define KEY_F19   0xF6 
#define KEY_F20   0xF7 
#define KEY_F21   0xF8 
#define KEY_F22   0xF9 
#define KEY_F23   0xFA
#define KEY_F24   0xFB

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

// Specific to DISCHORD buttons 
boolean auto_enable_mic;

// Define the Button Layout & Size and store in FLASH (save the RAM)
#define BWIDTH 90
#define BHEIGHT 90
#define XGAP 6
#define YGAP 10

#define COLUMN_1 (XGAP/2)
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


// ~~~~~~~~~~~~~~~~~~~
// MAIN CODE BLOCK 
//
void setup(void) {
  Serial.begin(115200);
  while (!Serial) { ; } /* Wait for Serial setup to complete */
  Serial.println(F("Stream Deck"));

  tft.reset();
  tft.begin(0x9486); // Hard-coded versus probing the hardware
  tft.fillScreen(BLACK);

  tft.setRotation(3);
  tft.setTextSize(2); // Same font size everywhere, so only do this once

  draw_screen(); // Render default buttons
  
  #if defined(ARDUINO_AVR_LEONARDO)
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
   Serial.println(p.y);
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

    switch (button_id) {
       //################## LINE 1 ##################
       case 0x00: // Dischord MICROPHONE 
              toggle_state(0x00);
              if (qry_state(0x00)) {  // MUTE request
            		draw_re(0x00, WHITE, "DISC", "Mic", "OFF",RED);
            
            		if (qry_state(0x05)) { // Is the DISCHORD speaker muted?
            		  auto_enable_mic = true; // Yes.. speaker is already muted
            		} else {
            		  auto_enable_mic = false;  
            		}
	            } else {
            		draw_re(0x00, GREEN, "DISC", "Mic", "ON");
            
            		clr_state(0x05); // Enable DISCHORD speaker
            		draw_re(0x05, GREEN, "DISC", "Speaker", "ON");
            
            		auto_enable_mic = false;
	            }
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F13);
              #endif
              break;
       case 0x01:
              toggle_state(0x01);
      	      if (qry_state(0x01)) {
      		      draw_re(0x01, WHITE, "TS", "Mic", "OFF",RED);
      	      } else {
      		      draw_re(0x01, GREEN, "TS", "Mic", "ON");;
      	      }
       
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F15);
              #endif
              break;
       case 0x02:
/*
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F17);
              #endif
              break;
*/
       case 0x03:
/*
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F18);
              #endif
              break;
*/
       case 0x04:
              #if defined(ARDUINO_AVR_LEONARDO) 
//	            Keyboard.write(KEY_F19);
	            Keyboard.write(KEY_F17+(button_id-0x02));
              #endif
              break;

       //################## LINE 2 ##################
       case 0x05:  // DISCHORD speaker state
              toggle_state(0x05);
      	      if (qry_state(0x05)) { // Mute Speaker request
            		draw_re(0x05, WHITE, "DISC", "Speaker", "OFF",RED);
      
                // Update the MIC as well - change to MUTED
      		      draw_re(0x00, WHITE, "DISC", "Mic", "OFF",RED);
		            set_state(0x00); 
	            } else {
		            draw_re(0x05, GREEN, "DISC", "Speaker", "ON");

                // check if we auto_enable_mic 
            		if (auto_enable_mic == true) { 
            		  draw_re(0x00, GREEN, "DISC", "Mic", "ON");
            		  clr_state(0x00); // Update DISCHORD MIC status
            		}
	            }
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F14);
              #endif
              break;
       case 0x06:
              toggle_state(0x06);
      	      if (qry_state(0x06)) {
            		draw_re(0x06, WHITE, "TS", "Speaker", "OFF",RED);
      	      } else {
            		draw_re(0x06, GREEN, "TS", "Speaker", "ON");
      	      }
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F16);
              #endif
	            break;
       case 0x07:
       case 0x08:
       case 0x09: // Currently these don't do anything
              break;
       //################## LINE 3 ##################
       case 0x0A:
              toggle_state(0x0A);
      	      if (qry_state(0x0A)) {
            		draw_re(0x0A, WHITE, "OBS", "Mic", "OFF",RED);
      	      } else {
            		draw_re(0x0A, CYAN, "OBS", "Mic", "ON");
      	      }
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F23);
              #endif
	            break;
       case 0x0B:
              toggle_state(0x0B);
      	      if (qry_state(0x0B)) {
            		draw_re(0x0B, WHITE, "OBS", "Speaker", "OFF",RED);
      	      } else {
            		draw_re(0x0B, CYAN, "OBS", "Speaker", "ON");
      	      }
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F24);
              #endif
	            break;
/* Code Saving Attempt */
       case 0x0C:
/*
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F20);
              #endif
              break;
*/
       case 0x0D:
/*
              #if defined(ARDUINO_AVR_LEONARDO) 
	            Keyboard.write(KEY_F21);
              #endif
              break;
*/
       case 0x0E:
              #if defined(ARDUINO_AVR_LEONARDO) 
	            //Keyboard.write(KEY_F22);
	            Keyboard.write(KEY_F20+(button_id-0x0C));
              #endif
              break;
       default: {
              // Error Scenario - this should not be possible
             }
    } // End SWITCH 
    delay(500);
  }
}

const char  BUTTONS[15][4][8] = {
  { "DISC", "Mic",   "ON",     "OFF" },
  { "TS",   "Mic",   "ON",     "OFF" },
  { "OBS",  "Scene", "Idle",   "Idle"},
  { "OBS",  "Timer", "ON/OFF", "ON/OFF"},
  { "OBS",  "Scene", "Active", "Active"},

  { "DISC", "Speaker", "ON",   "OFF"},
  { "TS", "Speaker", "ON",   "OFF"},
  { "","","",""},
  { "","","",""},
  { "","","",""},

  { "OBS", "Mic", "ON","OFF"},
  { "OBS", "Speaker", "ON","OFF"},
  { "OBS", "Scene", "OW","OW"},
  { "OBS", "Scene", "PUBG","PUBG"},
  { "OBS", "Scene", "SoW", "SoW"}
};


void draw_screen() {
  
  // Draw the buttons
  //draw_re(0x00, GREEN, "DISC", "Mic", "ON");
  draw_re(0x00, GREEN);
  //draw_re(0x01, GREEN, "TS", "Mic", "ON");
  draw_re(0x01, GREEN);
  draw_re(0x02, ORANGE, "OBS", "Scene", "Idle");
  draw_re(0x03, ORANGE, "OBS", "Timer", "ON/OFF");
  draw_re(0x04, ORANGE, "OBS", "Scene", "Active");

  draw_re(0x05, GREEN, "DISC", "Speaker", "ON");
  draw_re(0x06, GREEN, "TS", "Speaker", "ON");
  draw_re(0x07);
  draw_re(0x08);
  draw_re(0x09);

  draw_re(0x0A, CYAN, "OBS", "Mic", "ON");
  draw_re(0x0B, CYAN, "OBS", "Speaker", "ON");
  draw_re(0x0C, ORANGE, "OBS", "Scene", "OW");
  draw_re(0x0D, ORANGE, "OBS", "Scene", "PUBG");
  draw_re(0x0E, ORANGE, "OBS", "Scene", "SoW");

}

#define USE_ROUND_RECT
void draw_re(uint8_t bid) {
#ifdef USE_ROUND_RECT
  tft.drawRoundRect(pgm_read_word(&(COLUMN_MAP[COLUMN(bid)])),
           pgm_read_word(&(ROW_MAP[ROW(bid)])), 
           BWIDTH, BHEIGHT, min(BWIDTH,BHEIGHT)/4, WHITE);
#else
  tft.fillRect(pgm_read_word(&(COLUMN_MAP[COLUMN(bid)])),
           pgm_read_word(&(ROW_MAP[ROW(bid)])), 
           BWIDTH, BHEIGHT,  WHITE);
  tft.fillRect(pgm_read_word(&(COLUMN_MAP[COLUMN(bid)]))+1,
           pgm_read_word(&(ROW_MAP[ROW(bid)]))+1, 
           BWIDTH-2, BHEIGHT-2,  BLACK);
#endif
}

void draw_re(uint8_t bid, uint16_t color) {
   draw_re(bid, color, BUTTONS[bid][0], BUTTONS[bid][1],BUTTONS[bid][2],BLACK);
}

void draw_re(uint8_t bid, uint16_t color, uint16_t bgcolor) {
   draw_re(bid, color, BUTTONS[bid][0], BUTTONS[bid][1],BUTTONS[bid][3],bgcolor);
}

void draw_re(uint8_t bid, uint16_t color, String txt1, String txt2, String txt3) {
   draw_re(bid, color, txt1, txt2, txt3, BLACK);
}
void draw_re(uint8_t bid, uint16_t color, String txt1, String txt2, String txt3, uint16_t bgcolor) {
  int x = pgm_read_word(&(COLUMN_MAP[COLUMN(bid)]));
  int y = pgm_read_word(&(ROW_MAP[ROW(bid)]));
  int txtdst = 5;

#ifdef USE_ROUND_RECT
  tft.drawRoundRect(x, y, BWIDTH, BHEIGHT,min(BWIDTH,BHEIGHT)/4, WHITE);
  tft.fillRoundRect(x + 1, y + 1, BWIDTH-2, BHEIGHT-2,min(BWIDTH-2, BHEIGHT-2)/4, bgcolor);
#else
  //tft.drawRect(x, y, BWIDTH, BHEIGHT,WHITE);
  tft.fillRect(x, y, BWIDTH, BHEIGHT,WHITE);
  tft.fillRect(x + 1, y + 1, BWIDTH-2, BHEIGHT-2,bgcolor);
#endif
  
  tft.setTextColor(color);
  //tft.setCursor(x + txtdst, y + txtdst+10);
  tft.setCursor(x + 47-txt1.length()*6, y + txtdst+10);
  tft.print(txt1);
  tft.setCursor(x + 47-txt2.length()*6, y + txtdst + 30);
  tft.print(txt2);
  tft.setCursor(x + 47-txt3.length()*6, y + txtdst + 50);
  tft.print(txt3);
}
