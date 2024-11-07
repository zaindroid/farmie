

// Import required libraries
#include "WiFi.h"
#include <Preferences.h>
#include "ESPAsyncWebServer.h"
// #include <Adafruit_BME280.h>
// #include <Adafruit_Sensor>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <Wire.h>
#include <Ezo_i2c.h>
#include <Wire.h>
#include <Ezo_i2c_util.h>
#include <sequencer2.h>
Preferences preferences;
Sequencer2 sensorSequencer(&step1, 1000, &step2, 0); // Setup sequencer with two steps
// Global variable to track mode
bool isAutoMode = true; // Default to Auto mode

Ezo_board HUM = Ezo_board(111, "HUM"); // Initialize the humidity sensor with its I2C address

char sensorData[32]; // Buffer to hold sensor data
float humidity = 0.0;
float temperature = 0.0;


// #define I2C_SDA 26
// #define I2C_SCL 27
RTC_DS3231 rtc;

char sensorDataBuffer[32]; // Buffer to store sensor data

// Step 1: Send the command to read from the sensor
void step1() {
    HUM.send_cmd("R");
}

// Step 2: Read and parse the sensor response
void step2() {
    HUM.receive_cmd(sensorDataBuffer, sizeof(sensorDataBuffer));
     char* humidityStr = strtok(sensorDataBuffer, ",");
    char* temperatureStr = strtok(NULL, ",");

    if (humidityStr != NULL && temperatureStr != NULL) {
        humidity = atof(humidityStr);
        temperature = atof(temperatureStr);
    }
}
void setupRTC() {
  Wire.begin();
  
  // Start the NVS (Non-volatile storage)
  preferences.begin("rtc", false);
  
  // Check if the RTC is working
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  
  // Check if the RTC has lost power or if the time has not been set previously
  bool isRTCSet = preferences.getBool("rtcSet", false);
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, check the battery!");
    
    // Set time only if it’s the first time after power loss
    if (!isRTCSet) {
      Serial.println("First time setting RTC time.");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      preferences.putBool("rtcSet", true); // Mark the RTC as set
    }
  } else if (!isRTCSet) {
    // RTC did not lose power, but it's the first time setting the time
    Serial.println("First time setting RTC time (not due to power loss).");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    preferences.putBool("rtcSet", true); // Mark the RTC as set
  } else {
    // If RTC time is already set and it hasn't lost power, just continue
    Serial.println("RTC is running with correct time.");
  }
  
  preferences.end();
}



String getFormattedTime() {
  DateTime now = rtc.now();
  char buf[] = "YYYY-MM-DD HH:MM:SS";
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(buf);
}
// Global variables to store alarm IDs
AlarmId AlarmIdOn;
AlarmId AlarmIdOff;

// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

// Replace with your network credentials
// const char* ssid = "Rivendell";
// const char* password = "Fireon1122";

// const char *ssid = "Redmi 9A";
// const char *password = "fireon11";  // You can set a password for your AP

const char *ssid = "Farmie";
const char *password = "farmie2.0";  // You can set a password for your AP

// Web Server HTTP Authentication credentials
const char* http_username = "farmie";
const char* http_password = "farmie2.0";

// Adafruit_BME280 bme;         // BME280 connect to ESP32 I2C (GPIO 21 = SDA, GPIO 22 = SCL)
const int buttonPin = 32;    // Pushbutton
const int ledPin = 4;       // Status LED
const int output = 5;       // Output socket
const int ldr = 33;          // LDR (Light Dependent Resistor)
// const int motionSensor = 27; // PIR Motion Sensor
const int waterPumpPin = 13;     // Water pump connected to GPIO 5
const int fertilizerPumpPin = 12; // Fertilizer pump connected to GPIO 21

// Define the PWM channel for the light
const int lightPwmChannel = 2; // Ensure this is a unique channel not used by other PWM devices
const int lightFreq = 5000; // 5kHz frequency is typical for LEDs
const int lightResolution = 8; // 8-bit resolution
const int lightPwmPin = 22; // LED connected to GPIO 23

