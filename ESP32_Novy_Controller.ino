#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif
#include <RCSwitch.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "config.h"

// Button commands
enum ButtonCommand {
  CMD_LIGHT = 0,
  CMD_POWER = 1,
  CMD_PLUS = 2,
  CMD_MINUS = 3,
  CMD_NOVY = 4,
  CMD_BRIGHTDIM = 5,
  CMD_BRIGHTDIMLOW = 6
};

// HTML Webpage build in PROGMEM
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Novy Control Panel</title>
<style>
:root {
  --bg-primary: #000000;
  --bg-secondary: #1c1c1e;
  --bg-tertiary: #2c2c2e;
  --text-primary: #ffffff;
  --text-secondary: #8e8e93;
  --accent: #007aff;
  --accent-hover: #0056cc;
  --success: #30d158;
  --warning: #ff9500;
  --danger: #ff3b30;
  --border: #38383a;
  --shadow: rgba(0,0,0,0.3);
}
[data-theme='light'] {
  --bg-primary: #f2f2f7;
  --bg-secondary: #ffffff;
  --bg-tertiary: #f2f2f7;
  --text-primary: #000000;
  --text-secondary: #6d6d70;
  --accent: #007aff;
  --accent-hover: #0056cc;
  --success: #30d158;
  --warning: #ff9500;
  --danger: #ff3b30;
  --border: #d1d1d6;
  --shadow: rgba(0,0,0,0.1);
}
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: var(--bg-primary);
  color: var(--text-primary);
  height: 100vh;
  overflow: hidden;
  transition: all 0.3s ease;
}
.header {
  background: var(--bg-secondary);
  border-bottom: 1px solid var(--border);
  padding: 12px 20px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  backdrop-filter: blur(20px);
  position: relative;
}
.header-left {
  display: flex;
  align-items: center;
  gap: 15px;
}
.reset-btn {
  background: #ff3b30;
  color: white;
  border: none;
  padding: 8px 16px;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
  box-shadow: 0 2px 8px rgba(255, 59, 48, 0.3);
}
.reset-btn:hover {
  background: #d70015;
  transform: translateY(-1px);
  box-shadow: 0 4px 12px rgba(255, 59, 48, 0.4);
}
.reset-btn:active {
  transform: translateY(0);
  box-shadow: 0 2px 8px rgba(255, 59, 48, 0.3);
}
.title {
  font-size: 20px;
  font-weight: 700;
  color: var(--text-primary);
}
.header-right {
  display: flex;
  align-items: center;
  gap: 15px;
}
.status-badges {
  display: flex;
  gap: 10px;
}
.status {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px;
  background: rgba(48, 209, 88, 0.15);
  border: 1px solid var(--success);
  border-radius: 20px;
  font-size: 12px;
  font-weight: 600;
  color: var(--success);
}
.heap-status {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px;
  border-radius: 20px;
  font-size: 12px;
  font-weight: 600;
  border: 1px solid;
  transition: all 0.3s ease;
}
.status-dot {
  width: 8px;
  height: 8px;
  background: var(--success);
  border-radius: 50%;
  animation: pulse 2s infinite;
}
.heap-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  transition: all 0.3s ease;
}
@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}
.theme-toggle {
  position: relative;
  width: 50px;
  height: 28px;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 14px;
  cursor: pointer;
  transition: all 0.3s ease;
}
.theme-toggle::before {
  content: '';
  position: absolute;
  top: 2px;
  left: 2px;
  width: 22px;
  height: 22px;
  background: var(--text-primary);
  border-radius: 50%;
  transition: all 0.3s ease;
  box-shadow: 0 2px 4px var(--shadow);
}
[data-theme='light'] .theme-toggle::before {
  transform: translateX(22px);
}
.container {
  height: calc(100vh - 65px);
  display: flex;
  flex-direction: column;
  padding: 20px;
  gap: 20px;
}
.controls {
  background: var(--bg-secondary);
  border: 1px solid var(--border);
  border-radius: 16px;
  padding: 20px;
  backdrop-filter: blur(20px);
  box-shadow: 0 4px 20px var(--shadow);
}
.button-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 12px;
}
.btn {
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  color: var(--text-primary);
  padding: 16px 20px;
  border-radius: 12px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
  backdrop-filter: blur(10px);
  position: relative;
  overflow: hidden;
}
.btn:hover {
  background: var(--accent);
  border-color: var(--accent);
  color: white;
  transform: translateY(-2px);
  box-shadow: 0 8px 25px rgba(0, 122, 255, 0.3);
}
.btn:active {
  transform: translateY(0);
  box-shadow: 0 4px 15px rgba(0, 122, 255, 0.2);
}
.log-section {
  flex: 1;
  background: var(--bg-secondary);
  border: 1px solid var(--border);
  border-radius: 16px;
  padding: 20px;
  backdrop-filter: blur(20px);
  box-shadow: 0 4px 20px var(--shadow);
  display: flex;
  flex-direction: column;
}
.log-title {
  font-size: 18px;
  font-weight: 700;
  margin-bottom: 15px;
  color: var(--text-primary);
}
.log-container {
  flex: 1;
  background: var(--bg-tertiary);
  border: 1px solid var(--border);
  border-radius: 12px;
  padding: 15px;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', monospace;
  font-size: 13px;
  line-height: 1.4;
  color: var(--text-primary);
  overflow-y: auto;
  white-space: pre-wrap;
  word-wrap: break-word;
}
.log-container::-webkit-scrollbar {
  width: 8px;
}
.log-container::-webkit-scrollbar-track {
  background: var(--bg-secondary);
  border-radius: 4px;
}
.log-container::-webkit-scrollbar-thumb {
  background: var(--border);
  border-radius: 4px;
}
.log-container::-webkit-scrollbar-thumb:hover {
  background: var(--text-secondary);
}
@media (max-width: 1024px) {
  .button-grid { grid-template-columns: 1fr; }
  .container { padding: 15px; }
  .header { padding: 10px 15px; }
  .header-left, .header-right { gap: 10px; }
  .status-badges { flex-direction: column; gap: 5px; }
}
</style></head><body>

