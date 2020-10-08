#include <WiFi.h>
#include <SPI.h>
#include <TEA5767N.h> //https://github.com/mroger/TEA5767
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "time.h"
#include "stdio.h"

#include "webpage.h"

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

Adafruit_PCD8544 display = Adafruit_PCD8544(26, 27, 14);

AsyncWebServer server(80);

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

void updateIntensityOnDevice() {
	ledcWrite(0, intensity);
}

void set_freq(float frequency) {
	freq = frequency;
	radio.selectFrequency(freq);
}

void IRAM_ATTR btn_change() {
		time(&last_action);
    if(alarm_active) {
			//turn alarm off
			alarm_active = false;
			intensity = 0;
			radio.setStandByOn();
		}else {
			alarm_active = true;
			intensity = intensity_max;
			radio.setStandByOff();
		}
		updateIntensityOnDevice();
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
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 1);//trigger wakeup on high
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

  //init webpage
  init_webserver();
	
	//init radio
	radio.setMonoReception();
	radio.setStereoNoiseCancellingOn();
	radio.selectFrequency(freq);
	ledcSetup(0, 5000, 8);//channel frequence resolution
	ledcAttachPin(34, 0);//GPIO34
	
	//init led display
  display.begin();
	
	//prepare for sleep in active_duration minutes
  time(&last_action);
	
	// init btn for start radio / stop alarm
	attachInterrupt(GPIO_NUM_33, btn_change, FALLING)
	
	delay(100);
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
		updateIntensityOnDevice();
	}
	
	//maybe perform alarm step
	if(alarm_active) {
		float duration = (nowstamp-alarm_start) / 60000;
		uint8_t new_intensity = intensity_scaling * duration * duration + intensity_min;
		if(new_intensity >= 255) {
			//alarm finished but stay active till deactivated
			new_intensity = 255;
		}
		if(new_intensity != intensity) {
			//update intensity of radio, lamp
			updateIntensityOnDevice();
			//TODO update radio
			intensity = new_intensity;
		}
	}
	
	//draw time etc. on display
	display.clearDisplay()
	display.setTextSize(2);
	display.setTextColor(BLACK);
	display.setCursor(1,1);
	char time[6];
	sprintf(time, "%02u:%02u", now.tm_hour, now.tm_min);
	display.println(time);
	display.fillRect(0,display.height()-2, display.width() * intensity / 255, display.height(), BLACK);
	display.display();
	
	//wait a bit, as actions don't have to be performed constantly
	delay(1000);
}

String processor(const String& var) {
  if(var == "intenmin")
    return String(intensity_min);
  else if(var == "intenmax")
    return String(intensity_max);
  else if(var == "duration")
    return String(alarm_duration);
  else if(var == "forerun")
    return String(alarm_forerun);
  else if(var == "active")
    return String(active_duration);
  else if(var == "time") {
		char ret[6];
		sprintf(ret, "%02u:%02u", alarm_hour, alarm_minute);
		return String(ret);
	} else if(var == "freq")
    return String(freq);
  else {
		Serial.print("Unknown preprocessor string: ");
		Serial.println(var);
		return String();
	}
}

void print_debug(AsyncWebServerRequest *request) {
  Serial.println("---Printing Debug Information---");
  //List all collected headers
  int headers = request->headers();
  int i;
  for(i=0;i<headers;i++){
    AsyncWebHeader* h = request->getHeader(i);
    Serial.printf("HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
  }
  //List all parameters
  int params = request->params();
  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    if(p->isFile()){ //p->isPost() is also true
      Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    } else if(p->isPost()){
      Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    } else {
      Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    }
  }
}

void handle_post_body(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  Serial.println("Got post body");
  /**if(!index){
    Serial.printf("BodyStart: %u B\n", total);
  }
  for(size_t i=0; i<len; i++){
    Serial.write(data[i]);
  }
  if(index + len == total){
    Serial.printf("BodyEnd: %u B\n", total);
  }*/
  //turn data into null terminated string
  //result: &time=&duration=30&intensitymin=0&intensitymax=255&freq=106.7&activetime=10&forerun=2
  char bodystr[len+1];
  memcpy(bodystr, data, len);
  bodystr[len+1] = '\0';
  Serial.println(bodystr);

  //extract tokens (type, number) from string
  char* token = strtok(bodystr, "&=:");
  while(token != NULL) {
    Serial.printf("token: %s \n", token);
    if(strcmp(token, "time")) {               //&time=07:30
      char* buf = strtok(NULL, "&=:");
      int tim = strtol(buf,NULL,10);
      Serial.printf("hour: %d", tim);
      buf = strtok(NULL, "&=:");
      tim = strtol(buf,NULL,10);
      Serial.printf("min: %d \n", tim);
    }else if(strcmp(token, "duration")) {     //&duration=30
      char* buf = strtok(NULL, "&=:");
      int tim = strtol(buf,NULL,10);
      Serial.printf("duration: %d", tim);
    }else if(strcmp(token, "intensitymin")) { //&intensitymin=0
      char* buf = strtok(NULL, "&=:");
      int tim = strtol(buf,NULL,10);
      Serial.printf("intensitymin: %d", tim);
    }else if(strcmp(token, "intensitymax")) { //&intensitymax=255
      char* buf = strtok(NULL, "&=:");
      int tim = strtol(buf,NULL,10);
      Serial.printf("intensitymax: %d", tim);
    }else if(strcmp(token, "freq")) {         //&freq=106.7
      char* buf = strtok(NULL, "&=:");
      float tim = strtof(buf,NULL);
      Serial.printf("hour: %f", tim);
    }else if(strcmp(token, "activemin")) {    //&activemin=10
      char* buf = strtok(NULL, "&=:");
      int tim = strtol(buf,NULL,10);
      Serial.printf("activemin: %d", tim);
    }else if(strcmp(token, "forerun")) {      //&forerun=2
      char* buf = strtok(NULL, "&=:");
      int tim = strtol(buf,NULL,10);
      Serial.printf("forerun: %d", tim);
    }
    token = strtok(NULL, "&=:");
  }
	//TODO actually update config
	//TODO sanity checks e.g. intensitymin >= 0 intensitymax <=255 etc.
}

void init_webserver() {	
	//setup server
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //send webpage
    request->send_P(200, "text/html", webpage, processor);
    Serial.println("Got / get");
  });
  server.on(
    "/",
    HTTP_POST,
    [](AsyncWebServerRequest *request){},
    NULL,
    handle_post_body
  );
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
    print_debug(request);
  });
  server.begin();
  Serial.println("Webserver started");
}