// Define the PWM channel for the fan
const int fanPwmChannel = 1;
const int fanFreq = 25000; // 25kHz frequency typically used for fans
const int fanResolution = 8; // 8-bit resolution
const int fanPwmPin = 23; // Fan connected to GPIO 22

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
  .topnav {overflow: hidden; background-color: #1b78e2; color: white;display: flex;justify-content: space-between; align-items: center; padding: 0 20px;}
  .topnav h3 {flex-grow: 1;text-align: center;margin: 0;}
.time-display {font-size: 1.2rem;font-weight: bold;  margin-right: 15px; /* additional styling to match the theme */}

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
    .icon-pointer{ cursor: pointer; padding-left: 15px; }

     .mode-switch-container {
    display: flex;
    justify-content: center;
    align-items: center;
    margin-bottom: 30px; /* Adjust this value for more or less spacing */
    margin-top: 20px; /* Optional: add some space above the switch */
  }

  .mode-switch-container span {
    font-size: 1.2rem;
    font-weight: bold;
    margin: 0 10px; /* Space between the text and the switch */
    color: #333;
  }

  .switch {
    position: relative;
    display: inline-block;
    width: 100px;
    height: 40px;
  }

  .switch input { display: none; }

  .slider {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: #ccc;
    border-radius: 20px;
    cursor: pointer;
    transition: 0.4s;
  }

  .slider:before {
    position: absolute;
    content: "";
    height: 32px;
    width: 32px;
    left: 4px;
    bottom: 4px;
    background-color: white;
    border-radius: 50%;
    transition: 0.4s;
  }

  input:checked + .slider {
    background-color: #1b78e2; /* Default blue for auto mode */
    box-shadow: 0 0 20px 10px rgba(0, 0, 255, 0.5); /* Blue glow effect */
    animation: blue-glow 1.5s infinite alternate; /* Animation for blue glow */
  }

  input:not(:checked) + .slider {
    background-color: #ff3b3b; /* Red for manual mode */
    box-shadow: 0 0 20px 10px rgba(255, 0, 0, 0.5); /* Red glow effect */
    animation: red-glow 1.5s infinite alternate; /* Animation for red glow */
  }

  input:checked + .slider:before {
    transform: translateX(60px); /* Adjust based on width of switch */
  }

  /* Keyframes for blue glow */
  @keyframes blue-glow {
    from { box-shadow: 0 0 10px 5px rgba(0, 0, 255, 0.5); }
    to { box-shadow: 0 0 20px 10px rgba(0, 0, 255, 0.9); }
  }

  /* Keyframes for red glow */
  @keyframes red-glow {
    from { box-shadow: 0 0 10px 5px rgba(255, 0, 0, 0.5); }
    to { box-shadow: 0 0 20px 10px rgba(255, 0, 0, 0.9); }
  }

  /* Rounded switch */
  .slider.round {
    border-radius: 20px;
  }

  .slider.round:before {
    border-radius: 50%;
  }
  </style>
</head>
<body>
<div class="topnav">
  <h3>FARMIE DASHBOARD</h3>
  <span id="current-time" class="time-display">22:53:39</span>
  <i class="fas fa-user-slash icon-pointer" onclick="logoutButton()"></i>
</div>

  <div class="content">
 <div class="mode-switch-container">
  <span>Manual</span>
  <label class="switch">
    <input type="checkbox" id="modeToggle" onchange="toggleMode(this)">
    <span class="slider round"></span>
  </label>
  <span>Auto</span>
</div>
    <div class="cards">
      %BUTTONPLACEHOLDER%


      <div class="card card-bme">
        <h4><i class="fas fa-chart-bar"></i> TEMPERATURE</h4><div><p class="reading"><span id="temp"></span>&deg;C</p></div>
      </div>
      <div class="card card-bme">
        <h4><i class="fas fa-chart-bar"></i> HUMIDITY</h4><div><p class="reading"><span id="humi"></span>&percnt;</p></div>
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

function toggleMode(element) {
  var mode = element.checked ? "auto" : "manual";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/mode-toggle?mode=" + mode, true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4 && xhr.status === 200) {
      console.log("Mode changed to: " + mode);
    }
  };
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

   source.addEventListener('time', function(e) {
    document.getElementById('current-time').innerHTML = e.data;
  }, false);
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 source.addEventListener('time', function(event) {
  document.getElementById('time-display').innerHTML = event.data;
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

void turnOnFan();
void turnOffFan();
void turnOnLight();
void turnOffLight();
void turnOnFertilizerPump();
void turnOffFertilizerPump();
void turnOnWaterPump();
void turnOffWaterPump();
void setSchedules();
void Repeats2() {
  Serial.println("2 second timer");
}
void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
    setupRTC();
      preferences.end();
   DateTime now = rtc.now();
  setTime(now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year() - 2000); // Adjust year
  // setTime(14, 53, 00, 27, 2, 4024 - 2000); // Adjust year
  

  setSchedules();
  // initialize the pushbutton pin as an input
  pinMode(buttonPin, INPUT);
  // initialize the LED pin as an output
  pinMode(ledPin, OUTPUT);
  // initialize the LED pin as an output
  pinMode(output, OUTPUT);
  // PIR Motion Sensor mode INPUT_PULLUP
  // pinMode(motionSensor, INPUT_PULLUP);
  pinMode(waterPumpPin, OUTPUT);
pinMode(fertilizerPumpPin, OUTPUT);
 digitalWrite(fertilizerPumpPin,LOW);

 // Setup PWM for the lightPin
  ledcSetup(lightPwmChannel, lightFreq, lightResolution);
  ledcAttachPin(lightPwmPin, lightPwmChannel);

    // Setup PWM for the fanPin
  ledcSetup(fanPwmChannel, fanFreq, fanResolution);
  ledcAttachPin(fanPwmPin, fanPwmChannel);

  // Set motionSensor pin as interrupt, assign interrupt function and set RISING mode
  // attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);
  //   WiFi.mode(WIFI_AP_STA);
  // // Connect to Wi-Fi
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000);
  //   Serial.println("Connecting to WiFi..");
  // }

   WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());


  //  timeClient.begin();

  //   // Update the NTP client to set the system time
  // timeClient.update();
  // setTime(timeClient.getEpochTime());
  // Print ESP32 Local IP Address
  // Serial.println(WiFi.localIP());

  // Atlas Scientific sensor initialization