<div class='header'>
<div class='header-left'>
<button class='reset-btn' onclick='resetDevice()'>Reboot</button>
<div class='title'>Novy Control Panel</div>
</div>
<div class='header-right'>
<div class='status-badges'>
<div class='status'>
<div class='status-dot'></div>
Home Assistant Connected
</div>
<div class='heap-status' id='heapStatus'>
<div class='heap-dot' id='heapDot'></div>
<span id='heapText'>Heap: --</span>
</div>
<div class='heap-status' id='rssiStatus'>
<div class='heap-dot' id='rssiDot'></div>
<span id='rssiText'>WiFi: --</span>
</div>
</div>
<div class='theme-toggle' onclick='toggleTheme()'></div>
</div>
</div>

<div class='container'>
<div class='controls'>
<div class='button-grid'>
<button class='btn' onclick='sendCommand("toggleLight")'>Light</button>
<button class='btn' onclick='sendCommand("toggleBrightness")'>Brightness</button>
<button class='btn' onclick='sendCommand("toggleBrightnesslow")'>Brightness low</button>
<button class='btn' onclick='sendCommand("togglePower")'>Power</button>
<button class='btn' onclick='sendCommand("togglePlus")'>Plus</button>
<button class='btn' onclick='sendCommand("toggleMinus")'>Minus</button>
<button class='btn' onclick='sendCommand("toggleNovy")'>Novy</button>
</div>
</div>

<div class='log-section'>
<div class='log-title'>System Log</div>
<div class='log-container' id='logContainer'>
%LOGS%
</div>
</div>
</div>

<script>
function sendCommand(cmd) {
  fetch('/' + cmd, { method: 'GET' })
    .then(response => response.text())
    .then(data => console.log('Success:', data))
    .catch(error => console.error('Error:', error));
}

function resetDevice() {
  if (confirm('Are you sure you want to reset the device? This will reboot the ESP32.')) {
    fetch('/reset', { method: 'POST' })
      .then(() => {
        alert('Device is resetting... The page will reload in 10 seconds.');
        setTimeout(() => location.reload(), 10000);
      })
      .catch(error => console.error('Reset error:', error));
  }
}

function toggleTheme() {
  const body = document.body;
  const currentTheme = body.getAttribute('data-theme');
  const newTheme = currentTheme === 'light' ? 'dark' : 'light';
  body.setAttribute('data-theme', newTheme);
  localStorage.setItem('theme', newTheme);
}

