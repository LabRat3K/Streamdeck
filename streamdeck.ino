#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include <HX8347_kbv.h>
#include "Keyboard.h"

// TOUCH SCREEN CALIBRATION
// Run the calibration routine and update the MIN/MAX for X/Y
//
#define TS_MINX 320
#define TS_MAXX 3850
#define TS_MINY 3800
#define TS_MAXY 305

// TFT CONFIGURATION
// These pins should correspond with your TFT display wiring
#define LCD_CS      A3
#define LCD_CD      A2
#define LCD_WR      A1
#define LCD_RD      A0
#define LCD_RESET   A4
#define TIRQ_PIN    3
#define TFT_PIN     4


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

// Key event definitions
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

boolean t1, t2, t3, t4, t5  , t6, t7, t8, t9, t10   , t11, t12;
boolean t1h;

XPT2046_Touchscreen tss(TFT_PIN, TIRQ_PIN);
HX8347_kbv tft;

void setup(void) {
  Serial.begin(9600);
  Serial.setTimeout(50);
  tft.begin();
  initial();
  Keyboard.begin();
  tss.begin();
}

void loop() {
  write_Jinput();
  if (tss.touched()) {
    TS_Point p = tss.getPoint();
    p.x = map(p.x, MINX, MAXX, 0, 480);
    p.y = map(p.y, MINY, MAXY, 0, 320);

    //################## Code for actions here ##################

    // Determine touch point is in a column
    unsigned byte intersections= 0x00;
    if (p.x >  20 && p.x <  90) intersections |= 0x01;
    if (p.x > 115 && p.x < 180) intersections |= 0x02;
    if (p.x > 200 && p.x < 275) intersections |= 0x04;
    if (p.x > 290 && p.x < 365) intersections |= 0x08;
    if (p.x > 385 && p.x < 455) intersections |= 0x10;


    if (p.y >  80 && p.y < 150)  intersection |= 0x20;
    if (p.y > 165 && p.y < 235)  intersection |= 0x40;
    if (p.y > 245 && p.y < 315)  intersection |= 0x80;

/* 
   Now we can use a single byte to identify if a key is pressed 
   Top Row: 0x21, 0x22, 0x24, 0x28, 0x30
   2nd Row: 0x41, 0x42, 0x44, 0x48, 0x50
   3rd Row: 0x81, 0x82, 0x84, 0x88, 0x90
*/
    switch (intersections) {
       //################## LINE 1 ##################
       case 0x21:
	      if (t1) {
		draw_re(10, 60, GREEN, "DISC", "Mic", "ON");
		draw_re(10, 120, GREEN, "DISC", "Speaker", "ON");
		t6 = false;
		t1 = false;
		t1h = false;
	      } else {
		if (t6) {
		  t1h = false;
		} else {
		  t1h = true;
		}
		draw_re(10, 60, RED, "DISC", "Mic", "OFF");
		t1 = true;
	      }
	      Keyboard.write(KEY_F13);
              break;
       case 0x22:
	      if (t2) {
		draw_re(70, 60, GREEN, "TS", "Mic", "ON");;
		t2 = false;
	      } else {
		draw_re(70, 60, RED, "TS", "Mic", "OFF");
		t2 = true;
	      }
	      Keyboard.write(KEY_F15);
              break;
       case 0x24:
	      Keyboard.write(KEY_F17);
              break;
       case 0x28:
	      Keyboard.write(KEY_F18);
              break;
       case 0x30:
	      Keyboard.write(KEY_F19);
              break;

       //################## LINE 2 ##################
       case 0x41:
	      if (t6) {
		if (t1h == false) {
		  draw_re(10, 60, GREEN, "DISC", "Mic", "ON");
		  t1 = false;
		}
		draw_re(10, 120, GREEN, "DISC", "Speaker", "ON");
		t6 = false;
	      } else {
		draw_re(10, 120, RED, "DISC", "Speaker", "OFF");
		draw_re(10, 60, RED, "DISC", "Mic", "OFF");
		t6 = true;
		t1 = true;
	      }
	      Keyboard.write(KEY_F14);
              break;
       case 0x42:
	      if (t7) {
		draw_re(70, 120, GREEN, "TS", "Speaker", "ON");
		t7 = false;
	      } else {
		draw_re(70, 120, RED, "TS", "Speaker", "OFF");
		t7 = true;
	      }
	      Keyboard.write(KEY_F16);
	      break;
       case 0x44:
       case 0x48:
       case 0x50: // Currently these don't do anything
              break;
       //################## LINE 3 ##################
       case 0x81:
	      if (t11) {
		draw_re(10, 180, CYAN, "OBS", "Mic", "ON");
		t11 = false;
	      } else {
		draw_re(10, 180, RED, "OBS", "Mic", "OFF");
		t11 = true;
	      }
	      Keyboard.write(KEY_F23);
	      break;
       case 0x82:
	      if (t12) {
		draw_re(70, 180, CYAN, "OBS", "Speaker", "ON");
		t12 = false;
	      } else {
		draw_re(70, 180, RED, "OBS", "Speaker", "OFF");
		t12 = true;
	      }
	      Keyboard.write(KEY_F24);
	      break;
       case 0x84:
	      Keyboard.write(KEY_F20);
              break;
       case 0x88:
	      Keyboard.write(KEY_F21);
              break;
       case 0x90:
	      Keyboard.write(KEY_F22);
              break;
       default: {
              // Do Nothing - this was not a keypress
              }
    } // End SWITCH 
    delay(500);
  }
}