HUM.send_cmd("I2C,112");
delay(300);
HUM.send_cmd("O,T,1"); // Enable temperature output
delay(300);


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

// Endpoint to toggle mode
server.on("/mode-toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (request->hasParam("mode")) {
    String mode = request->getParam("mode")->value();
    isAutoMode = (mode == "auto");
    Serial.print("Mode changed to: ");
    Serial.println(isAutoMode ? "Auto" : "Manual");
    request->send(200, "text/plain", isAutoMode ? "Switched to Auto Mode" : "Switched to Manual Mode");
  } else {
    request->send(400, "text/plain", "Bad Request: mode parameter is missing");
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

 void sendSensorData() {
    static unsigned long lastSendTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime > 1000) { // Throttle data sending to once per second
        String sensorData = "Humidity: " + String(humidity) + ", Temperature: " + String(temperature);
        events.send(sensorData.c_str(), "sensor-update", currentTime);
        lastSendTime = currentTime;
    }


    events.send("ping",NULL,millis());
    events.send(String(temperature).c_str(),"temperature",millis());
    events.send(String(humidity).c_str(),"humidity",millis());
    events.send(String(64).c_str(),"light",millis());
}

// Peripheral control variables

void manageSchedules(const DateTime& now);
void printPeripheralStatuses();
unsigned long lastStatusPrintTime = 0; // Tracks the last time statuses were printed
const unsigned long statusPrintInterval = 60000; // Interval to print status (60000 ms = 1 minute)
bool isFanOn = false;
bool areLightsOn = false;
bool isWaterPumpOn = false;
bool isFertilizerPumpOn = false;