function updateHeapStatus() {
  fetch('/heap')
    .then(response => response.json())
    .then(data => {
      const heapStatus = document.getElementById('heapStatus');
      const heapDot = document.getElementById('heapDot');
      const heapText = document.getElementById('heapText');
      
      const percentage = data.percentage;
      const color = data.color;
      
      heapText.textContent = `Heap: ${percentage}%`;
      heapDot.style.backgroundColor = color;
      heapStatus.style.borderColor = color;
      heapStatus.style.color = color;
      heapStatus.style.backgroundColor = color + '26'; // Add transparency
    })
    .catch(error => {
      console.error('Heap status error:', error);
      document.getElementById('heapText').textContent = 'Heap: Error';
    });
}

function updateRSSIStatus() {
  fetch('/rssi')
    .then(response => response.json())
    .then(data => {
      const rssiStatus = document.getElementById('rssiStatus');
      const rssiDot = document.getElementById('rssiDot');
      const rssiText = document.getElementById('rssiText');
      
      const rssi = data.rssi;
      const percentage = data.percentage;
      const color = data.color;
      const bars = data.bars;
      
      rssiText.textContent = `WiFi: ${rssi}dBm`;
      rssiDot.style.backgroundColor = color;
      rssiStatus.style.borderColor = color;
      rssiStatus.style.color = color;
      rssiStatus.style.backgroundColor = color + '26'; // Add transparency
    })
    .catch(error => {
      console.error('RSSI status error:', error);
      document.getElementById('rssiText').textContent = 'WiFi: Error';
    });
}

document.addEventListener('DOMContentLoaded', function() {
  const savedTheme = localStorage.getItem('theme') || 'dark';
  document.body.setAttribute('data-theme', savedTheme);
  const logContainer = document.getElementById('logContainer');
  logContainer.scrollTop = logContainer.scrollHeight;
  
  // Initial heap status update
  updateHeapStatus();
  updateRSSIStatus();
});

setInterval(() => {
  fetch('/log')
    .then(response => response.text())
    .then(data => {
      const logContainer = document.getElementById('logContainer');
      const wasAtBottom = logContainer.scrollTop + logContainer.clientHeight >= logContainer.scrollHeight - 5;
      logContainer.innerHTML = data;
      if (wasAtBottom) logContainer.scrollTop = logContainer.scrollHeight;
    });
}, 2000);

// Update heap and RSSI status every 5 seconds
setInterval(() => {
  updateHeapStatus();
  updateRSSIStatus();
}, 5000);
</script></body></html>
)rawliteral";

// HEAP setup
unsigned long lastHeapCheck = 0;
uint32_t initialFreeHeap = 0;
uint32_t currentFreeHeap = 0;
uint32_t minFreeHeap = UINT32_MAX;

// Function to get RSSI percentage (0-100%)
int getRSSIPercentage(int rssi) {
  if (rssi >= -50) return 100;
  if (rssi <= -100) return 0;
  return 2 * (rssi + 100);
}

// Function to get RSSI status color
String getRSSIStatusColor(int rssi) {
  if (rssi >= -50) return "#30d158"; // Green - Excellent
  if (rssi >= -60) return "#30d158"; // Green - Good  
  if (rssi >= -70) return "#ff9500"; // Orange - Fair
  return "#ff3b30"; // Red - Poor
}

// Function to get RSSI bars (1-4)
int getRSSIBars(int rssi) {
  if (rssi >= -50) return 4;
  if (rssi >= -60) return 3;
  if (rssi >= -70) return 2;
  return 1;
}

// Hardware setup
RCSwitch transmitter = RCSwitch();
#ifdef ESP32
WebServer server(80);
#elif defined(ESP8266)
ESP8266WebServer server(80);
#endif
WiFiClient espClient;
PubSubClient client(espClient);

// Logging system
const int LOG_BUFFER_SIZE = 10;
String logBuffer[LOG_BUFFER_SIZE];
int logIndex = 0;

// Home Assistant Discovery
bool haDiscoverySent = false;

// Button configuration
struct ButtonConfig {
  String name;
  String icon;
  ButtonCommand cmd;
};

ButtonConfig buttons[] = {
  {"Light", "mdi:lightbulb", CMD_LIGHT},
  {"Power", "mdi:power", CMD_POWER},
  {"Plus", "mdi:plus", CMD_PLUS},
  {"Minus", "mdi:minus", CMD_MINUS},
  {"Novy", "mdi:fan", CMD_NOVY},
  {"Brightness", "mdi:brightness-6", CMD_BRIGHTDIM},
  {"Brightness-low", "mdi:brightness-5", CMD_BRIGHTDIMLOW}
};

