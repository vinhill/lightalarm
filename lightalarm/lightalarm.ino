#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include "stdio.h"

const char* ssid = "FRITZ!Box 7490";
const char* password = "10907494459553438174";

const char* ntp_server = "pool.ntp.org";
#define GMT_OFFSET 3600 //CEST / Berlin is UTC+1
#define DST_OFFSET 3600 //daylight saving time is +1 hour

AsyncWebServer server(80);

uint8_t alarm_hour = 7;
uint8_t alarm_minute = 30;
uint8_t alarm_duration = 30;
uint8_t alarm_forerun = 2;
uint8_t intensity_min = 0;
uint8_t intensity_max = 255;
float intensity_scaling = 0;
time_t alarm_start;

#define VOLUME_PIN 34 //GPIO34
float freq = 106.7f;

uint8_t active_duration = 10;
time_t last_action;
bool alarm_active = false;

const static char webpage[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  
	<title>LightAlarm</title>
  
	<link href='http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/themes/base/jquery-ui.css' rel='stylesheet' type='text/css'>
	<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/4.7.0/css/font-awesome.min.css">
	<script src="https://code.jquery.com/jquery-1.12.4.js"></script>
  <script src="https://code.jquery.com/ui/1.12.1/jquery-ui.js"></script>
  
	<!-- Buttons -->
	<script>
		function onAdvancedClick() {
			var div = document.getElementById("advanced-settings");
			var btn = document.getElementById("btn-advanced");
			if(div.style.display === "none") {
				div.style.display = "block";
				btn.firstChild.data = "hide";
			}else {
				div.style.display = "none";
				btn.firstChild.data = "show";
			}
		}
		
		function transmitToArduino() {
			var body = [];
			body.push(
				"&time=",
				document.getElementById("alarm-time-viewer").value,
				"&duration=",
				document.getElementById("alarm-duration-viewer").value,
				"&intensitymin=",
				$( "#intensity-range-slider" ).slider( "values", 0 ),
				"&intensitymax=",
				$( "#intensity-range-slider" ).slider( "values", 1 ),
				"&freq=",
				document.getElementById("frequency-viewer").value,
				"&activetime=",
				document.getElementById("active-duration-viewer").value,
				"&forerun=",
				document.getElementById("alarm-forerun-viewer").value,
				"&nocache=" + Math.random() * 1000000
			)
			console.log(body.join(""));
			var request = new XMLHttpRequest();
			request.open("POST", "", true);
			request.send(body.join(""));
		}
		
		function syncToArduino() {
			document.getElementById("alarm-time-viewer").value = "%time%";
			document.getElementById("frequency-viewer").value = %freq%;
			document.getElementById("active-duration-viewer").value = %active%;
			document.getElementById("alarm-forerun-viewer").value = %forerun%;
		}
		</script>
		<script>
		$( function() {
			$( "#intensity-range-slider" ).slider({
				range: true,
				min: 0,
				max: 255,
				values: [ %intenmin%, %intenmax% ],
				slide: function( event, ui ) {
					$( "#intensity-viewer" ).val( ui.values[ 0 ] + " - " + ui.values[ 1 ] );
				}
			});
			$( "#intensity-viewer" ).val( $( "#intensity-range-slider" ).slider( "values", 0 )
					+ " - " + $( "#intensity-range-slider" ).slider( "values", 1 ) );
		} );
		$( function() {
			$( "#alarm-duration-slider" ).slider({
				range: false,
				min: 5,
				max: 60,
				value: %duration%,
				slide: function( event, ui ) {
					$( "#alarm-duration-viewer" ).val( ui.value );
				}
			});
			$( "#alarm-duration-viewer" ).val( $( "#alarm-duration-slider" ).slider("value") );
		} );
		
		$(document).ready( function() {
			syncToArduino();
		});
  </script>
	
	<style>
		h1 {
			color: #33cccc;
			font-weight: bold;
		}
		h2 {
			font-size: large;
			color: #4EA5A5;
		}
		.description{
			color: #666666;
			font-size: small;
			line-break: normal;
		}
		.value-selector{
			width: 25em;
			padding-left: 1em;
			margin-bottom: 0.5em;
		}
		.value-text{
			border: 0;
			color: #f6931f;
			font-weight: bold;
		}
	</style>
</head>
<body>
	<h1>
			LightAlarm Configuration Page
	</h1>
	
	<div class="description">
		Remember to press <button onClick="transmitToArduino()" style="font-size:x-small">save settings</button> before closing the page!
	</div>
	
	<h2>
		Alarm Settings
	</h2>
	
	<div id="alarm-time-div" class="value-selector">
		<label for="alarm-time-viewer">Time:</label>
		<input type="time" id="alarm-time-viewer" class="value-text" min="00:00" max="23:59" value="07:30">
		<div class="description">
			The time at which the LightAlarm should start.
		</div>
	</div>
	
	<div id="alarm-duration-div" class="value-selector">
		<label for="alarm-duration-viewer">Duration:</label>
		<input type="text" id="alarm-duration-viewer" readonly class="value-text" style="width: 20px">
		<div style="display: inline-block; width: 100px; height: 10px" id="alarm-duration-slider"></div>
		&#10710
		<div class="description">
			Starting at the alarm time, how long should the LightAlarm take till it reaches peak intensity?
		</div>
	</div>
	
	<div id="intensity-range-div" class="value-selector">
		<label for="intensity-viewer">Brightness and Volume:</label>
		<input type="text" id="intensity-viewer" readonly class="value-text" style="width: 60px">
		<div style="display: inline-block; width: 100px; height: 10px; margin-right: 1em" id="intensity-range-slider"></div>
		<i class="fa fa-lightbulb-o" style="font-size:normal"></i>
		<div class="description">
			Select at which intensity the LightAlarm should start (brightness of light and volume of radio) as well as the maximum intensity that will be reached.
		</div>
	</div>
	
	<h2>
		Radio
	</h2>
	
	<div id="radio-frequency-div" class="value-selector">
		<label for="frequency-viewer">Frequency:</label>
		<input type="number" id="frequency-viewer" class="value-text" style="width: 50px" value="106.7" min="97" max="107">
		<i class="fa fa-signal" style="font-size:normal"></i>
		<div class="description">
			The frequency of the radio.
		</div>
	</div>
	
	<h2 style="display:inline">
		Advanced
	</h2>
	
	<button id="btn-advanced" onClick="onAdvancedClick()">show</button>
	
	<div id="advanced-settings" style="display: none">
		
		<div id="active-duration-div" class="value-selector">
			<label for="active-duration-viewer">Active Duration:</label>
			<input type="number" id="active-duration-viewer" class="value-text" style="width: 50px" value="10" min="1" max="255">
			<div class="description">
				How many minutes the LightAlarm will wait before going into energy saving mode. This needs to be larger than Alarm Forerun. During energy saving mode the lcd is off and this webpage cannot be accessed.
			</div>
		</div>
		
		<div id="alarm-forerun-div" class="value-selector">
			<label for="alarm-forerun-viewer">Alarm Forerun:</label>
			<input type="number" id="alarm-forerun-viewer" class="value-text" style="width: 50px" value="2" min="1">
			<div class="description">
				How many minutes the LightAlarm will awake from energy saving mode before triggering the actual alarm. This needs to be bigger than zero and smaller than Active Duration. During this forerun, radio and light are off but the lcd already shows the clock.
			</div>
		</div>
		<!--
		<div id="slow-start-percentage-div" class="value-selector">
			<label for="slow-start-percentage-viewer">Slow Start Percentage:</label>
			<input type="number" id="slow-start-percentage-viewer" class="value-text" style="width: 50px" value="0.05" min="0" max="1">
			<div class="description">
				Humans perceive intensity of light logarithmic, therfore the first increases in intensity are slowed down. Otherwise the light would appear to become quite bright quite early.
				<br>
				So the first slow start percentage of all increases will take slow start slowness as long as normal.
			</div>
		</div>
		
		<div id="slow-start-slowness-div" class="value-selector">
			<label for="slow-start-slowness-viewer">Slow Start Slowness:</label>
			<input type="number" id="slow-start-slowness-viewer" class="value-text" style="width: 50px" value="6" min="0" max="255">
			<div class="description">
				Humans perceive intensity of light logarithmic, therfore the first increases in intensity are slowed down. Otherwise the light would appear to become quite bright quite early.
				<br>
				So the first slow start percentage of all increases will take slow start slowness as long as normal.
			</div>
		</div>
		-->
	
	</div>
	
	<br>
	<button onClick="transmitToArduino()">save settings</button>
</body>
</html>
)rawliteral";

