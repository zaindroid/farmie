/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-iot-shield-pcb-dashboard/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

// Replace with your network credentials
const char* ssid = "Rivendell";
const char* password = "Fireon1122";

// Web Server HTTP Authentication credentials
const char* http_username = "farmie";
const char* http_password = "farmie2.0";

Adafruit_BME280 bme;         // BME280 connect to ESP32 I2C (GPIO 21 = SDA, GPIO 22 = SCL)
const int buttonPin = 32;    // Pushbutton
const int ledPin = 19;       // Status LED
const int output = 18;       // Output socket
const int ldr = 33;          // LDR (Light Dependent Resistor)
const int motionSensor = 27; // PIR Motion Sensor
const int waterPumpPin = 5;     // Water pump connected to GPIO 5
const int fertilizerPumpPin = 21; // Fertilizer pump connected to GPIO 21

// Define the PWM channel for the light
const int lightPwmChannel = 2; // Ensure this is a unique channel not used by other PWM devices
const int lightFreq = 5000; // 5kHz frequency is typical for LEDs
const int lightResolution = 8; // 8-bit resolution
const int lightPwmPin = 23; // LED connected to GPIO 23

// Define the PWM channel for the fan
const int fanPwmChannel = 1;
const int fanFreq = 25000; // 25kHz frequency typically used for fans
const int fanResolution = 8; // 8-bit resolution
const int fanPwmPin = 22; // Fan connected to GPIO 22

int ledState = LOW;           // current state of the output pin
int buttonState;              // current reading from the input pin
int lastButtonState = LOW;    // previous reading from the input pin
bool motionDetected = false;  // flag variable to send motion alert message
bool clearMotionAlert = true; // clear last motion alert message from web page

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncEventSource events("/events");

const char* PARAM_INPUT_1 = "state";

// Checks if motion was detected
void IRAM_ATTR detectsMovement() {
  //Serial.println("MOTION DETECTED!!!");
  motionDetected = true;
  clearMotionAlert = false;
}

// Main HTML web page in root url /
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>FARMIE DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h3 {font-size: 1.8rem; color: white;}
    h4 { font-size: 1.2rem;}
    p { font-size: 1.4rem;}
    body {  margin: 0;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px; margin-bottom: 20px;}
    .switch input {display: none;}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 68px;   opacity: 0.8;   cursor: pointer;}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #1b78e2}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    .topnav { overflow: hidden; background-color: #1b78e2;}
    .content { padding: 20px;}
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
    .slider2 { -webkit-appearance: none; margin: 14px;  height: 20px; background: #ccc; outline: none; opacity: 0.8; -webkit-transition: .2s; transition: opacity .2s; margin-bottom: 40px; }
    .slider:hover, .slider2:hover { opacity: 1; }
    .slider2::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 40px; height: 40px; background: #008B74; cursor: pointer; }
    .slider2::-moz-range-thumb { width: 40px; height: 40px; background: #008B74; cursor: pointer;}
    .reading { font-size: 2.6rem;}
    .card-switch {color: #50a2ff; }
    .card-light{ color: #008B74;}
    .card-bme{ color: #572dfb;}
    .card-motion{ color: #3b3b3b; cursor: pointer;}
    .icon-pointer{ cursor: pointer;}
  </style>
</head>
<body>
  <div class="topnav">
    <h3>FARMIE DASHBOARD <span style="text-align:right;">&nbsp;&nbsp; <i class="fas fa-user-slash icon-pointer" onclick="logoutButton()"></i></span></h3>
  </div>
  <div class="content">
    <div class="cards">
      %BUTTONPLACEHOLDER%
    
      <div class="card card-bme">
        <h4><i class="fas fa-chart-bar"></i> TEMPERATURE</h4><div><p class="reading"><span id="temp"></span>&deg;C</p></div>
      </div>
      <div class="card card-bme">
        <h4><i class="fas fa-chart-bar"></i> HUMIDITY</h4><div><p class="reading"><span id="humi"></span>&percnt;</p></div>
      </div>

      <div class="card">
  <h4>Light Intensity</h4>
  <input type="range" onchange="adjustLightIntensity(this)" id="light-slider" min="0" max="255" value="127" class="slider2">
</div>

<div class="card">
  <h4>Fan Speed</h4>
  <input type="range" onchange="adjustFanSpeed(this)" id="fan-slider" min="0" max="255" value="127" class="slider2">
</div>


  </div>
<script>
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}

function togglePump(pumpType) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/toggle-" + pumpType, true);
  xhr.send();
}

function adjustLightIntensity(element) {
  var xhr = new XMLHttpRequest();
  var value = element.value; // Get the value from the slider
  xhr.open("GET", "/adjust-light?intensity=" + value, true); // Send the value as a query parameter
  xhr.send();
}

function adjustFanSpeed(element) {
  var xhr = new XMLHttpRequest();
  var value = element.value; // Get the value from the slider
  xhr.open("GET", "/adjust-fan?speed=" + value, true); // Send the value as a query parameter
  xhr.send();
}

function controlOutput(element) {
  var xhr = new XMLHttpRequest();
  var state = element.checked ? "1" : "0";
  var url = "";

  // Determine the control type based on the ID attribute of the element
  switch(element.id) {
    case "output":
      url = "/output?state=" + state;
      break;
    case "status-led":
      url = "/toggle-led?state=" + state;
      break;
    case "toggle-water":
      url = "/toggle-water?state=" + state;
      break;
    case "toggle-fertilizer":
      url = "/toggle-fertilizer?state=" + state;
      break;
    default:
      console.error("Unknown control element", element.id);
      return; // Exit the function if control type is unknown
  }

  xhr.open("GET", url, true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4 && xhr.status === 200) {
      console.log(xhr.responseText);
    }
  };
  xhr.send();
}


function toggleLed(element) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/toggle", true);
  xhr.send();
}

if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 source.addEventListener('led_state', function(e) {
  console.log("led_state", e.data);
  var inputChecked;
  if( e.data == 1){ inputChecked = true; }
  else { inputChecked = false; }
  document.getElementById("led").checked = inputChecked;
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("humi").innerHTML = e.data;
 }, false);

}</script>
</body>
</html>
)rawliteral";

