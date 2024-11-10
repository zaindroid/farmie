
// Import required libraries
#include "WiFi.h"
#include <Preferences.h>
#include "ESPAsyncWebServer.h"
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
#include <LittleFS.h>

// Function declarations
void appendSchedule(fs::FS &fs, const char *path, const char *message);
void processSchedules(String filePath, const DateTime& now, void (*turnOn)(), void (*turnOff)());

// Function declarations
String readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);

// Other function declarations...

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
  if (rtc.lostPower() || !isRTCSet) {
    Serial.println("RTC lost power, check the battery or set the correct time!");
    // Only adjust the RTC if it's the first time or if it has truly lost power
    if (!isRTCSet) {
      // Set the RTC to the compile-time values if time was never set
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      preferences.putBool("rtcSet", true);
    }
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

<div class="card">
  <h4>Schedule Settings</h4>
  <form id="scheduleForm">
    <label for="deviceSelect">Select Device:</label>
    <select id="deviceSelect" name="device" onchange="showFields()">
      <option value="light">Light</option>
      <option value="fan">Fan</option>
      <option value="waterPump1">Water Pump Schedule 1</option>
      <option value="waterPump2">Water Pump Schedule 2</option>
      <option value="waterPump3">Water Pump Schedule 3</option>
      <option value="fertilizerPump">Fertilizer Pump</option>
    </select><br><br>

    <!-- Light input fields -->
    <div id="lightFields" class="input-fields">
      <label for="lightOnHour">On Hour:</label>
      <input type="number" id="lightOnHour" name="onHour" min="0" max="23"><br>
      <label for="lightOnMinute">On Minute:</label>
      <input type="number" id="lightOnMinute" name="onMinute" min="0" max="59"><br>
      <label for="lightOffHour">Off Hour:</label>
      <input type="number" id="lightOffHour" name="offHour" min="0" max="23"><br>
      <label for="lightOffMinute">Off Minute:</label>
      <input type="number" id="lightOffMinute" name="offMinute" min="0" max="59"><br>
    </div>

    <!-- Fan input fields -->
    <div id="fanFields" class="input-fields">
      <label for="fanOnHour">On Hour:</label>
      <input type="number" id="fanOnHour" name="onHour" min="0" max="23"><br>
      <label for="fanOnMinute">On Minute:</label>
      <input type="number" id="fanOnMinute" name="onMinute" min="0" max="59"><br>
      <label for="fanOffHour">Off Hour:</label>
      <input type="number" id="fanOffHour" name="offHour" min="0" max="23"><br>
      <label for="fanOffMinute">Off Minute:</label>
      <input type="number" id="fanOffMinute" name="offMinute" min="0" max="59"><br>
    </div>

<!-- Water Pump input fields -->
<div id="waterPumpFields" class="input-fields">
  <label for="waterOnHour">On Hour:</label>
  <input type="number" id="waterOnHour" name="onHour" min="0" max="23"><br>
  <label for="waterOnMinute">On Minute:</label>
  <input type="number" id="waterOnMinute" name="onMinute" min="0" max="59"><br>
  <label for="waterDuration">Duration (minutes):</label>
  <input type="number" id="waterDuration" name="duration" min="1"><br>
</div>


    <!-- Fertilizer Pump input fields -->
    <div id="fertilizerFields" class="input-fields">
      <label for="fertilizerDay">Day:</label>
      <input type="number" id="fertilizerDay" name="day" min="1" max="7"><br>
      <label for="fertilizerOnHour">On Hour:</label>
      <input type="number" id="fertilizerOnHour" name="onHour" min="0" max="23"><br>
      <label for="fertilizerOnMinute">On Minute:</label>
      <input type="number" id="fertilizerOnMinute" name="onMinute" min="0" max="59"><br>
      <label for="fertilizerDuration">Duration (seconds):</label>
      <input type="number" id="fertilizerDuration" name="duration" min="1"><br>
    </div>

    <input type="button" value="Save Schedule" onclick="saveSchedule()">
  </form>
  <div id="scheduleDisplay">
    <h4>Current Schedule:</h4>
    <p id="currentSchedule">No schedule selected</p>
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

function showFields() {
  document.querySelectorAll('.input-fields').forEach(function(field) {
    field.style.display = 'none';
  });
  var device = document.getElementById("deviceSelect").value;
  if (device.includes("waterPump")) {
    document.getElementById("waterPumpFields").style.display = 'block';
  } else if (device === "fertilizerPump") {
    document.getElementById("fertilizerFields").style.display = 'block';
  } else {
    document.getElementById(device + "Fields").style.display = 'block';
  }
}

function saveSchedule() {
  var device = document.getElementById("deviceSelect").value;
  var params = `device=${device}`;

  if (device === 'light' || device === 'fan') {
    params += `&onHour=${document.getElementById(device + "OnHour").value}`;
    params += `&onMinute=${document.getElementById(device + "OnMinute").value}`;
    params += `&offHour=${document.getElementById(device + "OffHour").value}`;
    params += `&offMinute=${document.getElementById(device + "OffMinute").value}`;
  } else if (device.includes('waterPump')) {
    params += `&onHour=${document.getElementById("waterOnHour").value}`;
    params += `&onMinute=${document.getElementById("waterOnMinute").value}`;
    params += `&duration=${document.getElementById("waterDuration").value}`;
  } else if (device === 'fertilizerPump') {
    params += `&day=${document.getElementById("fertilizerDay").value}`;
    params += `&onHour=${document.getElementById("fertilizerOnHour").value}`;
    params += `&onMinute=${document.getElementById("fertilizerOnMinute").value}`;
    params += `&duration=${document.getElementById("fertilizerDuration").value}`;
  }

  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/set-schedule?" + params, true);
  xhr.onload = function() {
    if (xhr.status === 200) {
      alert("Schedule saved!");
      loadSchedule();
    } else {
      alert("Failed to save schedule: " + xhr.responseText);
    }
  };
  xhr.send();
}

function loadSchedule() {
  var device = document.getElementById("deviceSelect").value;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", `/get-schedule?device=${device}`, true);
  xhr.onload = function() {
    if (xhr.status === 200) {
      var schedule = xhr.responseText.split(',');

      if (device.startsWith("waterPump")) {
        // Display the schedule for water pumps
        if (schedule.length === 3) {
          document.getElementById("currentSchedule").innerText = `On: ${schedule[0]}:${schedule[1]}, Duration: ${schedule[2]} minutes`;
        } else {
          document.getElementById("currentSchedule").innerText = "No schedule found for water pump.";
        }
      } else if (device === "fertilizerPump") {
        // Display the schedule for fertilizer pump
        if (schedule.length === 4) {
          document.getElementById("currentSchedule").innerText = `Day: ${schedule[0]}, On: ${schedule[1]}:${schedule[2]}, Duration: ${schedule[3]} minutes`;
        } else {
          document.getElementById("currentSchedule").innerText = "No schedule found for fertilizer pump.";
        }
      } else {
        // Display the schedule for light or fan
        if (schedule.length === 4) {
          document.getElementById("currentSchedule").innerText = `On: ${schedule[0]}:${schedule[1]}, Off: ${schedule[2]}:${schedule[3]}`;
        } else {
          document.getElementById("currentSchedule").innerText = "No schedule found for light/fan.";
        }
      }
    } else {
      document.getElementById("currentSchedule").innerText = "No schedule found.";
    }
  };
  xhr.send();
}


window.onload = function() {
  loadSchedule();
  showFields(); // Ensure the correct input fields are displayed based on the initial selection
};

// Load the schedule whenever the dropdown selection changes
document.getElementById("deviceSelect").addEventListener("change", function() {
  loadSchedule(); // Load schedule for the newly selected device
  showFields(); // Display the appropriate input fields
});

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

void clearOldSchedules() {
  LittleFS.remove("/waterPump1_schedule.txt");
  LittleFS.remove("/waterPump2_schedule.txt");
  LittleFS.remove("/waterPump3_schedule.txt");
  LittleFS.remove("/fan_schedule.txt");
  LittleFS.remove("/light_schedule.txt");
  LittleFS.remove("/fertilizerPump_schedule.txt");
  Serial.println("Cleared old schedule files.");
}
  


void turnOnFan();
void turnOffFan();
void turnOnLight();
void turnOffLight();
void turnOnFertilizerPump();
void turnOffFertilizerPump();
void turnOnWaterPump();
void turnOffWaterPump();
// void setSchedules();
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
  
    preferences.begin("code_flag", false); // Open the preferences storage
  bool codeUploaded = preferences.getBool("codeUploaded", false);

  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Check if this is the first run after code upload
  // Check if this is the first run after code upload
  if (!codeUploaded) {
    clearOldSchedules(); // Clear old schedules only when new code is uploaded
    preferences.putBool("codeUploaded", true); // Set the flag to true after clearing
    Serial.println("Flag set to indicate code upload.");
  } else {
    Serial.println("Retaining schedules on reset.");
  }

  // Ensure all schedule files exist with default content
  if (!LittleFS.exists("/light_schedule.txt")) {
    writeFile(LittleFS, "/light_schedule.txt", "0,0,0,0");
  }
  if (!LittleFS.exists("/fan_schedule.txt")) {
    writeFile(LittleFS, "/fan_schedule.txt", "0,0,0,0");
  }
  if (!LittleFS.exists("/waterPump1_schedule.txt")) {
    writeFile(LittleFS, "/waterPump1_schedule.txt", "0,0,0");
  }
  if (!LittleFS.exists("/waterPump2_schedule.txt")) {
    writeFile(LittleFS, "/waterPump2_schedule.txt", "0,0,0");
  }
  if (!LittleFS.exists("/waterPump3_schedule.txt")) {
    writeFile(LittleFS, "/waterPump3_schedule.txt", "0,0,0");
  }
  if (!LittleFS.exists("/fertilizerPump_schedule.txt")) {
    writeFile(LittleFS, "/fertilizerPump_schedule.txt", "0,0,0,0");
  }

  preferences.end(); // Close the preferences storage

  // setSchedules();

  // // Initialize LittleFS and format it if necessary
  // if (!LittleFS.begin()) {
  //   Serial.println("An error has occurred while mounting LittleFS. Formatting...");
  //   LittleFS.format(); // Format LittleFS to clear all existing data when new code is uploaded
  //   if (!LittleFS.begin()) {
  //     Serial.println("Failed to mount LittleFS after formatting.");
  //     return;
  //   }
  // } else {
  //   // Uncomment the line below if you always want to format LittleFS on each upload (optional)
  //   // LittleFS.format();
  //   Serial.println("LittleFS mounted successfully");
  // }
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

server.on("/set-schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (request->hasParam("device")) {
    String device = request->getParam("device")->value();
    String scheduleData;
    String filePath = "/" + device + "_schedule.txt";

    // Collect parameters based on the device type
    if (device == "light" || device == "fan") {
      if (request->hasParam("onHour") && request->hasParam("onMinute") && request->hasParam("offHour") && request->hasParam("offMinute")) {
        int onHour = request->getParam("onHour")->value().toInt();
        int onMinute = request->getParam("onMinute")->value().toInt();
        int offHour = request->getParam("offHour")->value().toInt();
        int offMinute = request->getParam("offMinute")->value().toInt();
        scheduleData = String(onHour) + "," + String(onMinute) + "," + String(offHour) + "," + String(offMinute);
      }
    }  else if (device.startsWith("waterPump")) {
      if (request->hasParam("onHour") && request->hasParam("onMinute") && request->hasParam("duration")) {
        int onHour = request->getParam("onHour")->value().toInt();
        int onMinute = request->getParam("onMinute")->value().toInt();
        int durationInMinutes = request->getParam("duration")->value().toInt();
        scheduleData = String(onHour) + "," + String(onMinute) + "," + String(durationInMinutes); // Store directly in minutes
      }
    }
    else if (device == "fertilizerPump") {
      if (request->hasParam("day") && request->hasParam("onHour") && request->hasParam("onMinute") && request->hasParam("duration")) {
        int day = request->getParam("day")->value().toInt();
        int onHour = request->getParam("onHour")->value().toInt();
        int onMinute = request->getParam("onMinute")->value().toInt();
        int duration = request->getParam("duration")->value().toInt();
        scheduleData = String(day) + "," + String(onHour) + "," + String(onMinute) + "," + String(duration);
      }
    }

    if (scheduleData.length() > 0) {
      writeFile(LittleFS, filePath.c_str(), scheduleData.c_str());
      Serial.println("Schedule saved for " + device + ": " + scheduleData); // Debug log
      request->send(200, "text/plain", "Schedule saved for " + device);
    } else {
      Serial.println("Failed to save schedule: Missing parameters."); // Debug log
      request->send(400, "text/plain", "Bad Request: Missing parameters.");
    }
  } else {
    Serial.println("Failed to save schedule: Missing device parameter."); // Debug log
    request->send(400, "text/plain", "Bad Request: Missing device parameter.");
  }
});