void loop(){
  Alarm.delay(100);
DateTime now = rtc.now();
sensorSequencer.run();
  static unsigned long lastTimeEvent = 0;
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastTimeEvent > 1000) { // Update every second
    String formattedTime = getFormattedTime();
    events.send(formattedTime.c_str(), "time", millis());
    lastTimeEvent = currentTime;
  }
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  // read the state of the switch into a local variable
  // int reading = digitalRead(buttonPin);

  // // If the switch changed
  // if (reading != lastButtonState) {
  //   // reset the debouncing timer
  //   lastDebounceTime = millis();
  // }

  // if ((millis() - lastDebounceTime) > debounceDelay) {
  //   // if the button state has changed:
  //   if (reading != buttonState) {
  //     buttonState = reading;
  //     // only toggle the LED if the new button state is HIGH
  //     if (buttonState == HIGH) {
  //       ledState = !ledState;
  //       digitalWrite(ledPin, ledState);
  //       events.send(String(digitalRead(ledPin)).c_str(),"led_state",millis());
  //     }
  //   }
  // }
   manageSchedules(now);
   printPeripheralStatuses();

  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    events.send(String(37.5).c_str(),"temperature",millis());
    events.send(String(85).c_str(),"humidity",millis());
    events.send(String(64).c_str(),"light",millis());
    sendSensorData();
    lastEventTime = millis();
  }


  

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  // lastButtonState = reading;
}
void manageSchedules(const DateTime& now) {
  // LED Light Schedule
    if (!isAutoMode) return; // Skip schedules in Manual mode
  
  if ((now.hour() == 6 && now.minute() >= 25) || (now.hour() > 6 && now.hour() < 24)) {
    turnOnLight();
  } else if (now.hour() == 0) {
    turnOffLight();
  }

  // Water Pump Schedule
    if ((now.hour() == 2 && now.minute() >= 30 && now.minute() < 43) || 
      (now.hour() == 8 && now.minute() >= 30 && now.minute() < 43)|| 
      (now.hour() == 14 && now.minute() >= 30 && now.minute() < 43)|| 
      (now.hour() == 20 && now.minute() >= 30 && now.minute() < 43)) {
    turnOnWaterPump();
  }
  
  // Note: The turnOffWaterPump is called automatically after 780 seconds by the timer

  // Fertilizer Pump Schedule (assuming Monday is day 1)
  if (now.dayOfTheWeek() == 1 && now.hour() == 8 && now.minute() == 0 && now.second() < 15) {
    turnOnFertilizerPump();
  }
  // Note: The turnOffFertilizerPump is called automatically after 15 seconds by the timer
    // Fan Control Logic
    turnOnFan();
  if (digitalRead(ledPin) == LOW) { // LED is off
    ledcWrite(lightPwmChannel, 25); // Dim fan to 10% (approx. 25/255)
  }
  
  // if (temperature > 25.0) {
  //   ledcWrite(fanPwmChannel, 255); // Increase fan to 100%
  // } else {
  //   ledcWrite(fanPwmChannel, 128); // Normal operation at 50% (approx. 128/255)
  // }

}
void printPeripheralStatuses() {
  unsigned long currentTime = millis();
  if (currentTime - lastStatusPrintTime >= statusPrintInterval) {
    // It's been at least a minute
    Serial.println("Peripheral Statuses:");
    Serial.println("----------------------------");
    Serial.print("Fan: ");
    Serial.println(isFanOn ? "On" : "Off");
    Serial.print("Lights: ");
    Serial.println(areLightsOn ? "On" : "Off");
    Serial.print("Water Pump: ");
    Serial.println(isWaterPumpOn ? "On" : "Off");
    Serial.print("Fertilizer Pump: ");
    Serial.println(isFertilizerPumpOn ? "On" : "Off");
    Serial.println("----------------------------\n");

    // Update the last print time
    lastStatusPrintTime = currentTime;
  }
}