String outputState(int gpio){
  if(digitalRead(gpio)){
    return "checked";
  }
  else {
    return "";
  }
}
String pumpState(int gpio) {
  return digitalRead(gpio) ? "ON" : "OFF";
}

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

String processor(const String& var){
  if(var == "BUTTONPLACEHOLDER"){
    String buttons;
    // Generate HTML for the OUTPUT control
    String outputStateValue = outputState(output); // Get the state of the output pin
    buttons += "<div class=\"card card-switch\"><h4>FANS</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"controlOutput(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label></div>";

    // Generate HTML for the STATUS LED control
    String ledStateValue = outputState(ledPin); // Get the state of the STATUS LED
    buttons += "<div class=\"card card-switch\"><h4>LIGHTS</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"controlOutput(this)\" id=\"status-led\" " + ledStateValue + "><span class=\"slider\"></span></label></div>";

    // Handle water pump and fertilizer pump with specific pump control
    // Generate HTML for the WATER PUMP control
    String waterPumpStateValue = outputState(waterPumpPin); // Get the state of the WATER PUMP
    buttons += "<div class=\"card card-switch\"><h4>WATER PUMP</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"controlOutput(this)\" id=\"toggle-water\" " + waterPumpStateValue + "><span class=\"slider\"></span></label></div>";
 // Generate HTML for the FERTILIZER PUMP control
    String fertilizerPumpStateValue = outputState(fertilizerPumpPin); // Get the state of the FERTILIZER PUMP
    buttons += "<div class=\"card card-switch\"><h4>FERTILIZER PUMP</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"controlOutput(this)\" id=\"toggle-fertilizer\" " + fertilizerPumpStateValue + "><span class=\"slider\"></span></label></div>";

    
    return buttons;
  }
  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
    
  // if (!bme.begin(0x76)) {
  //   Serial.println("Could not find a valid BME280 sensor, check wiring!");
  //   while (1);
  // }
  
  // initialize the pushbutton pin as an input
  pinMode(buttonPin, INPUT);
  // initialize the LED pin as an output
  pinMode(ledPin, OUTPUT);
  // initialize the LED pin as an output
  pinMode(output, OUTPUT);
  // PIR Motion Sensor mode INPUT_PULLUP
  pinMode(motionSensor, INPUT_PULLUP);
  pinMode(waterPumpPin, OUTPUT);
pinMode(fertilizerPumpPin, OUTPUT);

 // Setup PWM for the lightPin
  ledcSetup(lightPwmChannel, lightFreq, lightResolution);
  ledcAttachPin(lightPwmPin, lightPwmChannel);

    // Setup PWM for the fanPin
  ledcSetup(fanPwmChannel, fanFreq, fanResolution);
  ledcAttachPin(fanPwmPin, fanPwmChannel);

  // Set motionSensor pin as interrupt, assign interrupt function and set RISING mode
  // attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);
    WiFi.mode(WIFI_AP_STA);
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
   if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });
// Send a GET request to toggle output <ESP_IP>/output?state=<inputMessage>
server.on("/output", HTTP_GET, [] (AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  if (request->hasParam(PARAM_INPUT_1)) {
    int state = request->getParam(PARAM_INPUT_1)->value().toInt();
    digitalWrite(output, state);
    request->send(200, "text/plain", state ? "Output ON" : "Output OFF");
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
});
  // Send a GET request to control on board status LED <ESP_IP>/toggle
  server.on("/toggle", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    request->send(200, "text/plain", "OK");
  });
  // Send a GET request to toggle status LED <ESP_IP>/toggle-led?state=<inputMessage>
server.on("/toggle-led", HTTP_GET, [] (AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  if (request->hasParam(PARAM_INPUT_1)) {
    int state = request->getParam(PARAM_INPUT_1)->value().toInt();
    digitalWrite(ledPin, state);
    request->send(200, "text/plain", state ? "LED ON" : "LED OFF");
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
});

// Toggle water pump
server.on("/toggle-water", HTTP_GET, [] (AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  digitalWrite(waterPumpPin, !digitalRead(waterPumpPin)); // Toggle the water pump state
  request->send(200, "text/plain", digitalRead(waterPumpPin) ? "ON" : "OFF");
});

 server.on("/adjust-fan", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    if (request->hasParam("speed")) {
      int speed = request->getParam("speed")->value().toInt();
      ledcWrite(fanPwmChannel, speed);
      request->send(200, "text/plain", "Fan speed set to " + String(speed));
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

// Route for adjusting light intensity
server.on("/adjust-light", HTTP_GET, [] (AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  if (request->hasParam("intensity")) {
    int intensity = request->getParam("intensity")->value().toInt();
    // Map the intensity to the PWM range and write it
    ledcWrite(lightPwmChannel, intensity);
    request->send(200, "text/plain", "Light intensity set to " + String(intensity));
  } else {
    request->send(400, "text/plain", "Bad Request");
  }
});


// Toggle fertilizer pump
server.on("/toggle-fertilizer", HTTP_GET, [] (AsyncWebServerRequest *request) {
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  digitalWrite(fertilizerPumpPin, !digitalRead(fertilizerPumpPin)); // Toggle the fertilizer pump state
  request->send(200, "text/plain", digitalRead(fertilizerPumpPin) ? "ON" : "OFF");
});
  // Send a GET request to clear the "Motion Detected" message <ESP_IP>/clear-motion
  // server.on("/clear-motion", HTTP_GET, [] (AsyncWebServerRequest *request) {
  //   if(!request->authenticate(http_username, http_password))
  //     return request->requestAuthentication();
  //   clearMotionAlert = true;
  //   request->send(200, "text/plain", "OK");
  // });
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis and set reconnect delay to 1 second
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);
  
  // Start server
  server.begin();
}

 
void loop(){
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 10000;
  // read the state of the switch into a local variable
  int reading = digitalRead(buttonPin);

  // If the switch changed
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;
      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        events.send(String(digitalRead(ledPin)).c_str(),"led_state",millis());
      }
    }
  }

  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    events.send(String(37.5).c_str(),"temperature",millis());
    events.send(String(85).c_str(),"humidity",millis());
    events.send(String(64).c_str(),"light",millis());
    lastEventTime = millis();
  }
  

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState = reading;
}
