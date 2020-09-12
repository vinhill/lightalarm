#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

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

AsyncWebServer server(80);

ConfigVariables config;

String processor(const String& var) {
  if(var == "intenmin")
    return String(*config.intensity_min);
  else if(var == "intenmax")
    return String(*config.intensity_max);
  else if(var == "duration")
    return String(*config.alarm_duration);
  else if(var == "forerun")
    return String(*config.alarm_forerun);
  else if(var == "active")
    return String(*config.active_duration);
  else if(var == "time") {
		char ret[5];
		sprintf(ret, "%02u:%02u", *config.alarm_hour, *config.alarm_minute);
		return String(ret);
	} else if(var == "freq")
    return String(*config.freq);
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
}

void init_webserver(ConfigVariables configvariables) {
	//pointers to all the variables we have to modify
	config = configvariables;
	
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
}