server.on("/get-schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (request->hasParam("device")) {
    String device = request->getParam("device")->value();
    String filePath = "/" + device + "_schedule.txt";
    String schedule = readFile(LittleFS, filePath.c_str());

    if (schedule.length() > 0) {
      Serial.println("Schedule retrieved: " + schedule); // Debug log
      request->send(200, "text/plain", schedule);
    } else {
      Serial.println("No schedule found for " + device); // Debug log
      request->send(404, "text/plain", "No schedule found for " + device);
    }
  } else {
    request->send(400, "text/plain", "Bad Request: missing device parameter");
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



}
   // Append schedule data to a file in LittleFS
void appendSchedule(fs::FS &fs, const char *path, const char *message) {
  File file = fs.open(path, "a");
  if (!file) {
    Serial.println("- failed to open file for appending");
    return;
  }
  file.println(message);
  file.close();
}

void processSchedules(String filePath, const DateTime& now, void (*turnOn)(), void (*turnOff)()) {
  String schedule = readFile(LittleFS, filePath.c_str());

  if (schedule.length() > 0) {
    Serial.println("Processing schedule: " + schedule);

    int onHour, onMinute, offHour, offMinute, duration;
    if (filePath.endsWith("light_schedule.txt") || filePath.endsWith("fan_schedule.txt")) {
      sscanf(schedule.c_str(), "%d,%d,%d,%d", &onHour, &onMinute, &offHour, &offMinute);
      Serial.printf("Parsed schedule for %s: On - %02d:%02d, Off - %02d:%02d\n", filePath.c_str(), onHour, onMinute, offHour, offMinute);
      Serial.printf("Current time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

      // Check if current time is within the active period
      bool isActive = (now.hour() > onHour || (now.hour() == onHour && now.minute() >= onMinute)) &&
                      (now.hour() < offHour || (now.hour() == offHour && now.minute() < offMinute));

      if (isActive) {
        Serial.println("Within active period: Turning on the device.");
        turnOn();
      } else if (now.hour() == offHour && now.minute() == offMinute) {
        Serial.println("Reached off time: Turning off the device.");
        turnOff();
      } else {
        Serial.println("Outside active period for lights/fan.");
      }
    } else if (filePath.startsWith("/waterPump")) {
      sscanf(schedule.c_str(), "%d,%d,%d", &onHour, &onMinute, &duration);
      Serial.printf("Parsed schedule for %s: On - %02d:%02d, Duration - %d minutes\n", filePath.c_str(), onHour, onMinute, duration);
      Serial.printf("Current time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

      if (now.hour() == onHour && now.minute() == onMinute) {
        Serial.println("Within active period: Turning on the water pump.");
        turnOn();
        Alarm.timerOnce(duration * 60, turnOff); // Convert minutes to seconds here
      } else {
        Serial.println("Outside active period for water pump.");
      }
    }else if (filePath.endsWith("fertilizerPump_schedule.txt")) {
      int day;
      sscanf(schedule.c_str(), "%d,%d,%d,%d", &day, &onHour, &onMinute, &duration);
      Serial.printf("Parsed schedule for fertilizer pump: Day - %d, On - %02d:%02d, Duration - %d seconds\n", day, onHour, onMinute, duration);
      Serial.printf("Current time: Day - %d, Time - %02d:%02d:%02d\n", now.dayOfTheWeek(), now.hour(), now.minute(), now.second());

      if (now.dayOfTheWeek() == day && now.hour() == onHour && now.minute() == onMinute) {
        Serial.println("Turning on the fertilizer pump.");
        turnOn();
        Serial.printf("Setting timer to turn off after %d seconds.\n", duration);
        Alarm.timerOnce(duration, turnOff);
      } else {
        Serial.println("Outside active period for fertilizer pump.");
      }
    }
  } else {
    Serial.println("No schedule found or failed to read.");
  }
}


// Modify the manageSchedules function to handle multiple schedules
void manageSchedules(const DateTime& now) {
    if (!isAutoMode) {
        return; // Skip schedules in manual mode
    }

    processSchedules("/fan_schedule.txt", now, turnOnFan, turnOffFan);
    processSchedules("/light_schedule.txt", now, turnOnLight, turnOffLight);
    
    // Process all water pump schedules
    processSchedules("/waterPump1_schedule.txt", now, turnOnWaterPump, turnOffWaterPump);
    processSchedules("/waterPump2_schedule.txt", now, turnOnWaterPump, turnOffWaterPump);
    processSchedules("/waterPump3_schedule.txt", now, turnOnWaterPump, turnOffWaterPump);

    processSchedules("/fertilizerPump_schedule.txt", now, turnOnFertilizerPump, turnOffFertilizerPump);
}




// Print the statuses of the peripherals
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


void turnOnFan() {
  // Code to turn on the fan
  // ledcWrite(fanPwmChannel, 255);
  digitalWrite(output,HIGH);
    isFanOn = true;
  // Serial.println("Fan turned on");
}

void turnOffFan() {
  // Code to turn off the fan
    ledcWrite(fanPwmChannel, 0); // Turn off the fan
  digitalWrite(output,LOW);
  // Serial.println("Fan turned off");
  isFanOn = false;
  
}
void turnOnLight(){
  digitalWrite(ledPin,HIGH);
  // Serial.println("Lights turned on");
    areLightsOn = true;

}
void turnOffLight(){
    digitalWrite(ledPin,LOW);
  // Serial.println("Lights turned off");
  areLightsOn = false;
}
void turnOnFertilizerPump(){
  digitalWrite(fertilizerPumpPin,HIGH);
  isFertilizerPumpOn=true;
  // Serial.println("Fertilizer pump turned on");
  // Alarm.timerOnce(15, turnOffFertilizerPump);    

}
void turnOffFertilizerPump(){

    digitalWrite(fertilizerPumpPin,LOW);
  // Serial.println("Fertilizer pump turned off");
  isFertilizerPumpOn=false;
  
}
void turnOnWaterPump(){

  digitalWrite(waterPumpPin,HIGH);
    isWaterPumpOn = true;
  // Serial.println("Water pump turned on");
  // Alarm.timerOnce(780, turnOffWaterPump);            // called once after 10 seconds
}
void turnOffWaterPump(){
  
  digitalWrite(waterPumpPin,LOW);
  isWaterPumpOn = false;
  // Serial.println("Water pump turned off");

}


// Function to read content from a file
String readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  Serial.println(fileContent);
  return fileContent;
}

// Function to write content to a file
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}

