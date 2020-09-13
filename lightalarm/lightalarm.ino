#include <WiFi.h>
#include "time.h"
#include "stdio.h"
#include <TEA5767N.h> //https://github.com/mroger/TEA5767

#include "web.h"
#include "display.h"

//sign-in data for wlan
const char* ssid = "FRITZ!Box 7490";
const char* password =  //TODO

const char* ntp_server = "pool.ntp.org";
#define GMT_OFFSET 3600 //CEST / Berlin is UTC+1
#define DST_OFFSET 3600 //daylight saving time is +1 hour

uint8_t alarm_hour = 7;
uint8_t alarm_minute = 30;
uint8_t alarm_duration = 30;
//how many minutes before the alarm the esp32 will wake up
uint8_t alarm_forerun = 2;
uint8_t intensity_min = 0;
uint8_t intensity;
uint8_t intensity_max = 255;
//timestamp from when the currently running alarm (if any) started
time_t alarm_start;
bool alarm_active = false;

#define VOLUME_PIN 34 //GPIO34
float freq = 106.7f;
TEA5767N radio = TEA5767N();

//after how many minutes of inactivity the esp32 will go to deepsleep
uint8_t active_duration = 10;
//timestamp from the last action preventing the esp from deepsleep
time_t last_action;

void log_radio() {
	Serial.print("Freq: ");
	Serial.print(freq);
	Serial.print(" signal level: ");
	Serial.println(radio.getSignalLevel());
}

//prints current time to console
bool log_time() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return false;
  }
  Serial.println(&timeinfo, "Time: %A, %B %d %Y %H:%M:%S");
  return true;
}

//syncs clock to ntp server
void sync_time() {
  Serial.println("Attempting NTP request");
  configTime(GMT_OFFSET, DST_OFFSET, ntp_server);
}

//puts esp into deepsleep
void go_to_sleep() {
  Serial.println("Going to sleep");
  WiFi.disconnect();
  
	//get timestamp at which alarm shall happen
	struct tm* alarm;
	if(!getLocalTime(alarm)) {
		Serial.println("Fatal: couldn't set alarm");
	}
	//if current hour > alarm_hour or = and min > alarm_minute then advance one day
	if(alarm->tm_hour > alarm_hour
				|| (alarm->tm_hour == alarm_hour && alarm->tm_min > alarm_minute)) {
		alarm->tm_mday = alarm->tm_mday + 1;
	}
	alarm->tm_sec = 0;
	alarm->tm_min = alarm_minute;
	alarm->tm_hour = alarm_hour;
	time_t alarmstamp = mktime(alarm);
	alarmstamp -= alarm_forerun * 60000;
	
	//get current timestamp
	time_t now;
	time(&now);
	
	//check alarm hasn't passed
	if(alarmstamp <= now) {
			//can happen, if go_to_sleep() is called shortly before the alarm
			//and alarm_forerun then brings it into the past
			//so just skip this go_to_sleep
			Serial.println("Wanted to set alarm in the past. Probably just alarm_forerun.");
			return;
	}
	
	//wait to be readible in console, then go to sleep
  delay(1000);
	//in microseconds, so * 1000
  esp_sleep_enable_timer_wakeup( (alarmstamp - now) * 1000 );
  esp_deep_sleep_start();
}

void setup() {
	Serial.begin(115200);
  Serial.println("Woke up");
	
	// check wlan
	if(WiFi.status() != WL_CONNECTED) {
    //clear all previous things, apparently this is necessary
    //WiFi.disconnect();
		Serial.print("Wlan not connected, trying ");
		Serial.println(ssid);
		WiFi.mode(WIFI_STA);
		//WiFi.begin(ssid, password);
		while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
			delay(500);
			Serial.print(".");
		}
		Serial.println();
		Serial.print("Got IP: ");
		Serial.println(WiFi.localIP());
	}else {
		Serial.println("Wlan connected");
	}

  //test if time can be gotten, otherwise sync via NTP
  if (!log_time()) {
		//sync_time(); //TODO
    log_time();
	}

  //setup webpage
	ConfigVariables config = {
		&intenmin,
		&intenmax,
		&alarm_duration,
		&alarm_forerun,
		&active_duration,
		&alarm_hour,
		&alarm_minute,
		&get_freq,
		&update_freq,
	};
  init_webserver(config);
	
	//init radio
	radio.setMonoReception();
	radio.setStereoNoiseCancellingOn();
	radio.selectFrequency(freq);
	
	//init led matrix
	init_display();
	
	//prepare for sleep in active_duration minutes
  time(&last_action);
	//TODO button for wakeup
	
	delay(100);
}

void set_freq(float frequency) {
	freq = frequency;
	radio.selectFrequency(freq);
}

void loop() {
	//check for having to go to sleep
  time_t nowstamp;
  time(&nowstamp);
  if(nowstamp > last_action + active_duration * 60000 && !alarm_active) {
    go_to_sleep();
  }
	
	//check for having to start alarm
	struct tm now;
	if(!getLocalTime(&now)) {
		Serial.println("Fatal: couldn't check for alarm");
	}else if(now.tm_hour == alarm_hour && now.tm_min == alarm_minute) {
		alarm_active = true;
		time(&alarm_start);
		intensity = intensity_min;
		radio.setStandByOff();
		log_radio();
		//intensity is calculated as f(x) = intensity_scaling * duration^2 + intensity_min
		//that way intensity increases slower at the beginning
		intensity_scaling = (intensity_max - intensity_min) / (alarm_duration * alarm_duration);
	}
	
	//maybe perform alarm step
	if(alarm_active) {
		float duration = (nowstamp-alarm_start) / 60000;
		uint8_t tmp = intensity_scaling * duration * duration + intensity_min;
		if(tmp >= 255) {
			//alarm finished but stay active till deactivated
			//TODO deactivate alarm button
			tmp = 255;
		}
		if(tmp != intensity) {
			//update intensity of radio, lamp
			update_alarm_intensity(intensity);
			//TODO update radio
			intensity = tmp;
		}
	}
	
	//actualize display
	display_loop();
	
	//wait a bit, as actions don't have to be performed constantly
	delay(1000);
}

//TODO switch from LED Matrix to lcd5110+lamp?