const int BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0]);

// Function declarations
void setupWiFi();
void setupOTA();
void setupWebServer();
void setupMQTT();
void connectToMQTT();
void sendHomeAssistantDiscovery();
void handleButtonPress(ButtonCommand cmd);
void sendRFCommand(ButtonCommand cmd, int channelIndex = 0);
void publishMQTTStatus(String button, String state = "ON");
void addToLogBuffer(String message);
String getLogContent();
String generateWebPage();
void mqttCallback(char* topic, byte* payload, unsigned int length);
String getButtonNameByCommand(ButtonCommand cmd);
String toLowerCase(String str);

// Helper function to convert string to lowercase
String toLowerCase(String str) {
  str.toLowerCase();
  return str;
}

void setup() {
  Serial.begin(115200);

  // Capture initial heap size
  initialFreeHeap = ESP.getFreeHeap();
  currentFreeHeap = initialFreeHeap;
  minFreeHeap = initialFreeHeap;

  // Initialize RF transmitter
  pinMode(POWER_433MHZ_PIN, OUTPUT);
  digitalWrite(POWER_433MHZ_PIN, HIGH);
  transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);
  transmitter.setPulseLength(350);
  transmitter.setProtocol(12);

  setupWiFi();
  setupOTA();
  setupMQTT();
  setupWebServer();

  server.begin();
  addToLogBuffer("System initialized successfully");
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    addToLogBuffer("WiFi connection failed!");
    return;
  }

  addToLogBuffer("IP Address: " + WiFi.localIP().toString());
}

void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME.c_str());
  ArduinoOTA.setPassword(OTAPASSWORD.c_str());

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update completed");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

// Monitor heap
void updateHeapInfo() {
  currentFreeHeap = ESP.getFreeHeap();
  
  // Track minimum heap (worst case)
  if (currentFreeHeap < minFreeHeap) {
    minFreeHeap = currentFreeHeap;
  }
  
  // Log heap info every 5 minutes
  if (millis() - lastHeapCheck > 300000) {
    lastHeapCheck = millis();
    addToLogBuffer("Heap - Current: " + String(currentFreeHeap) + 
                   " Min: " + String(minFreeHeap) + 
                   " Initial: " + String(initialFreeHeap));
  }
}

// Function to get heap percentage
int getHeapPercentage() {
  if (initialFreeHeap == 0) return 100;
  return (currentFreeHeap * 100) / initialFreeHeap;
}

// Function to get heap status color
String getHeapStatusColor() {
  int percentage = getHeapPercentage();
  if (percentage > 69) return "#30d158"; // Green
  if (percentage > 49) return "#ff9500"; // Orange
  return "#ff3b30"; // Red
}

void setupMQTT() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setBufferSize(1024); // Increased for HA discovery
  client.setCallback(mqttCallback);
}

