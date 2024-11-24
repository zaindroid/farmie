
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
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/bootstrap/5.3.0/css/bootstrap.min.css">
  <link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Gordita|Basier+Mono">
  <style>
    /* Styling for the new frontend */
    body { font-family: 'Gordita', sans-serif; margin: 0; background-color: #f5f7fa; }
    .topnav { overflow: hidden; background-color: #5dbb63; color: white; display: flex; justify-content: space-between; align-items: center; padding: 15px 20px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }
    .topnav h3 { margin: 0; flex-grow: 1; text-align: center; font-weight: bold; font-size: 1.6rem; font-family: 'Basier Mono', monospace; }
    .content { padding: 20px; }
    .cards { display: grid; grid-template-columns: repeat(2, 1fr); gap: 20px; justify-items: width:60% ; center; margin: 0 auto}
    .device-card { width: 200px; height: 200px; padding: 20px; background-color: #f4f4f4; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); display: flex; flex-direction: column; align-items: center; justify-content: space-between; border: 2px solid #5dbb63; }
    .device-card h5 { font-size: 1.4rem; font-weight: bold; color: #5dbb63; }
    .btn-primary { background-color: #5dbb63; border-color: #5dbb63; color: white; font-weight: bold; }
    .btn-primary:hover { background-color: #4a9e52; border-color: #4a9e52; }
  </style>
</head>
<body>
<div class="topnav">
  <h3>FARMIE DASHBOARD</h3>
  <span id="current-time" class="time-display">--:--:--</span>
</div>

<div id="dashboard" class="content">
  <div class="cards">
    <div class="device-card">
      <h5>Fans</h5>
      <button class="btn btn-primary" onclick="toggleDevice(this, 'fan')">Off</button>
    </div>
    <div class="device-card">
      <h5>Lights</h5>
      <button class="btn btn-primary" onclick="toggleDevice(this, 'light')">Off</button>
    </div>
    <div class="device-card">
      <h5>Water Pump</h5>
      <button class="btn btn-primary" onclick="toggleDevice(this, 'waterPump')">Off</button>
    </div>
    <div class="device-card">
      <h5>Fertilizer Pump</h5>
      <button class="btn btn-primary" onclick="toggleDevice(this, 'fertilizerPump')">Off</button>
    </div>
  </div>
  <div class="card">
    <h4>Temperature</h4>
    <p><span id="temp">--</span>&deg;C</p>
  </div>
  <div class="card">
    <h4>Humidity</h4>
    <p><span id="humi">--</span>&percnt;</p>
  </div>
</div>

<script>
  function toggleDevice(button, device) {
    const isOff = button.textContent === 'Off';
    button.classList.toggle('btn-success', isOff);
    button.classList.toggle('btn-primary', !isOff);
    button.textContent = isOff ? 'On' : 'Off';

    const xhr = new XMLHttpRequest();
    xhr.open("GET", `/toggle-${device}?state=${isOff ? 'on' : 'off'}`, true);
    xhr.send();
  }

  function updateSensorData(id, value) {
    document.getElementById(id).textContent = value;
  }

  const source = new EventSource('/events');
  source.addEventListener('temperature', function(e) {
    updateSensorData('temp', e.data);
  }, false);

  source.addEventListener('humidity', function(e) {
    updateSensorData('humi', e.data);
  }, false);

  source.addEventListener('time', function(e) {
    document.getElementById('current-time').textContent = e.data;
  }, false);
</script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/bootstrap/5.3.0/js/bootstrap.bundle.min.js"></script>
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
  Serial.println("An error has occurred while mounting LittleFS. Formatting...");
  LittleFS.format(); // Optional: Format LittleFS if necessary
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS after formatting.");
    return;
  }
}
  Serial.println("LittleFS mounted successfully");


    WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());


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

// // Toggle water pump
// server.on("/toggle-water", HTTP_GET, [] (AsyncWebServerRequest *request) {
//   if(!request->authenticate(http_username, http_password))
//     return request->requestAuthentication();
//   digitalWrite(waterPumpPin, !digitalRead(waterPumpPin)); // Toggle the water pump state
//   request->send(200, "text/plain", digitalRead(waterPumpPin) ? "ON" : "OFF");
// });

//  server.on("/adjust-fan", HTTP_GET, [] (AsyncWebServerRequest *request) {
//     if(!request->authenticate(http_username, http_password))
//       return request->requestAuthentication();
//     if (request->hasParam("speed")) {
//       int speed = request->getParam("speed")->value().toInt();
//       ledcWrite(fanPwmChannel, speed);
//       request->send(200, "text/plain", "Fan speed set to " + String(speed));
//     } else {
//       request->send(400, "text/plain", "Bad Request");
//     }
//   });


// // Route for adjusting light intensity
// server.on("/adjust-light", HTTP_GET, [] (AsyncWebServerRequest *request) {
//   if(!request->authenticate(http_username, http_password))
//     return request->requestAuthentication();
//   if (request->hasParam("intensity")) {
//     int intensity = request->getParam("intensity")->value().toInt();
//     // Map the intensity to the PWM range and write it
//     ledcWrite(lightPwmChannel, intensity);
//     request->send(200, "text/plain", "Light intensity set to " + String(intensity));
//   } else {
//     request->send(400, "text/plain", "Bad Request");
//   }
// });

  server.on("/toggle-fan", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      if (state == "on") turnOnFan();
      else turnOffFan();
      request->send(200, "text/plain", state == "on" ? "Fan ON" : "Fan OFF");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  // Endpoint for toggling light
  server.on("/toggle-light", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      if (state == "on") turnOnLight();
      else turnOffLight();
      request->send(200, "text/plain", state == "on" ? "Light ON" : "Light OFF");
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


server.on("/get-current-mode", HTTP_GET, [](AsyncWebServerRequest *request) {
  request->send(200, "text/plain", isAutoMode ? "auto" : "manual");
});


  // Endpoint for toggling water pump
  server.on("/toggle-waterPump", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      if (state == "on") turnOnWaterPump();
      else turnOffWaterPump();
      request->send(200, "text/plain", state == "on" ? "Water Pump ON" : "Water Pump OFF");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  // Endpoint for toggling fertilizer pump
  server.on("/toggle-fertilizerPump", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      if (state == "on") turnOnFertilizerPump();
      else turnOffFertilizerPump();
      request->send(200, "text/plain", state == "on" ? "Fertilizer Pump ON" : "Fertilizer Pump OFF");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
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
    // Serial.println("Processing schedule: " + schedule);

    int onHour, onMinute, offHour, offMinute, duration;
    static bool isFertilizerPumpActive = false;
    

    if (filePath.endsWith("light_schedule.txt") || filePath.endsWith("fan_schedule.txt")) {

      String device_name = "";

      if (filePath.endsWith("light_schedule.txt")) {
          device_name = "lights";
      } else if (filePath.endsWith("fan_schedule.txt")) {
          device_name = "fan";
      }


      sscanf(schedule.c_str(), "%d,%d,%d,%d", &onHour, &onMinute, &offHour, &offMinute);
      // Serial.printf("Parsed schedule for %s: On - %02d:%02d, Off - %02d:%02d\n", filePath.c_str(), onHour, onMinute, offHour, offMinute);
      // Serial.printf("Current time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

      // Check if current time is within the active period
      bool isActive = (now.hour() > onHour || (now.hour() == onHour && now.minute() >= onMinute)) &&
                      (now.hour() < offHour || (now.hour() == offHour && now.minute() < offMinute));

      if (isActive) {
        Serial.printf("Within active period: Turning on the %s.",device_name.c_str());
        turnOn();
      } else if (now.hour() == offHour && now.minute() == offMinute) {
        Serial.printf("Reached off time: Turning off the %s.",device_name.c_str());
        turnOff();
      } else {
        Serial.printf("Outside active period for  %s.",device_name.c_str());
      }
    } else if (filePath.startsWith("/waterPump")) {
      sscanf(schedule.c_str(), "%d,%d,%d", &onHour, &onMinute, &duration);
      // Serial.printf("Parsed schedule for %s: On - %02d:%02d, Duration - %d minutes\n", filePath.c_str(), onHour, onMinute, duration);
      // Serial.printf("Current time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());

      if (now.hour() == onHour && now.minute() == onMinute) {
        Serial.println("Within active period: Turning on the water pump.");
        turnOn();
        Alarm.timerOnce(duration * 60, turnOff); // Convert minutes to seconds here
      } else {
        Serial.println("Outside active period for water pump.");
      }
    } else if (filePath.endsWith("fertilizerPump_schedule.txt")) {
    int day;
    sscanf(schedule.c_str(), "%d,%d,%d,%d", &day, &onHour, &onMinute, &duration);
    // Serial.printf("Parsed schedule for fertilizer pump: Day - %d, On - %02d:%02d, Duration - %d seconds\n", day, onHour, onMinute, duration);
    // Serial.printf("Current time: Day - %d, Time - %02d:%02d:%02d\n", now.dayOfTheWeek(), now.hour(), now.minute(), now.second());

    if (now.dayOfTheWeek() == day && now.hour() == onHour && now.minute() == onMinute && !isFertilizerPumpActive) {
        Serial.println("Turning on the fertilizer pump.");
        turnOn();
        isFertilizerPumpActive = true; // Set the active flag
        Serial.printf("Setting timer to turn off after %d seconds.\n", duration);
        Alarm.timerOnce(duration, turnOffFertilizerPump); // Use the function pointer
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
   Serial.printf("Current time: Day - %d, Time - %02d:%02d:%02d\n", now.dayOfTheWeek(), now.hour(), now.minute(), now.second());

     if (!isAutoMode) {
        Serial.println("Manual mode active: Skipping schedule management.");
        return; // Skip processing schedules in Manual mode
    }

    processSchedules("/fan_schedule.txt", now, turnOnFan, turnOffFan);
    processSchedules("/light_schedule.txt", now, turnOnLight, turnOffLight);
    
    // Process all water pump schedules
    processSchedules("/waterPump1_schedule.txt", now, turnOnWaterPump, turnOffWaterPump);
    processSchedules("/waterPump2_schedule.txt", now, turnOnWaterPump, turnOffWaterPump);
    processSchedules("/waterPump3_schedule.txt", now, turnOnWaterPump, turnOffWaterPump);

    processSchedules("/fertilizerPump_schedule.txt", now, turnOnFertilizerPump, turnOffFertilizerPump);

    
  Serial.println("");
  Serial.println("=======================================================");
  Serial.println("");
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
  // Serial.printf("Reading file: %s\r\n", path);
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

