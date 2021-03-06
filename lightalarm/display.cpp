//adapted from pixeltime example

#include <PxMatrix.h>
#include "time.h"
#include "stdio.h"

// This is how many color levels the display shows - the more the slower the update
//#define PxMATRIX_COLOR_DEPTH 8

// Defines the speed of the SPI bus (reducing this may help if you experience noisy images)
//#define PxMATRIX_SPI_FREQUENCY 20000000

// Creates a second buffer for backround drawing (doubles the required RAM)
//#define PxMATRIX_double_buffer true

// Defines the buffer height / the maximum height of the matrix
#define PxMATRIX_MAX_HEIGHT 32

// Defines the buffer width / the maximum width of the matrix
#define PxMATRIX_MAX_WIDTH 64

#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time=60; //30-70 is usually fine

PxMATRIX display(PxMATRIX_MAX_WIDTH,PxMATRIX_MAX_HEIGHT,P_LAT, P_OE,P_A,P_B,P_C,P_D,P_E);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myWARMWHITE = display.color565(255, 214, 170);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);

uint16_t myCOLORS[8]={myRED,myGREEN,myBLUE,myWHITE,myYELLOW,myCYAN,myMAGENTA,myBLACK};

void IRAM_ATTR display_updater(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void display_update_enable(bool is_enable) {
  if (is_enable)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 4000, true);
    timerAlarmEnable(timer);
  }
  else
  {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
}

void init_display() {
  // Define display layout, e.g. 1/16 step
  display.begin(16);

  // Control the minimum color values that result in an active pixel
  //display.setColorOffset(5, 5,5);

  // Set the brightness of the panels (default is 255)
  //display.setBrightness(50);

  display_update_enable(true);
}

//space in px on the left of the display for e.g. text
#define WIDTH_SEPARATOR 30

void display_loop() {
	//clear text area
	fill(0,0,WIDTH_SEPARATOR, PxMATRIX_MAX_HEIGHT, myBLACK);
	
	//show time
	struct tm now;
	if(getLocalTime(&now)) {
		char time[6];
		sprintf(time, "%02u:%02u", now.tm_hour, now.tm_min);
		display.setTextColor(myCyan);
		display.setCursor(2,0);
		display.print(time);
	}
	//TODO uv-index, weather...
}

void fill(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color) {
	for(uint8_t x = x1; x < x2; x++) {
		for(uint8_t y = y1; y < y2; y++) {
			display.drawPixel(x,y,color);
		}
	}
}

void update_alarm_intensity(uint8_t intensity) {
	//make right side bright
	uint8_t height = PxMATRIX_MAX_HEIGHT / 255 * intensity / 2; //todo /2 to not pull too much power
	fill(WIDTH_SEPARATOR,0,PxMATRIX_MAX_WIDTH, height, myWARMWHITE);
}