void setupWebServer() {
  // Main page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", generateWebPage());
  });

  // API endpoints
  server.on("/log", HTTP_GET, []() {
    server.send(200, "text/plain", getLogContent());
  });

  // Heap status endpoint
  server.on("/heap", HTTP_GET, []() {
    DynamicJsonDocument doc(200);
    doc["current"] = currentFreeHeap;
    doc["initial"] = initialFreeHeap;
    doc["minimum"] = minFreeHeap;
    doc["percentage"] = getHeapPercentage();
    doc["color"] = getHeapStatusColor();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // RSSI status endpoint
  server.on("/rssi", HTTP_GET, []() {
    DynamicJsonDocument doc(200);
    int rssi = WiFi.RSSI();
    doc["rssi"] = rssi;
    doc["percentage"] = getRSSIPercentage(rssi);
    doc["color"] = getRSSIStatusColor(rssi);
    doc["bars"] = getRSSIBars(rssi);
  
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // Button endpoints
  server.on("/toggleLight", HTTP_GET, []() {
    server.send(200, "text/plain", "Light toggled");
    handleButtonPress(CMD_LIGHT);
  });

  server.on("/togglePower", HTTP_GET, []() {
    server.send(200, "text/plain", "Power toggled");
    handleButtonPress(CMD_POWER);
  });

  server.on("/togglePlus", HTTP_GET, []() {
    server.send(200, "text/plain", "Plus toggled");
    handleButtonPress(CMD_PLUS);
  });

  server.on("/toggleMinus", HTTP_GET, []() {
    server.send(200, "text/plain", "Minus toggled");
    handleButtonPress(CMD_MINUS);
  });

  server.on("/toggleNovy", HTTP_GET, []() {
    server.send(200, "text/plain", "Novy toggled");
    handleButtonPress(CMD_NOVY);
  });

  server.on("/toggleBrightness", HTTP_GET, []() {
    server.send(200, "text/plain", "Brightness toggled");
    handleButtonPress(CMD_BRIGHTDIM);
  });

  server.on("/toggleBrightnesslow", HTTP_GET, []() {
    server.send(200, "text/plain", "Brightness low toggled");
    handleButtonPress(CMD_BRIGHTDIMLOW);
  });

  server.on("/reset", HTTP_POST, []() {
    server.send(200, "text/plain", "Resetting device...");
    addToLogBuffer("Device reset requested via web interface");
    delay(1000);
    ESP.restart();
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
}

void sendHomeAssistantDiscovery() {
  if (!client.connected()) return;

  addToLogBuffer("Sending HA Discovery...");

  // Device information
  DynamicJsonDocument deviceDoc(512);
  deviceDoc["identifiers"][0] = HOSTNAME;
  deviceDoc["name"] = HOSTNAME + " Novy Controller";
  deviceDoc["model"] = "Novy RF Controller";
  deviceDoc["manufacturer"] = "renedis";
  deviceDoc["sw_version"] = "1.0";

  // Create buttons for each command
  for (int i = 0; i < BUTTON_COUNT; i++) {
    DynamicJsonDocument doc(1024);

    String buttonId = toLowerCase(buttons[i].name);
    String hostnameId = toLowerCase(HOSTNAME);
    String uniqueId = HOSTNAME + "_" + buttonId;
    String entityId = "button." + hostnameId + "_" + buttonId;

    // Button configuration
    doc["name"] = buttons[i].name;
    doc["unique_id"] = uniqueId;
    doc["device_class"] = "restart"; // Generic button class
    doc["icon"] = buttons[i].icon;

    // Topics
    doc["command_topic"] = MQTT_PREFIX + "/" + HOSTNAME + "/button/" + buttonId + "/set";
    doc["state_topic"] = MQTT_PREFIX + "/" + HOSTNAME + "/button/" + buttonId + "/state";
    doc["availability_topic"] = MQTT_PREFIX + "/" + HOSTNAME + "/status";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";

    // Device info
    doc["device"] = deviceDoc;

    // Publish discovery
    String discoveryTopic = "homeassistant/button/" + HOSTNAME + "/" + buttonId + "/config";
    String payload;
    serializeJson(doc, payload);

    client.publish(discoveryTopic.c_str(), payload.c_str(), true);
  }

  // Publish availability
  client.publish((MQTT_PREFIX + "/" + HOSTNAME + "/status").c_str(), "online", true);

  addToLogBuffer("HA Discovery sent");
  haDiscoverySent = true;
}

void handleButtonPress(ButtonCommand cmd) {
  sendRFCommand(cmd);

  // Publish MQTT status for Home Assistant
  String buttonName = getButtonNameByCommand(cmd);
  publishMQTTStatus(toLowerCase(buttonName), "PRESS");

  publishMQTTStatus(toLowerCase(buttonName), "");
}

String getButtonNameByCommand(ButtonCommand cmd) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (buttons[i].cmd == cmd) {
      return buttons[i].name;
    }
  }
  return "unknown";
}

void sendRFCommand(ButtonCommand cmd, int channelIndex) {
  pinMode(TRANSMIT_433MHZ_PIN, OUTPUT);
  transmitter.enableTransmit(TRANSMIT_433MHZ_PIN);
  transmitter.setPulseLength(350);
  transmitter.setProtocol(12);

  String command;
  int repeatCount = 3; // Default RF repeater

  switch(cmd) {
    case CMD_LIGHT:
      command = NOVY_COMMAND_LIGHT;
      repeatCount = 2; // Only send twice for LIGHT. 3 or more will cause brighten/dim
      break;
    case CMD_BRIGHTDIM:
      command = NOVY_COMMAND_LIGHT; // Light command is used for brightness
      repeatCount = 4; // Send four times for LIGHT brightness.
      break;
    case CMD_BRIGHTDIMLOW:
      command = NOVY_COMMAND_LIGHT; // Light command is used for brightness
      repeatCount = 10; // Send ten times for LIGHT brightness. Brightness: 1= 4 repeats. 2= 6 repeats. 3= 8 repeats. 4= 10 repeats.
      break;
    case CMD_POWER:
      command = NOVY_COMMAND_POWER;
      break;
    case CMD_PLUS:
      command = NOVY_COMMAND_PLUS;
      break;
    case CMD_MINUS:
      command = NOVY_COMMAND_MINUS;
      break;
    case CMD_NOVY:
      command = NOVY_COMMAND_NOVY;
      break;
  }

  String rfCode = NOVY_DEVICE_CODE[channelIndex] + NOVY_PREFIX + command;

  for(int i = 0; i < repeatCount; i++) {
    transmitter.send(rfCode.c_str());
    delay(50);
  }
  addToLogBuffer("RF sent: " + getButtonNameByCommand(cmd) + " (" + rfCode + "), repeats: " + String(repeatCount));
}

void publishMQTTStatus(String button, String state) {
  if (client.connected()) {
    String topic = MQTT_PREFIX + "/" + HOSTNAME + "/button/" + button + "/state";
    client.publish(topic.c_str(), state.c_str());
  }
}

void connectToMQTT() {
  int attempts = 0;
  while (!client.connected() && attempts < 5) {  // Limit attempts
    yield(); // Feed watchdog
    addToLogBuffer("Connecting to MQTT...");

    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, 
                      (MQTT_PREFIX + "/" + HOSTNAME + "/status").c_str(), 0, true, "offline")) {
      addToLogBuffer("Connected to MQTT");
  
      // Subscribe to command topics for Home Assistant
      for (int i = 0; i < BUTTON_COUNT; i++) {
        String buttonId = toLowerCase(buttons[i].name);
        String commandTopic = MQTT_PREFIX + "/" + HOSTNAME + "/button/" + buttonId + "/set";
        client.subscribe(commandTopic.c_str());
        yield(); // Feed watchdog between operations
      }
  
      // Send Home Assistant discovery after connection
      if (!haDiscoverySent) {
        delay(1000);
        sendHomeAssistantDiscovery();
      } else {
        client.publish((MQTT_PREFIX + "/" + HOSTNAME + "/status").c_str(), "online", true);
      }
  
    } else {
      addToLogBuffer("MQTT failed, rc=" + String(client.state()) + " Retrying...");
      attempts++;
      delay(2000); // Shorter delay
      yield(); // Feed watchdog
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Fix the payload handling bug - properly null-terminate the string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  String logMsg = "HA (MQTT) message sent [" + String(topic) + "] " + String(message);
  addToLogBuffer(logMsg);

  // Parse topic to determine command
  String topicStr = String(topic);
  String baseTopic = MQTT_PREFIX + "/" + HOSTNAME + "/button/";

  if (topicStr.startsWith(baseTopic) && topicStr.endsWith("/set")) {
    // Extract button name from topic
    String buttonName = topicStr.substring(baseTopic.length());
    buttonName = buttonName.substring(0, buttonName.length() - 4); // Remove "/set"

    // Find matching button and execute command
    for (int i = 0; i < BUTTON_COUNT; i++) {
      String buttonNameLower = toLowerCase(buttons[i].name);
      if (buttonNameLower == buttonName) {
        sendRFCommand(buttons[i].cmd);
        publishMQTTStatus(buttonName, "PRESS");
        publishMQTTStatus(buttonName, "");
        break;
      }
    }
  }
}

void addToLogBuffer(String message) {
  logBuffer[logIndex] = message;
  logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
}

String getLogContent() {
  String content = "";
  for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
    int index = (logIndex + i) % LOG_BUFFER_SIZE;
    if (logBuffer[index].length() > 0) {
      content += logBuffer[index] + "<br>";
    }
  }
  return content;
}

String generateWebPage() {
  String html = FPSTR(HTML_PAGE);
  html.replace("%LOGS%", getLogContent());
  return html;
}

void loop() {
  updateHeapInfo();

  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();
  ArduinoOTA.handle();
  server.handleClient();
}