bool print_time() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return false;
  }
  Serial.println(&timeinfo, "Time: %A, %B %d %Y %H:%M:%S");
  return true;
}

void sync_time() {
  Serial.println("Attempting NTP request");
  configTime(GMT_OFFSET, DST_OFFSET, ntp_server);
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
		char ret[5];
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
}

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
  if (!print_time()) {
		//sync_time(); //TODO
    print_time();
	}

  //setup server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //send webpage
    request->send_P(200, "text/html", webpage, processor);
    Serial.println("Got / get");
  });
  server.on(
    "/",
    HTTP_POST,
    [](AsyncWebServerRequest * request){},
    NULL,
    handle_post_body
  );
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404);
    print_debug(request);
  });
  server.begin();
  Serial.println("Webserver started");
  delay(100);

  //prepare for sleep in active_duration minutes
  time(&last_action);
	//TODO button for wakeup
}

void loop() {
  time_t nowstamp;
  time(&nowstamp);
  if(nowstamp > last_action + active_duration * 60000 && !alarm_active) {
    go_to_sleep();
  }
	struct tm now;
	if(!getLocalTime(&now)) {
		Serial.println("Fatal: couldn't check for alarm");
	}else if(now.tm_hour == alarm_hour && now.tm_min == alarm_minute) {
		alarm_active = true;
		time(&alarm_start);
		//intensity is calculated as f(x) = intensity_scaling * duration^2 + intensity_min
		//that way the beginning is a bit slower
		intensity_scaling = (intensity_max - intensity_min) / (alarm_duration * alarm_duration);
	}
	if(alarm_active) {
		float duration = (nowstamp-alarm_start) / 60000;
		uint8_t intensity = intensity_scaling * duration * duration + intensity_min;
		if(intensity >= 255) {
			//alarm finished but stay active till deactivated
			//TODO deactivate alarm button
			intensity = 255;
		}
		//TODO use intensity on radio, lamp
	}
	//TODO print date, time, uv, weather...
	delay(1000);
}