void initial() {
  //################## Initial ##################
  tft.setRotation(3);
  tft.fillScreen(RED);
  tft.fillScreen(WHITE);
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);

  draw_re(10, 60, GREEN, "DISC", "Mic", "ON");
  draw_re(70, 60, GREEN, "TS", "Mic", "ON");
  draw_re(130, 60, ORANGE, "OBS", "Scene", "Idle");
  draw_re(190, 60, ORANGE, "OBS", "Timer", "ON/OFF");
  draw_re(250, 60, ORANGE, "OBS", "Scene", "Active");

  draw_re(10, 120, GREEN, "DISC", "Speaker", "ON");
  draw_re(70, 120, GREEN, "TS", "Speaker", "ON");
  draw_re(130, 120);
  draw_re(190, 120);
  draw_re(250, 120);

  draw_re(10, 180, CYAN, "OBS", "Mic", "ON");
  draw_re(70, 180, CYAN, "OBS", "Speaker", "ON");
  draw_re(130, 180, ORANGE, "OBS", "Scene", "OW");
  draw_re(190, 180, ORANGE, "OBS", "Scene", "PUBG");
  draw_re(250, 180, ORANGE, "OBS", "Scene", "SoW");

}

void draw_re(int x, int y) {
  //Lenght: 32 + 20 = 52
  int dist = 10;
  tft.drawRect(x, y, 32 + 2 * dist, 32 + 2 * dist, WHITE);
}

void draw_re(int x, int y, uint16_t color, String txt1, String txt2, String txt3) {
  //Lenght: 32 + 20 = 52
  int dist = 10;
  int txtdst = 5;
  tft.drawRect(x, y, 32 + 2 * dist, 32 + 2 * dist, WHITE);
  tft.fillRect(x + 1, y + 1, 50, 50, BLACK);
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(x + txtdst, y + txtdst);
  tft.println(txt1);
  tft.setCursor(x + txtdst, y + txtdst + 10);
  tft.println(txt2);
  tft.setCursor(x + txtdst, y + txtdst + 20);
  tft.println(txt3);
}

void write_Jinput() {
  String JString = Serial.readString();
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  int x_desc = 5;
  int y_desc = 5;
  String count_items = getValue(JString, ';', 0);

  for (int i = 1; i < count_items.toInt() * 2; i = i + 2) {
    String string_desc = getValue(JString, ';', i);
    String string_item = getValue(JString, ';', i + 1);
    tft.fillRect(x_desc + 50, y_desc, 100, 7, BLACK); //x,y,l,h
    tft.setCursor(x_desc, y_desc);
    tft.println(string_desc);
    tft.setCursor(50 + x_desc, y_desc);
    tft.println(string_item);
    y_desc > 24 ? x_desc = x_desc + 150 : x_desc;
    y_desc > 24 ? y_desc = 5 : y_desc = y_desc + 10;
  }
  JString = "";
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
